import { toBufferBE } from '@aztec/barretenberg/bigint_buffer';
import { TxType } from '@aztec/barretenberg/blockchain';
import { BridgeCallData } from '@aztec/barretenberg/bridge_call_data';
import { ProofData } from '@aztec/barretenberg/client_proofs';
import { BridgeConfig } from '@aztec/barretenberg/rollup_provider';
import { numToUInt32BE } from '@aztec/barretenberg/serialize';
import { randomBytes } from 'crypto';
import { TxDao } from '../entity/index.js';
import { TxFeeResolver } from '../tx_fee_resolver/index.js';
import { Tx } from './tx.js';
import { TxFeeAllocator } from './tx_fee_allocator.js';
import { jest } from '@jest/globals';

const bridgeConfigs: BridgeConfig[] = [
  {
    bridgeAddressId: 1,
    numTxs: 5,
    gas: 500000,
    permittedAssets: [0, 1],
  },
  {
    bridgeAddressId: 2,
    numTxs: 10,
    gas: 2000000,
    permittedAssets: [0, 1],
  },
];

type Mockify<T> = {
  [P in keyof T]?: ReturnType<typeof jest.fn>;
};

const BASE_GAS = 20000;
const feeConstants = [10000, 10000, 50000, 60000, 0, 50000, 30000];
const NON_FEE_PAYING_ASSET = 9999;
const GAS_PRICE = 1n;

const getBridgeCost = (bridgeCallData: bigint) => {
  const bridge = BridgeCallData.fromBigInt(bridgeCallData);
  const bridgeConfig = bridgeConfigs.find(bc => bc.bridgeAddressId === bridge.bridgeAddressId);
  if (!bridgeConfig) {
    throw new Error(`Requested cost for invalid bridgeCallData: ${bridgeCallData.toString()}`);
  }
  return bridgeConfig.gas!;
};

const getSingleBridgeCost = (bridgeCallData: bigint) => {
  const bridge = BridgeCallData.fromBigInt(bridgeCallData);
  const bridgeConfig = bridgeConfigs.find(bc => bc.bridgeAddressId === bridge.bridgeAddressId);
  if (!bridgeConfig) {
    throw new Error(`Requested cost for invalid bridgeCallData: ${bridgeCallData.toString()}`);
  }
  const { gas, numTxs } = bridgeConfig;
  return Math.ceil(gas! / numTxs);
};

const generateValidBridgeCallData = (bridgeConfig: BridgeConfig) => {
  return new BridgeCallData(
    bridgeConfig.bridgeAddressId,
    bridgeConfig.permittedAssets[0],
    bridgeConfig.permittedAssets[1],
    undefined,
    undefined,
    0n,
  );
};

const bridgeCallDatas = bridgeConfigs.map(bc => generateValidBridgeCallData(bc));

const getTxGasWithBase = (txType: TxType) => feeConstants[txType] + BASE_GAS;

const txTypeToProofId = (txType: TxType) => (txType < TxType.WITHDRAW_HIGH_GAS ? txType + 1 : txType);

const toProofData = (buf: Buffer) => {
  return new ProofData(buf);
};

const toFee = (gas: number) => BigInt(gas) * GAS_PRICE;
const toGas = (fee: bigint) => Number(fee / GAS_PRICE);

const toTxDao = (tx: Tx, txType: TxType) => {
  return new TxDao({
    id: tx.proof.txId,
    proofData: tx.proof.rawProofData,
    offchainTxData: undefined,
    signature: undefined,
    nullifier1: undefined,
    nullifier2: undefined,
    dataRootsIndex: 0,
    created: new Date(),
    txType,
    excessGas: 0, // provided later
  });
};

