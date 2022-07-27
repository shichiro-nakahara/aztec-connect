import { EthAddress } from '@aztec/barretenberg/address';
import { toBigIntBE } from '@aztec/barretenberg/bigint_buffer';
import { Blockchain, TxType } from '@aztec/barretenberg/blockchain';
import { BridgeCallData, validateBridgeCallData } from '@aztec/barretenberg/bridge_call_data';
import {
  AccountVerifier,
  DefiDepositProofData,
  JoinSplitProofData,
  JoinSplitVerifier,
  ProofData,
  ProofId,
} from '@aztec/barretenberg/client_proofs';
import { Crs } from '@aztec/barretenberg/crs';
import { NoteAlgorithms } from '@aztec/barretenberg/note_algorithms';
import {
  OffchainAccountData,
  OffchainDefiDepositData,
  OffchainJoinSplitData,
} from '@aztec/barretenberg/offchain_tx_data';
import { TxId } from '@aztec/barretenberg/tx_id';
import { createLogger } from '@aztec/barretenberg/log';
import { BarretenbergWasm, BarretenbergWorker, createWorker, destroyWorker } from '@aztec/barretenberg/wasm';
import { Mutex } from 'async-mutex';
import { ProofGenerator } from 'halloumi/proof_generator';
import { BridgeResolver } from '../bridge';
import { TxDao } from '../entity';
import { getTxTypeFromProofData } from '../get_tx_type';
import { Metrics } from '../metrics';
import { RollupDb } from '../rollup_db';
import { TxFeeResolver } from '../tx_fee_resolver';
import { Tx } from './tx';
import { TxFeeAllocator } from './tx_fee_allocator';

export class TxReceiver {
  private worker!: BarretenbergWorker;
  private mutex = new Mutex();

  constructor(
    private barretenberg: BarretenbergWasm,
    private noteAlgo: NoteAlgorithms,
    private rollupDb: RollupDb,
    private blockchain: Blockchain,
    private proofGenerator: ProofGenerator,
    private joinSplitVerifier: JoinSplitVerifier,
    private accountVerifier: AccountVerifier,
    private txFeeResolver: TxFeeResolver,
    private metrics: Metrics,
    private bridgeResolver: BridgeResolver,
    private log = createLogger('TxReceiver'),
  ) {}

  public async init() {
    const crs = new Crs(0);
    await crs.downloadG2Data();

    this.worker = await createWorker('0', this.barretenberg.module);

    this.log('Requesting verification keys from ProofGenerator...');

    const jsKey = await this.proofGenerator.getJoinSplitVk();
    await this.joinSplitVerifier.loadKey(this.worker, jsKey, crs.getG2Data());

    const accountKey = await this.proofGenerator.getAccountVk();
    await this.accountVerifier.loadKey(this.worker, accountKey, crs.getG2Data());
  }

  public async destroy() {
    await destroyWorker(this.worker);
  }

  public setTxFeeResolver(txFeeResolver: TxFeeResolver) {
    this.txFeeResolver = txFeeResolver;
  }

  public async receiveTxs(txs: Tx[]) {
    // We mutex this entire receive call until we move to "deposit to proof hash". Read more below.
    await this.mutex.acquire();
    try {
      const txTypes: TxType[] = [];
      for (let i = 0; i < txs.length; ++i) {
        const { proof } = txs[i];
        const txType = await getTxTypeFromProofData(proof, this.blockchain);
        this.metrics.txReceived(txType);
        this.log(`Received tx (${i + 1}/${txs.length}): ${proof.txId.toString('hex')}, type: ${TxType[txType]}`);
        txTypes.push(txType);
      }

      const txFeeAllocator = new TxFeeAllocator(this.txFeeResolver);
      const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);
      this.log(
        `Gas Required/Provided: ${validation.gasRequired}/${validation.gasProvided}. Fee asset index: ${validation.feePayingAsset}. Feeless txs: ${validation.hasFeelessTxs}.`,
      );
      if (validation.gasProvided < validation.gasRequired) {
        this.log(
          `Txs only contained enough fee to pay for ${validation.gasProvided} gas, but it needed ${validation.gasRequired}.`,
        );
        throw new Error('Insufficient fee.');
      }

      await this.validateChain(txs);

      await this.validateRequiredDeposit(txs);

      const txDaos: TxDao[] = [];
      for (let i = 0; i < txs.length; ++i) {
        const prevTxTime = i == 0 ? undefined : txDaos[i - 1].created;
        const txDao = await this.validateTx(txs[i], txTypes[i], prevTxTime);
        txDaos.push(txDao);
      }

      txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);
      await this.rollupDb.addTxs(txDaos);

