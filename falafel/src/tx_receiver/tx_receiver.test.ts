import { AliasHash } from '@aztec/barretenberg/account_id';
import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { toBufferBE } from '@aztec/barretenberg/bigint_buffer';
import { Blockchain } from '@aztec/barretenberg/blockchain';
import { BridgeCallData } from '@aztec/barretenberg/bridge_call_data';
import { AccountVerifier, JoinSplitVerifier, ProofData, ProofId } from '@aztec/barretenberg/client_proofs';
import { randomBytes } from '@aztec/barretenberg/crypto';
import { NoteAlgorithms } from '@aztec/barretenberg/note_algorithms';
import {
  OffchainAccountData,
  OffchainDefiDepositData,
  OffchainJoinSplitData,
} from '@aztec/barretenberg/offchain_tx_data';
import { numToUInt32BE } from '@aztec/barretenberg/serialize';
import { ViewingKey } from '@aztec/barretenberg/viewing_key';
import { BridgeResolver } from '../bridge';
import { RollupDb } from '../rollup_db';
import { TxFeeResolver } from '../tx_fee_resolver';
import { TxReceiver } from './tx_receiver';

type Mockify<T> = {
  [P in keyof T]: jest.Mock;
};

describe('tx receiver', () => {
  let txReceiver: TxReceiver;
  let noteAlgo: Mockify<NoteAlgorithms>;
  let rollupDb: Mockify<RollupDb>;
  let blockchain: Mockify<Blockchain>;
  let joinSplitVerifier: Mockify<JoinSplitVerifier>;
  let accountVerifier: Mockify<AccountVerifier>;
  let txFeeResolver: Mockify<TxFeeResolver>;
  let bridgeResolver: Mockify<BridgeResolver>;
  const maxAssetId = 2;
  const assets = Array(maxAssetId + 1).fill(0);
  const feePayingAssets = [0, 1];
  const nonFeePayingAssetId = 2;

  const mockTx = ({
    proofId = ProofId.SEND,
    noteCommitment1 = randomBytes(32),
    noteCommitment2 = randomBytes(32),
    nullifier1 = randomBytes(32),
    nullifier2 = randomBytes(32),
    publicValue = 0n,
    publicOwner = EthAddress.ZERO,
    publicAssetId = 0,
    txFee = 1n,
    txFeeAssetId = 0,
    bridgeCallData = BridgeCallData.ZERO,
    defiDepositValue = 0n,
    backwardLink = Buffer.alloc(32),
    allowChain = 0,
    offchainTxData = new OffchainJoinSplitData([ViewingKey.random(), ViewingKey.random()]).toBuffer(),
  } = {}) => ({
    proof: new ProofData(
      Buffer.concat([
        numToUInt32BE(proofId, 32),
        noteCommitment1,
        noteCommitment2,
        nullifier1,
        nullifier2,
        toBufferBE(publicValue, 32),
        publicOwner.toBuffer32(),
        numToUInt32BE(publicAssetId, 32),
        randomBytes(32), // noteTreeRoot
        toBufferBE(txFee, 32),
        numToUInt32BE(txFeeAssetId, 32),
        bridgeCallData.toBuffer(),
        toBufferBE(defiDepositValue, 32),
        randomBytes(32), // defiRoot
        backwardLink,
        numToUInt32BE(allowChain, 32),
      ]),
    ),
    offchainTxData,
    depositSignature: proofId === ProofId.DEPOSIT ? randomBytes(32) : undefined,
  });

  const mockAccountTx = (create = true, migrate = false) => {
    const accountPublicKey = GrumpkinAddress.random();
    const aliasHash = AliasHash.random();
    const spendingPublicKey1 = randomBytes(32);
    const spendingPublicKey2 = randomBytes(32);
    const offchainTxData = new OffchainAccountData(accountPublicKey, aliasHash, spendingPublicKey1, spendingPublicKey2);
    return mockTx({
      proofId: ProofId.ACCOUNT,
      offchainTxData: offchainTxData.toBuffer(),
      nullifier1: create ? randomBytes(32) : Buffer.alloc(32),
      nullifier2: create || migrate ? randomBytes(32) : Buffer.alloc(32),
    });
  };

  const mockDefiDepositTx = ({
    bridgeCallData = new BridgeCallData(0, 0, 1),
    defiDepositValue = 1n,
    txFee = 1n,
    allowChain = 0,
  } = {}) => {
    const partialState = randomBytes(32);
    const partialStateSecretEphPubKey = GrumpkinAddress.random();
    const viewingKey = ViewingKey.random();
    const offchainTxData = new OffchainDefiDepositData(
      bridgeCallData,
      partialState,
      partialStateSecretEphPubKey,
      defiDepositValue,
      txFee,
      viewingKey,
    );
    return mockTx({
      proofId: ProofId.DEFI_DEPOSIT,
      offchainTxData: offchainTxData.toBuffer(),
      bridgeCallData,
      defiDepositValue,
      txFee,
      allowChain,
    });
  };

  beforeEach(() => {
    const barretenberg = {} as any;

    noteAlgo = {
      accountNoteCommitment: jest.fn().mockReturnValue(randomBytes(32)),
    } as any;

    rollupDb = {
      nullifiersExist: jest.fn().mockResolvedValue(false),
      getUnsettledDepositTxs: jest.fn().mockResolvedValue([]),
      getUnsettledTxs: jest.fn().mockResolvedValue([]),
      getDataRootsIndex: jest.fn().mockResolvedValue(0),
      addTxs: jest.fn(),
      isAccountRegistered: jest.fn().mockResolvedValue(false),
      isAliasRegistered: jest.fn().mockResolvedValue(false),
    } as any;

    blockchain = {
      getBlockchainStatus: jest.fn().mockReturnValue({ assets, allowThirdPartyContracts: false }),
      isContract: jest.fn().mockResolvedValue(false),
      isEmpty: jest.fn().mockResolvedValue(false),
      getUserPendingDeposit: jest.fn().mockResolvedValue(10n),
      getUserProofApprovalStatus: jest.fn().mockResolvedValue(false),
      validateSignature: jest.fn().mockResolvedValue(true),
    } as any;

    const proofGenerator = {} as any;

    joinSplitVerifier = {
      verifyProof: jest.fn().mockResolvedValue(true),
    } as any;

    accountVerifier = {
      verifyProof: jest.fn().mockResolvedValue(true),
    } as any;

    txFeeResolver = {
      isFeePayingAsset: jest.fn().mockImplementation((assetId: number) => feePayingAssets.includes(assetId)),
      getAdjustedBridgeTxGas: jest.fn().mockReturnValue(0),
      getAdjustedTxGas: jest.fn().mockReturnValue(0),
      getGasPaidForByFee: jest.fn().mockReturnValue(0),
      getTxFeeFromGas: jest.fn().mockReturnValue(0),
    } as any;

    const metrics = {
      txReceived: jest.fn(),
    } as any;

    bridgeResolver = {
      getBridgeConfig: jest.fn().mockReturnValue({}),
    } as any;

    txReceiver = new TxReceiver(
      barretenberg,
      noteAlgo as any,
      rollupDb,
      blockchain,
      proofGenerator,
      joinSplitVerifier as any,
      accountVerifier as any,
      txFeeResolver as any,
      metrics,
      bridgeResolver as any,
      () => {},
    );
  });

  describe('deposit tx', () => {
    it('accept a deposit tx', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicValue: 1n })];

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a deposit tx paid with non-fee-paying asset', async () => {
      const txs = [
        mockTx({ proofId: ProofId.DEPOSIT, publicAssetId: nonFeePayingAssetId, txFeeAssetId: nonFeePayingAssetId }),
      ];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow(
        'Transactions must have exactly 1 fee paying asset.',
      );
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a deposit tx with unregistered asset', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicAssetId: maxAssetId + 1 })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Unsupported asset');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a deposit tx without enough pending funds', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicValue: 2n })];
      blockchain.getUserPendingDeposit.mockResolvedValue(1n);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('User insufficient pending deposit balance.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a deposit tx whose pending funds are spent by other unsettled txs', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicValue: 5n })];
      blockchain.getUserPendingDeposit.mockResolvedValue(10n);
      rollupDb.getUnsettledDepositTxs.mockResolvedValue([
        { proofData: mockTx({ proofId: ProofId.DEPOSIT, publicValue: 7n }).proof.rawProofData },
      ]);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('User insufficient pending deposit balance.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('accept a deposit tx whose pending funds are not all spent by other unsettled txs', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicValue: 3n })];
      blockchain.getUserPendingDeposit.mockResolvedValue(10n);
      rollupDb.getUnsettledDepositTxs.mockResolvedValue([
        { proofData: mockTx({ proofId: ProofId.DEPOSIT, publicValue: 7n }).proof.rawProofData },
      ]);

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a deposit tx without a signature', async () => {
      const tx = mockTx({ proofId: ProofId.DEPOSIT, publicValue: 1n });
      const txs = [{ proof: tx.proof, offchainTxData: tx.offchainTxData }];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Tx not approved or invalid signature');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('accept a deposit tx without a signature but has approved', async () => {
      const tx = mockTx({ proofId: ProofId.DEPOSIT, publicValue: 1n });
      const txs = [{ proof: tx.proof, offchainTxData: tx.offchainTxData }];
      blockchain.getUserProofApprovalStatus.mockResolvedValue(true);

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a deposit tx with an invalid signatrue', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, publicValue: 1n })];
      blockchain.validateSignature.mockReturnValue(false);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Tx not approved or invalid signature');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('withdraw tx', () => {
    it('accept a withdraw tx', async () => {
      const txs = [mockTx({ proofId: ProofId.WITHDRAW, publicValue: 1n })];

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a withdraw tx with unregistered asset', async () => {
      const txs = [mockTx({ proofId: ProofId.WITHDRAW, publicAssetId: maxAssetId + 1 })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Unsupported asset');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('send tx', () => {
    it('accept a send tx', async () => {
      const txs = [mockTx({ proofId: ProofId.SEND })];

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a send tx paid with non-fee-paying asset', async () => {
      const txs = [mockTx({ proofId: ProofId.SEND, txFeeAssetId: nonFeePayingAssetId })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow(
        'Transactions must have exactly 1 fee paying asset.',
      );
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('payment tx', () => {
    it('reject a payment tx with invalid proof', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT })];
      joinSplitVerifier.verifyProof.mockResolvedValue(false);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Payment proof verification failed.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a payment tx with invalid offchain data buffer size', async () => {
      const txs = [mockTx({ proofId: ProofId.DEPOSIT, offchainTxData: randomBytes(100) })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Invalid offchain data');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('account tx', () => {
    it('accept an account tx', async () => {
      const txs = [mockAccountTx()];
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(txs[0].proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(txs[0].proof.noteCommitment2);

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject an account tx with invalid proof', async () => {
      const txs = [mockAccountTx()];
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(txs[0].proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(txs[0].proof.noteCommitment2);
      accountVerifier.verifyProof.mockResolvedValue(false);

      await expect(txReceiver.receiveTxs(txs)).rejects.toThrow('Account proof verification failed.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject an account tx with invalid offchain data', async () => {
      const txs = [mockAccountTx()];
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(txs[0].proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(randomBytes(32));

      await expect(txReceiver.receiveTxs(txs)).rejects.toThrow('Invalid offchain account data.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);

      noteAlgo.accountNoteCommitment.mockReset();

      noteAlgo.accountNoteCommitment.mockReturnValue(randomBytes(32));

      await expect(txReceiver.receiveTxs(txs)).rejects.toThrow('Invalid offchain account data.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('rejects an account tx that attempts to re-register an account public key', async () => {
      const accountTx = mockAccountTx();
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment2);
      rollupDb.isAccountRegistered.mockResolvedValueOnce(true);
      await expect(txReceiver.receiveTxs([accountTx])).rejects.toThrow('Account key already registered');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('accepts an account tx that attempts to neither create or migrate an account', async () => {
      const accountTx = mockAccountTx(false, false);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment2);
      // ensure the call to isAccountRegistered returns true
      rollupDb.isAccountRegistered.mockResolvedValueOnce(true);
      await expect(txReceiver.receiveTxs([accountTx])).resolves.toEqual([accountTx.proof.txId]);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: accountTx.proof.txId })]);
    });

    it('rejects an account tx that attempts to create a previously created alias', async () => {
      const accountTx = mockAccountTx();
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment2);
      // ensure the call to isAliasRegistered returns true
      rollupDb.isAliasRegistered.mockResolvedValueOnce(true);
      await expect(txReceiver.receiveTxs([accountTx])).rejects.toThrow('Alias already registered');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('accepts an account tx that attempts to migrate a previously created alias', async () => {
      // create an account migration tx
      const accountTx = mockAccountTx(false);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment1);
      noteAlgo.accountNoteCommitment.mockReturnValueOnce(accountTx.proof.noteCommitment2);
      // ensure the call to isAliasRegistered returns true
      rollupDb.isAliasRegistered.mockResolvedValueOnce(true);
      await expect(txReceiver.receiveTxs([accountTx])).resolves.toEqual([accountTx.proof.txId]);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: accountTx.proof.txId })]);
    });
  });

  describe('defi deposit tx', () => {
    it('accept a defi deposit tx', async () => {
      const txs = [mockDefiDepositTx()];

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a defi deposit tx with invalid proof', async () => {
      const txs = [mockDefiDepositTx()];
      joinSplitVerifier.verifyProof.mockResolvedValue(false);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Defi-deposit proof verification failed.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with allow chain note 1', async () => {
      const txs = [mockDefiDepositTx({ allowChain: 1 })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Cannot chain from a defi deposit tx.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with allow chain note 2', async () => {
      const txs = [mockDefiDepositTx({ allowChain: 2 })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Cannot chain from a defi deposit tx.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with allow chain both notes', async () => {
      const txs = [mockDefiDepositTx({ allowChain: 3 })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Cannot chain from a defi deposit tx.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with identical input assets', async () => {
      const bridgeCallData = new BridgeCallData(0, 1, 2, 1);
      const txs = [mockDefiDepositTx({ bridgeCallData })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Invalid bridge call data');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with identical output assets', async () => {
      const bridgeCallData = new BridgeCallData(0, 1, 2, 3, 2);
      const txs = [mockDefiDepositTx({ bridgeCallData })];

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Invalid bridge call data');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a defi deposit tx with unrecognised bridge', async () => {
      const txs = [mockDefiDepositTx()];
      bridgeResolver.getBridgeConfig.mockReturnValue(undefined);

      await expect(() => txReceiver.receiveTxs(txs)).rejects.toThrow('Unrecognised Defi-bridge.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('accept a defi deposit tx with unrecognised bridge if third party contracts are allowed', async () => {
      const txs = [mockDefiDepositTx()];
      bridgeResolver.getBridgeConfig.mockReturnValue(undefined);
      blockchain.getBlockchainStatus.mockReturnValue({ assets, allowThirdPartyContracts: true });

      await txReceiver.receiveTxs(txs);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: txs[0].proof.txId })]);
    });

    it('reject a defi deposit tx with inconsistent offchain data', async () => {
      const partialState = randomBytes(32);
      const partialStateSecretEphPubKey = GrumpkinAddress.random();
      const viewingKey = ViewingKey.random();
      const bridgeCallData = new BridgeCallData(0, 1, 2);
      const defiDepositValue = 1n;
      const txFee = 2n;
      const offchainTxData = new OffchainDefiDepositData(
        bridgeCallData,
        partialState,
        partialStateSecretEphPubKey,
        defiDepositValue,
        txFee,
        viewingKey,
      );

      {
        const tx = mockTx({
          proofId: ProofId.DEFI_DEPOSIT,
          offchainTxData: offchainTxData.toBuffer(),
          bridgeCallData: new BridgeCallData(0, 1, 0), // <--
          defiDepositValue,
          txFee,
        });
        await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('offchain data');
      }

      {
        const tx = mockTx({
          proofId: ProofId.DEFI_DEPOSIT,
          offchainTxData: offchainTxData.toBuffer(),
          bridgeCallData,
          defiDepositValue: defiDepositValue + 1n, // <--
          txFee,
        });
        await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('offchain data');
      }

      {
        const tx = mockTx({
          proofId: ProofId.DEFI_DEPOSIT,
          offchainTxData: offchainTxData.toBuffer(),
          bridgeCallData,
          defiDepositValue,
          txFee: txFee + 1n, // <--
        });
        await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('offchain data');
      }

      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('chained txs', () => {
    it('accept chained txs', async () => {
      const tx0 = mockTx({ allowChain: 1 });
      const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment1 });

      await txReceiver.receiveTxs([tx0, tx1]);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([
        expect.objectContaining({ id: tx0.proof.txId }),
        expect.objectContaining({ id: tx1.proof.txId }),
      ]);
    });

    it('chained txs never have the same date', async () => {
      // setup a mock new Date() so that the same date is always returned
      // but if a specific date is constructed via an argument then return this
      const mockDate = new Date(1466424490000);
      const realDate = global.Date;
      const spy = jest.spyOn(global, 'Date').mockImplementation((...args): any => {
        // if an argument is given construct a date as normal, otherwise return our mock date
        if (args.length) {
          return new realDate(...args);
        }
        return mockDate;
      });

      const tx0 = mockTx({ allowChain: 1 });
      const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment1, allowChain: 2 });
      const tx2 = mockTx({ backwardLink: tx1.proof.noteCommitment2 });

      // the mock date constructor above will give all txs the same time but this should be correct for
      await txReceiver.receiveTxs([tx0, tx1, tx2]);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      const txDaos = rollupDb.addTxs.mock.calls[0][0];
      expect(txDaos[0].created.getTime()).toBe(1466424490000);
      expect(txDaos[1].created.getTime()).toBe(1466424490001);
      expect(txDaos[2].created.getTime()).toBe(1466424490002);

      spy.mockRestore();
    });

    it('accept a tx chained from an unsettled tx', async () => {
      const unsettledTx = mockTx({ allowChain: 1 });
      rollupDb.getUnsettledTxs.mockResolvedValue([{ proofData: unsettledTx.proof.rawProofData }]);
      const tx = mockTx({ backwardLink: unsettledTx.proof.noteCommitment1 });

      await txReceiver.receiveTxs([tx]);
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(1);
      expect(rollupDb.addTxs).toHaveBeenCalledWith([expect.objectContaining({ id: tx.proof.txId })]);
    });

    it('reject a tx chained from an unknown note', async () => {
      const tx = mockTx({ backwardLink: randomBytes(32) });

      await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('Linked tx not found.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a tx chained from a non chainable tx', async () => {
      {
        const tx0 = mockTx({ allowChain: 0 });
        const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment1 });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Linked tx not found.');
      }

      {
        const tx0 = mockTx({ allowChain: 0 });
        const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment2 });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Linked tx not found.');
      }

      {
        const tx0 = mockTx({ allowChain: 2 });
        const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment1 });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Linked tx not found.');
      }

      {
        const tx0 = mockTx({ allowChain: 1 });
        const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment2 });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Linked tx not found.');
      }

      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a tx chained from a tx that has been chained from', async () => {
      {
        const tx0 = mockTx({ allowChain: 1 });
        const tx1 = mockTx({ backwardLink: tx0.proof.noteCommitment1 });
        const tx2 = mockTx({ backwardLink: tx0.proof.noteCommitment1 });

        await expect(() => txReceiver.receiveTxs([tx0, tx1, tx2])).rejects.toThrow('Duplicated backward link.');
      }

      {
        const unsettledTx0 = mockTx({ allowChain: 1 });
        const unsettledTx1 = mockTx({ backwardLink: unsettledTx0.proof.noteCommitment1 });
        rollupDb.getUnsettledTxs.mockResolvedValue([
          { proofData: unsettledTx0.proof.rawProofData },
          { proofData: unsettledTx1.proof.rawProofData },
        ]);
        const tx = mockTx({ backwardLink: unsettledTx0.proof.noteCommitment1 });

        await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('Duplicated backward link.');
      }

      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });

  describe('double spend', () => {
    it('reject a tx that has the same nullifiers as previously received txs', async () => {
      const tx = mockTx();
      rollupDb.nullifiersExist.mockResolvedValue(true);

      await expect(() => txReceiver.receiveTxs([tx])).rejects.toThrow('Nullifier already exists.');
      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });

    it('reject a tx that have the same nullifiers as its preceding txs', async () => {
      const nullifier = randomBytes(32);
      {
        const tx0 = mockTx({ nullifier1: nullifier });
        const tx1 = mockTx({ nullifier1: nullifier });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Nullifier already exists.');
      }

      {
        const tx0 = mockTx({ nullifier1: nullifier });
        const tx1 = mockTx({ nullifier2: nullifier });
        await expect(() => txReceiver.receiveTxs([tx0, tx1])).rejects.toThrow('Nullifier already exists.');
      }

      expect(rollupDb.addTxs).toHaveBeenCalledTimes(0);
    });
  });
});
