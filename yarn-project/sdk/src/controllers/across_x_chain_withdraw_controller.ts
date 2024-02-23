import { EthAddress, GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { TxId } from '@polyaztec/barretenberg/tx_id';
import { CoreSdk } from '../core_sdk/index.js';
import { ProofOutput, proofOutputToProofTx } from '../proofs/index.js';
import { Signer } from '../signer/index.js';
import { createTxRefNo } from './create_tx_ref_no.js';
import { ClientEthereumBlockchain } from '@polyaztec/blockchain';
import config from '../config.js';
import { Timer } from '@polyaztec/barretenberg/timer';
import { sleep } from '@polyaztec/barretenberg/sleep';

export class AcrossXChainWithdrawController {
  private readonly requireFeePayingTx: boolean;
  private proofOutputs: ProofOutput[] = [];
  private feeProofOutputs: ProofOutput[] = [];
  private txIds: TxId[] = [];
  private withdrawId: number | undefined = undefined;
  private withdrawAmount: bigint | undefined = undefined;

  constructor(
    public readonly userId: GrumpkinAddress,
    private readonly userSigner: Signer,
    public readonly assetValue: AssetValue,
    public readonly fee: AssetValue,
    public readonly recipient: EthAddress,
    public readonly destinationChainId: number,
    private readonly core: CoreSdk,
    private readonly blockchain: ClientEthereumBlockchain,
  ) {
    if (!assetValue.value) {
      throw new Error('Value must be greater than 0.');
    }

    this.requireFeePayingTx = !!fee.value && fee.assetId !== assetValue.assetId;
  }

  public async initAcrossWithdraw() {
    const remoteStatus = await this.core.getRemoteStatus();
    const result = await(
      await fetch(
        `${config.nataGateway.keeper}/across-x-chain-withdraw`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            recipient: this.recipient.toString(),
            originToken: remoteStatus.blockchainStatus.assets[this.assetValue.assetId].address.toString(),
            amount: this.assetValue.value.toString(),
            destinationChainId: this.destinationChainId,
            assetId: this.assetValue.assetId,
          })
        }
      )
    ).json();

    const { id, txHash, withdrawAmount } = result.data;

    this.withdrawId = id;
    this.withdrawAmount = BigInt(withdrawAmount);

    let withdraw;
    const timer = new Timer();
    while (true) {
      withdraw = await this.blockchain.getAcrossXChainWithdrawal(id);
      if (withdraw) break;

      await sleep(1000);

      if (timer.s() > 300) {
        throw new Error(`Could not verify createWithdraw transaction status after 300s`);
      }
    }

    if (
      withdraw.destinationChainId != this.destinationChainId || 
      withdraw.assetId.toNumber() != this.assetValue.assetId || 
      withdraw.rpWithdrawAmount.toBigInt() != this.assetValue.value ||
      withdraw.recipient.toString() != this.recipient.toString()
    ) {
      throw new Error('Invalid withdraw id');
    }

    return { id, txHash, withdrawAmount };
  }

  public async createProof(timeout?: number) {
    if (!this.withdrawId || !this.withdrawAmount) {
      throw new Error('Call initWithdraw() first.');
    }

    const { assetId } = this.assetValue;
    const privateInput = this.withdrawAmount + (!this.requireFeePayingTx ? this.fee.value : BigInt(0));
    const spendingPublicKey = this.userSigner.getPublicKey();
    const spendingKeyRequired = !spendingPublicKey.equals(this.userId);

    const proofInputs = await this.core.createPaymentProofInputs(
      this.userId,
      assetId,
      BigInt(0),
      this.withdrawAmount,
      privateInput,
      BigInt(0),
      BigInt(0),
      this.userId,
      spendingKeyRequired,
      EthAddress.fromString(config.nataGateway.address),
      spendingPublicKey,
      2,
    );

    const txRefNo = this.requireFeePayingTx || proofInputs.length > 1 ? createTxRefNo() : 0;

    if (this.requireFeePayingTx) {
      const feeProofInputs = await this.core.createPaymentProofInputs(
        this.userId,
        this.fee.assetId,
        BigInt(0),
        BigInt(0),
        this.fee.value,
        BigInt(0),
        BigInt(0),
        this.userId,
        spendingKeyRequired,
        undefined,
        spendingPublicKey,
        2,
      );
      this.feeProofOutputs = [];
      for (const proofInput of feeProofInputs) {
        proofInput.signature = await this.userSigner.signMessage(proofInput.signingData);
        this.feeProofOutputs.push(await this.core.createPaymentProof(proofInput, txRefNo, timeout));
      }
    }

    {
      const proofOutputs: ProofOutput[] = [];
      for (const proofInput of proofInputs) {
        proofInput.signature = await this.userSigner.signMessage(proofInput.signingData);
        proofOutputs.push(await this.core.createPaymentProof(proofInput, txRefNo, timeout));
      }
      this.proofOutputs = proofOutputs;
    }
  }

  public exportProofTxs() {
    if (!this.proofOutputs.length) {
      throw new Error('Call createProof() first.');
    }

    return [...this.proofOutputs, ...this.feeProofOutputs].map(proofOutputToProofTx);
  }

  public async send() {
    if (!this.proofOutputs.length) {
      throw new Error('Call createProof() first.');
    }

    this.txIds = await this.core.sendProofs([...this.proofOutputs, ...this.feeProofOutputs]);
    return this.txIds[this.proofOutputs.length - 1];
  }

  public getTxIds() {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutputs.length ? 'createProof()' : 'send()'} first.`);
    }

    return this.txIds;
  }

  public async awaitSettlement(timeout?: number) {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutputs.length ? 'createProof()' : 'send()'} first.`);
    }

    await Promise.all(this.txIds.map(txId => this.core.awaitSettlement(txId, timeout)));
  }
}