      return txDaos.map(txDao => txDao.id);
    } finally {
      this.mutex.release();
    }
  }

  private async validateTx({ proof, offchainTxData, depositSignature }: Tx, txType: TxType, previousTxTime?: Date) {
    this.validateAsset(proof);

    if (proof.proofId === ProofId.DEPOSIT) {
      await this.validateDepositProofApproval(proof, depositSignature);
    }

    // Check the proof is valid.
    switch (proof.proofId) {
      case ProofId.DEPOSIT:
      case ProofId.WITHDRAW:
      case ProofId.SEND:
        await this.validatePaymentProof(proof, offchainTxData);
        break;
      case ProofId.ACCOUNT: {
        await this.validateAccountProof(proof, offchainTxData);
        break;
      }
      case ProofId.DEFI_DEPOSIT:
        await this.validateDefiDepositProof(proof, offchainTxData);
        break;
      default:
        throw new Error('Unknown proof id.');
    }

    const dataRootsIndex = await this.rollupDb.getDataRootsIndex(proof.noteTreeRoot);

    // When txs are provided to us in a batch it's possible that they are chained
    // we need to ensure that each tx in the chain has a timestamp that is greater than the previous
    // It's improbable that they would be given the same time but this should ensure it doesn't happen
    const getNextTxTime = () => {
      const currentTime = new Date();
      if (!previousTxTime || currentTime.getTime() > previousTxTime.getTime()) {
        return currentTime;
      }
      return new Date(previousTxTime.getTime() + 1);
    };

    return new TxDao({
      id: proof.txId,
      proofData: proof.rawProofData,
      offchainTxData,
      signature: proof.proofId === ProofId.DEPOSIT ? depositSignature : undefined,
      nullifier1: toBigIntBE(proof.nullifier1) ? proof.nullifier1 : undefined,
      nullifier2: toBigIntBE(proof.nullifier2) ? proof.nullifier2 : undefined,
      dataRootsIndex,
      created: getNextTxTime(),
      txType,
      excessGas: 0, // provided later
    });
  }

  private async validatePaymentProof(proof: ProofData, offchainTxData: Buffer) {
    try {
      OffchainJoinSplitData.fromBuffer(offchainTxData);
    } catch (e) {
      throw new Error(`Invalid offchain data: ${e.message}`);
    }

    if (!(await this.joinSplitVerifier.verifyProof(proof.rawProofData))) {
      throw new Error('Payment proof verification failed.');
    }
  }

  private async validateAccountProof(proof: ProofData, offchainTxData: Buffer) {
    const { accountPublicKey, aliasHash, spendingPublicKey1, spendingPublicKey2 } =
      OffchainAccountData.fromBuffer(offchainTxData);
    const expectedCommitments = [proof.noteCommitment1, proof.noteCommitment2];
    [spendingPublicKey1, spendingPublicKey2].forEach((spendingKey, i) => {
      const commitment = this.noteAlgo.accountNoteCommitment(aliasHash, accountPublicKey, spendingKey);
      if (!commitment.equals(expectedCommitments[i])) {
        throw new Error('Invalid offchain account data.');
      }
    });

    if (!(await this.accountVerifier.verifyProof(proof.rawProofData))) {
      throw new Error('Account proof verification failed.');
    }

    // if the second nullifier is non-zero then this is attempting to register
    // a new account public key. check that the key does not already exist
    if (!proof.nullifier2.equals(Buffer.alloc(32)) && (await this.rollupDb.isAccountRegistered(accountPublicKey))) {
      throw new Error('Account key already registered');
    }
    // if the first nullifier is non-zero then this is attempting to register
    // a new alias. check that the alias does not already exist in this case
    if (!proof.nullifier1.equals(Buffer.alloc(32)) && (await this.rollupDb.isAliasRegistered(aliasHash))) {
      throw new Error('Alias already registered');
    }
  }

  private async validateDefiDepositProof(proofData: ProofData, offchainTxData: Buffer) {
    if (proofData.allowChainFromNote1 || proofData.allowChainFromNote2) {
      throw new Error('Cannot chain from a defi deposit tx.');
    }

    const { bridgeCallData, defiDepositValue, txFee } = new DefiDepositProofData(proofData);
    try {
      validateBridgeCallData(bridgeCallData);
    } catch (e) {
      throw new Error(`Invalid bridge call data - ${e.message}`);
    }

    const bridgeConfig = this.bridgeResolver.getBridgeConfig(bridgeCallData.toBigInt());
    const blockchainStatus = this.blockchain.getBlockchainStatus();
    if (!blockchainStatus.allowThirdPartyContracts && !bridgeConfig) {
      this.log(`Unrecognised Defi bridge: ${bridgeCallData.toString()}`);
      throw new Error('Unrecognised Defi-bridge.');
    }

    const offchainData = OffchainDefiDepositData.fromBuffer(offchainTxData);
    if (!bridgeCallData.equals(offchainData.bridgeCallData)) {
      throw new Error(
        `Wrong bridgeCallData in offchain data. Expect ${bridgeCallData}. Got ${offchainData.bridgeCallData}.`,
      );
    }
    if (defiDepositValue !== offchainData.depositValue) {
      throw new Error(
        `Wrong depositValue in offchain data. Expect ${defiDepositValue}. Got ${offchainData.depositValue}.`,
      );
    }
    if (txFee !== offchainData.txFee) {
      throw new Error(`Wrong txFee in offchain data. Expect ${txFee}. Got ${offchainData.txFee}.`);
    }
    // TODO - check partialState

    if (!(await this.joinSplitVerifier.verifyProof(proofData.rawProofData))) {
      throw new Error('Defi-deposit proof verification failed.');
    }
  }

  private async validateChain(txs: Tx[]) {
    const unsettledTxs = (await this.rollupDb.getUnsettledTxs()).map(({ proofData }) => new ProofData(proofData));
    for (let i = 0; i < txs.length; ++i) {
      const { backwardLink } = txs[i].proof;
      if (!backwardLink.equals(Buffer.alloc(32))) {
        const precedingTxs = [...unsettledTxs, ...txs.slice(0, i).map(p => p.proof)];
        if (precedingTxs.some(tx => tx.backwardLink.equals(backwardLink))) {
          throw new Error('Duplicated backward link.');
        }

        const linkedTx = precedingTxs.some(
          tx =>
            (tx.allowChainFromNote1 && tx.noteCommitment1.equals(backwardLink)) ||
            (tx.allowChainFromNote2 && tx.noteCommitment2.equals(backwardLink)),
        );
        if (!linkedTx) {
          throw new Error('Linked tx not found.');
        }
      }

      const { nullifier1, nullifier2 } = txs[i].proof;
      if (
        (await this.rollupDb.nullifiersExist(nullifier1, nullifier2)) ||
        txs
          .slice(0, i)
          .some(p =>
            [p.proof.nullifier1, p.proof.nullifier2]
              .filter(n => !!toBigIntBE(n))
              .some(n => n.equals(nullifier1) || n.equals(nullifier2)),
          )
      ) {
        throw new Error('Nullifier already exists.');
      }
    }
  }

  private validateAsset(proof: ProofData) {
    const validateNonVirtualAssetId = (assetId: number) => {
      const { assets } = this.blockchain.getBlockchainStatus();
      if (assetId >= assets.length) {
        throw new Error(`Unsupported asset ${assetId}.`);
      }
    };

    switch (proof.proofId) {
      case ProofId.DEPOSIT:
      case ProofId.WITHDRAW: {
        const assetId = proof.publicAssetId.readUInt32BE(28);
        validateNonVirtualAssetId(assetId);
        break;
      }
      case ProofId.DEFI_DEPOSIT: {
        const bridgeCallData = BridgeCallData.fromBuffer(proof.bridgeCallData);
        if (!bridgeCallData.firstOutputVirtual) {
          validateNonVirtualAssetId(bridgeCallData.outputAssetIdA);
        }
        if (bridgeCallData.secondOutputInUse && !bridgeCallData.secondOutputVirtual) {
          validateNonVirtualAssetId(bridgeCallData.outputAssetIdB!);
        }
        break;
      }
      default:
    }
  }

  private async validateDepositProofApproval(proof: ProofData, depositSignature?: Buffer) {
    const { publicOwner } = new JoinSplitProofData(proof);
    const txId = new TxId(proof.txId);
    let proofApproval = await this.blockchain.getUserProofApprovalStatus(publicOwner, txId.toBuffer());
    if (!proofApproval && depositSignature) {
      const message = txId.toDepositSigningData();
      const lastByte = depositSignature[depositSignature.length - 1];
      if (lastByte == 0 || lastByte == 1) {
        depositSignature[depositSignature.length - 1] = lastByte + 27;
      }
      proofApproval = this.blockchain.validateSignature(publicOwner, depositSignature, message);
    }
    if (!proofApproval) {
      throw new Error(`Tx not approved or invalid signature: ${txId.toString()}`);
    }
  }

  private async validateRequiredDeposit(txs: Tx[]) {
    const depositProofs = txs
      .filter(tx => tx.proof.proofId === ProofId.DEPOSIT)
      .map(tx => new JoinSplitProofData(tx.proof));
    if (!depositProofs.length) {
      return;
    }

    const requiredDeposits: { publicOwner: EthAddress; publicAssetId: number; value: bigint }[] = [];
    depositProofs.forEach(({ publicAssetId, publicOwner, publicValue }) => {
      const index = requiredDeposits.findIndex(
        d => d.publicOwner.equals(publicOwner) && d.publicAssetId === publicAssetId,
      );
      if (index < 0) {
        requiredDeposits.push({ publicOwner, publicAssetId, value: publicValue });
      } else {
        requiredDeposits[index] = { publicOwner, publicAssetId, value: requiredDeposits[index].value + publicValue };
      }
    });

    const unsettledTxs = (await this.rollupDb.getUnsettledDepositTxs()).map(tx =>
      JoinSplitProofData.fromBuffer(tx.proofData),
    );

    for (const { publicOwner, publicAssetId, value } of requiredDeposits) {
      // WARNING! Need to check the sum of all deposits in txs remains <= the amount pending deposit on contract.
      // As the db read of existing txs, and insertion of new tx, needs to be atomic, we have to mutex receiveTx.
      const total =
        value +
        unsettledTxs
          .filter(tx => tx.publicOwner.equals(publicOwner) && tx.publicAssetId === publicAssetId)
          .reduce((acc, tx) => acc + tx.publicValue, 0n);
      const pendingDeposit = await this.blockchain.getUserPendingDeposit(publicAssetId, publicOwner);
      if (pendingDeposit < total) {
        throw new Error('User insufficient pending deposit balance.');
      }
    }
  }
}