const mockTx = (id: number, gas: number, assetId: number, txType = TxType.ACCOUNT, bridgeCallData = 0n) =>
  ({
    id: Buffer.from([id]),
    proof: toProofData(
      Buffer.concat([
        numToUInt32BE(txTypeToProofId(txType), 32),
        randomBytes(8 * 32),
        toBufferBE(toFee(gas), 32),
        numToUInt32BE(assetId, 32),
        toBufferBE(BridgeCallData.fromBigInt(bridgeCallData).toBigInt(), 32),
        randomBytes(5 * 32),
      ]),
    ),
  } as any as Tx);

const mockDefiBridgeTx = (id: number, gas: number, bridgeCallData: bigint, assetId = 0) =>
  mockTx(id, gas, assetId, TxType.DEFI_DEPOSIT, bridgeCallData);

const preciselyFundedTx = (id: number, txType: TxType, assetId: number, excessGas = 0) => {
  return mockTx(id, getTxGasWithBase(txType) + excessGas, assetId, txType);
};

describe('Tx Fee Allocator', () => {
  let feeResolver: Mockify<TxFeeResolver>;
  let txFeeAllocator: TxFeeAllocator;

  beforeEach(() => {
    jest.spyOn(console, 'log').mockImplementation(() => {});

    feeResolver = {
      getGasPaidForByFee: jest.fn((assetId: number, fee: bigint) => toGas(fee)),
      getTxFeeFromGas: jest.fn((assetId: number, gas: bigint) => gas),
      getAdjustedTxGas: jest.fn((assetId: number, txType: TxType) => getTxGasWithBase(txType)),
      getAdjustedBridgeTxGas: jest.fn(
        (assetId: number, bridgeCallData: bigint) =>
          getSingleBridgeCost(bridgeCallData) + getTxGasWithBase(TxType.DEFI_DEPOSIT),
      ),
      isFeePayingAsset: jest.fn((assetId: number) => assetId < 3),
    };

    txFeeAllocator = new TxFeeAllocator(feeResolver as any);
  });

  it('correctly validates single payment', () => {
    const tx = preciselyFundedTx(1, TxType.TRANSFER, 0);
    const validation = txFeeAllocator.validateReceivedTxs([tx], [TxType.TRANSFER]);
    expect(validation.feePayingAsset).toEqual(0);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER));
    expect(validation.gasRequired).toEqual(getTxGasWithBase(TxType.TRANSFER));
    expect(validation.hasFeelessTxs).toEqual(false);
  });

  it('correctly validates multiple payments', () => {
    const txs = [preciselyFundedTx(1, TxType.TRANSFER, 0), preciselyFundedTx(2, TxType.TRANSFER, 0)];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [TxType.TRANSFER, TxType.TRANSFER]);
    expect(validation.feePayingAsset).toEqual(0);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER) * 2);
    expect(validation.gasRequired).toEqual(getTxGasWithBase(TxType.TRANSFER) * 2);
    expect(validation.hasFeelessTxs).toEqual(false);
  });

  it('should throw if no fee paying assets', () => {
    const txs = [
      preciselyFundedTx(1, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(2, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
    ];
    expect(() => {
      txFeeAllocator.validateReceivedTxs(txs, [TxType.TRANSFER, TxType.TRANSFER]);
    }).toThrow('Transactions must have exactly 1 fee paying asset');
  });

  it('should throw if multiple fee paying assets', () => {
    const txs = [preciselyFundedTx(1, TxType.TRANSFER, 0), preciselyFundedTx(2, TxType.TRANSFER, 1)];
    expect(() => {
      txFeeAllocator.validateReceivedTxs(txs, [TxType.TRANSFER, TxType.TRANSFER]);
    }).toThrow('Transactions must have exactly 1 fee paying asset');
  });

  it('correctly determines fee paying asset', () => {
    const txs = [preciselyFundedTx(1, TxType.TRANSFER, 0), preciselyFundedTx(2, TxType.TRANSFER, NON_FEE_PAYING_ASSET)];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [TxType.TRANSFER, TxType.TRANSFER]);
    expect(validation.feePayingAsset).toEqual(0);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER));
    expect(validation.gasRequired).toEqual(getTxGasWithBase(TxType.TRANSFER) * 2);
    expect(validation.hasFeelessTxs).toEqual(true);
  });

  it('correctly detects non-paying DEFI', () => {
    const txs = [
      preciselyFundedTx(1, TxType.TRANSFER, 0),
      mockDefiBridgeTx(
        2,
        getTxGasWithBase(TxType.DEFI_DEPOSIT) + getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
        bridgeCallDatas[0].toBigInt(),
        NON_FEE_PAYING_ASSET,
      ),
    ];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [TxType.TRANSFER, TxType.DEFI_DEPOSIT]);
    expect(validation.feePayingAsset).toEqual(0);
    // should only count the gas provided by the TRANSFER
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER));
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(true);
  });

  it('correctly calculates gas', () => {
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1),
      preciselyFundedTx(2, TxType.DEPOSIT, 1),
      preciselyFundedTx(3, TxType.TRANSFER, 1),
      preciselyFundedTx(4, TxType.WITHDRAW_HIGH_GAS, 1),
      preciselyFundedTx(5, TxType.WITHDRAW_TO_WALLET, 1),
      mockDefiBridgeTx(
        6,
        getTxGasWithBase(TxType.DEFI_DEPOSIT) + getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
        bridgeCallDatas[0].toBigInt(),
        1,
      ),
    ];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.DEFI_DEPOSIT,
    ]);
    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
    );
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(false);
  });

  it('excludes gas from non-paying assets', () => {
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1),
      preciselyFundedTx(2, TxType.DEPOSIT, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(3, TxType.TRANSFER, 1),
      preciselyFundedTx(4, TxType.WITHDRAW_HIGH_GAS, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(5, TxType.WITHDRAW_TO_WALLET, 1),
      mockDefiBridgeTx(
        6,
        getTxGasWithBase(TxType.DEFI_DEPOSIT) + getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
        bridgeCallDatas[0].toBigInt(),
        NON_FEE_PAYING_ASSET,
      ),
    ];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.DEFI_DEPOSIT,
    ]);
    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET),
    );
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(true);
  });

  it('correctly calculates gas with excess', () => {
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1),
      preciselyFundedTx(2, TxType.DEPOSIT, 1),
      mockTx(3, getTxGasWithBase(TxType.TRANSFER) + 13, 1, TxType.TRANSFER),
      preciselyFundedTx(4, TxType.WITHDRAW_HIGH_GAS, 1),
      mockTx(5, getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) + 5, 1),
      mockDefiBridgeTx(
        6,
        getTxGasWithBase(TxType.DEFI_DEPOSIT) + getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
        bridgeCallDatas[0].toBigInt(),
        1,
      ),
    ];
    const validation = txFeeAllocator.validateReceivedTxs(txs, [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.DEFI_DEPOSIT,
    ]);
    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        13 +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        5 +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()),
    );
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(false);
  });

  it('should not modify excess gas if none provided', () => {
    const txs = [preciselyFundedTx(1, TxType.TRANSFER, 0), preciselyFundedTx(2, TxType.TRANSFER, 0)];
    const txTypes = [TxType.TRANSFER, TxType.TRANSFER];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 0]);
  });

  it('should allocate gas according to provided fee if all assets are fee paying', () => {
    const excessGas = [10, 11, 12, 13, 14];
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1, excessGas[0]),
      preciselyFundedTx(2, TxType.DEPOSIT, 1, excessGas[1]),
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas[2]),
      preciselyFundedTx(4, TxType.WITHDRAW_HIGH_GAS, 1, excessGas[3]),
      preciselyFundedTx(5, TxType.WITHDRAW_TO_WALLET, 1, excessGas[4]),
    ];
    const txTypes = [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
    ];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    const totalExcess = excessGas.reduce((p, n) => p + n, 0);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        totalExcess,
    );
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET),
    );
    expect(validation.hasFeelessTxs).toEqual(false);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    expect(txDaos.map(dao => dao.excessGas)).toEqual(excessGas);
  });

  it('should allocate gas according to provided fee if all assets are fee paying - include DEFI', () => {
    const excessGas = [10, 11, 12, 13, 14, 15];
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1, excessGas[0]),
      preciselyFundedTx(2, TxType.DEPOSIT, 1, excessGas[1]),
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas[2]),
      preciselyFundedTx(4, TxType.WITHDRAW_HIGH_GAS, 1, excessGas[3]),
      preciselyFundedTx(5, TxType.WITHDRAW_TO_WALLET, 1, excessGas[4]),
      mockDefiBridgeTx(
        6,
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
          getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
          getTxGasWithBase(TxType.DEFI_CLAIM) +
          excessGas[5],
        bridgeCallDatas[0].toBigInt(),
        1,
      ),
    ];
    const txTypes = [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.DEFI_DEPOSIT,
    ];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    const totalExcess = excessGas.reduce((p, n) => p + n, 0);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM) +
        totalExcess,
    );
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(false);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    expect(txDaos.map(dao => dao.excessGas)).toEqual(excessGas);
  });

  it('should allocate excess gas to first non-fee paying tx', () => {
    const excessGas = getTxGasWithBase(TxType.TRANSFER);
    const txs = [
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas),
      preciselyFundedTx(4, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
    ];
    const txTypes = [TxType.TRANSFER, TxType.TRANSFER];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER) + excessGas);

    expect(validation.gasRequired).toEqual(getTxGasWithBase(TxType.TRANSFER) * 2);
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    // no excess. the additional fee on the first transfer was completely used to pay for non-fee payer
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 0]);
  });

  it('should allocate excess gas to first non-fee paying tx 2', () => {
    const excessGas = getTxGasWithBase(TxType.TRANSFER) + 50;
    const txs = [
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas),
      preciselyFundedTx(4, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
    ];
    const txTypes = [TxType.TRANSFER, TxType.TRANSFER];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER) + excessGas);

    expect(validation.gasRequired).toEqual(getTxGasWithBase(TxType.TRANSFER) * 2);
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    // first transfer paid 50 more than needed for the second tx. the excess goes to the second transfer
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 50]);
  });

  it('should allocate excess gas to first non-fee paying tx 3', () => {
    const excessGas =
      getTxGasWithBase(TxType.TRANSFER) +
      getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) * 2 +
      getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
      101;
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1),
      preciselyFundedTx(2, TxType.DEPOSIT, 1, excessGas),
      preciselyFundedTx(3, TxType.TRANSFER, 1),
      preciselyFundedTx(4, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(5, TxType.WITHDRAW_HIGH_GAS, 1),
      preciselyFundedTx(6, TxType.WITHDRAW_HIGH_GAS, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(7, TxType.WITHDRAW_HIGH_GAS, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(8, TxType.WITHDRAW_TO_WALLET, 1),
      preciselyFundedTx(9, TxType.WITHDRAW_TO_WALLET, NON_FEE_PAYING_ASSET),
    ];
    const txTypes = [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.WITHDRAW_TO_WALLET,
    ];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        excessGas,
    );

    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) * 2 +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) * 3 +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) * 2,
    );
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    // only the 101 excess is left after tx costs have been accounted for
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 0, 0, 101, 0, 0, 0, 0, 0]);
  });

  it('should allocate excess gas to first non-fee paying tx 4', () => {
    const excessGas =
      getTxGasWithBase(TxType.TRANSFER) +
      getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) * 2 +
      getTxGasWithBase(TxType.WITHDRAW_TO_WALLET);
    const txs = [
      preciselyFundedTx(1, TxType.ACCOUNT, 1),
      preciselyFundedTx(2, TxType.DEPOSIT, 1, excessGas),
      preciselyFundedTx(3, TxType.TRANSFER, 1),
      preciselyFundedTx(4, TxType.TRANSFER, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(5, TxType.WITHDRAW_HIGH_GAS, 1),
      preciselyFundedTx(6, TxType.WITHDRAW_HIGH_GAS, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(7, TxType.WITHDRAW_HIGH_GAS, NON_FEE_PAYING_ASSET),
      preciselyFundedTx(8, TxType.WITHDRAW_TO_WALLET, 1),
      preciselyFundedTx(9, TxType.WITHDRAW_TO_WALLET, NON_FEE_PAYING_ASSET),
    ];
    const txTypes = [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.WITHDRAW_TO_WALLET,
      TxType.WITHDRAW_TO_WALLET,
    ];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) +
        excessGas,
    );
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.ACCOUNT) +
        getTxGasWithBase(TxType.DEPOSIT) +
        getTxGasWithBase(TxType.TRANSFER) * 2 +
        getTxGasWithBase(TxType.WITHDRAW_HIGH_GAS) * 3 +
        getTxGasWithBase(TxType.WITHDRAW_TO_WALLET) * 2,
    );
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    // no excess, tx costs have consumed all provided gas
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 0, 0, 0, 0, 0, 0, 0, 0]);
  });

  it('should allocate excess gas to non-paying DEFI', () => {
    const excessGas =
      getTxGasWithBase(TxType.DEFI_DEPOSIT) +
      getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
      getTxGasWithBase(TxType.DEFI_CLAIM);
    const txs = [
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas),
      mockDefiBridgeTx(6, 0, bridgeCallDatas[0].toBigInt(), 1),
    ];
    const txTypes = [TxType.TRANSFER, TxType.DEFI_DEPOSIT];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER) + excessGas);
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    // no excess. the additional fee on the first transfer was completely used to pay for the DEFI
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, 0]);
  });

  it('should allocate excess gas to non-paying DEFI - full bridge', () => {
    const excessGas =
      getTxGasWithBase(TxType.DEFI_DEPOSIT) +
      getBridgeCost(bridgeCallDatas[0].toBigInt()) +
      getTxGasWithBase(TxType.DEFI_CLAIM);
    const txs = [
      preciselyFundedTx(3, TxType.TRANSFER, 1, excessGas),
      mockDefiBridgeTx(6, 0, bridgeCallDatas[0].toBigInt(), NON_FEE_PAYING_ASSET),
    ];
    const txTypes = [TxType.TRANSFER, TxType.DEFI_DEPOSIT];

    // daos start off with 0 excess gas
    const txDaos = txs.map((tx, i) => {
      return toTxDao(tx, txTypes[i]);
    });

    const validation = txFeeAllocator.validateReceivedTxs(txs, txTypes);

    expect(validation.feePayingAsset).toEqual(1);
    expect(validation.gasProvided).toEqual(getTxGasWithBase(TxType.TRANSFER) + excessGas);
    // gas required includes the claim
    expect(validation.gasRequired).toEqual(
      getTxGasWithBase(TxType.TRANSFER) +
        getTxGasWithBase(TxType.DEFI_DEPOSIT) +
        getSingleBridgeCost(bridgeCallDatas[0].toBigInt()) +
        getTxGasWithBase(TxType.DEFI_CLAIM),
    );
    expect(validation.hasFeelessTxs).toEqual(true);

    // no excess gas so nothing should be 'reallocated'
    txFeeAllocator.reallocateGas(txDaos, txs, txTypes, validation);

    const expectedExcess = (bridgeConfigs[0].numTxs - 1) * getSingleBridgeCost(bridgeCallDatas[0].toBigInt());

    // the DEFI should have excess equal to all other bridge tx slots
    expect(txDaos.map(dao => dao.excessGas)).toEqual([0, expectedExcess]);
  });
});
