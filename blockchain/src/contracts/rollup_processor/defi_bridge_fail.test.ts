// eslint-disable-next-line @typescript-eslint/no-var-requires
const { solidity } = require('ethereum-waffle');
import chai from 'chai';

import { expect } from 'chai';
chai.use(solidity);

import { EthAddress } from '@aztec/barretenberg/address';
import { isVirtualAsset } from '@aztec/barretenberg/asset';
import { toBufferBE } from '@aztec/barretenberg/bigint_buffer';
import { Asset, TxHash } from '@aztec/barretenberg/blockchain';
import { DefiInteractionEvent } from '@aztec/barretenberg/block_source/defi_interaction_event';
import { BridgeCallData, virtualAssetIdFlag } from '@aztec/barretenberg/bridge_call_data';
import { computeInteractionHashes } from '@aztec/barretenberg/note_algorithms';
import { RollupProofData } from '@aztec/barretenberg/rollup_proof/rollup_proof_data';
import { WorldStateConstants } from '@aztec/barretenberg/world_state';
import { randomBytes } from 'crypto';
import { Contract, Signer } from 'ethers';
import { keccak256, LogDescription, toUtf8Bytes } from 'ethers/lib/utils';
import { ethers } from 'hardhat';
import { evmSnapshot, evmRevert, setEthBalance } from '../../ganache/hardhat_chain_manipulation';
import { createRollupProof, createSendProof, DefiInteractionData } from './fixtures/create_mock_proof';
import { deployMockBridge, MockBridgeParams } from './fixtures/setup_defi_bridges';
import { setupTestRollupProcessor } from './fixtures/setup_upgradeable_test_rollup_processor';
import { TestRollupProcessor } from './fixtures/test_rollup_processor';

const parseInteractionResultFromLog = (log: LogDescription) => {
  const {
    args: { encodedBridgeCallData, nonce, totalInputValue, totalOutputValueA, totalOutputValueB, result, errorReason },
  } = log;
  return new DefiInteractionEvent(
    BridgeCallData.fromBigInt(BigInt(encodedBridgeCallData)),
    nonce.toNumber(),
    BigInt(totalInputValue),
    BigInt(totalOutputValueA),
    BigInt(totalOutputValueB),
    result,
    Buffer.from(errorReason.slice(2), 'hex'),
  );
};

describe('rollup_processor: defi bridge failures', () => {
  let rollupProcessor: TestRollupProcessor;
  let assets: Asset[];
  let signers: Signer[];
  let addresses: EthAddress[];
  let rollupProvider: Signer;
  let assetAddresses: EthAddress[];

  let snapshot: string;

  const numberOfBridgeCalls = RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK;

  const topupToken = (assetId: number, amount: bigint) =>
    assets[assetId].mint(amount, rollupProcessor.address, { signingAddress: addresses[0] });

  const topupEth = async (amount: bigint) => {
    if (rollupProvider.provider) {
      await setEthBalance(
        rollupProcessor.address,
        amount + (await rollupProvider.provider.getBalance(rollupProcessor.address.toString())).toBigInt(),
      );
    } else {
      await setEthBalance(rollupProcessor.address, amount);
    }
  };

  const dummyProof = () => createSendProof(0);

  const mockBridge = (params: MockBridgeParams = {}) =>
    deployMockBridge(rollupProvider, rollupProcessor, assetAddresses, params);

  const expectResult = async (expectedResult: DefiInteractionEvent[], txHash: TxHash) => {
    const receipt = await ethers.provider.getTransactionReceipt(txHash.toString());
    const interactionResult = receipt.logs
      .filter(l => l.address === rollupProcessor.address.toString())
      .map(l => rollupProcessor.contract.interface.parseLog(l))
      .filter(e => e.eventFragment.name === 'DefiBridgeProcessed')
      .map(parseInteractionResultFromLog);
    expect(interactionResult.length).to.be.eq(expectedResult.length);
    for (let i = 0; i < expectedResult.length; ++i) {
      expect(interactionResult[i]).to.be.eql(expectedResult[i]);
    }

    const expectedHashes = computeInteractionHashes([
      ...expectedResult,
      ...[...Array(numberOfBridgeCalls - expectedResult.length)].map(() => DefiInteractionEvent.EMPTY),
    ]);

    const hashes = await rollupProcessor.defiInteractionHashes();
    const resultHashes = [
      ...hashes,
      ...[...Array(numberOfBridgeCalls - hashes.length)].map(() => WorldStateConstants.EMPTY_INTERACTION_HASH),
    ];
    expect(expectedHashes).to.be.eql(resultHashes);
  };

  const expectFailedResult = async (
    bridgeCallData: BridgeCallData,
    inputValue: bigint,
    txHash: TxHash,
    reason: Buffer,
  ) => {
    await expectResult([new DefiInteractionEvent(bridgeCallData, 0, inputValue, 0n, 0n, false, reason)], txHash);
  };

  const expectBalance = async (assetId: number, balance: bigint) => {
    if (!isVirtualAsset(assetId)) {
      expect(await assets[assetId].balanceOf(rollupProcessor.address)).to.be.eq(balance);
    }
  };

  const formatErrorMsg = (reason: string) => {
    // format the abi encoding of `revert(reason)`
    // first word is ERROR signature 0x08c379a000000000000000000000000000000000000000000000000000000000
    // 2nd word is position of start of string
    // 3rd word is length of string
    // remaining data is string, padded to a multiple of 32 bytes
    const paddingSize = 32 - (reason.length % 32);
    const signature = Buffer.from('08c379a0', 'hex');
    const offset = toBufferBE(BigInt(32), 32);
    const byteLength = toBufferBE(BigInt(reason.length), 32);
    const reasonBytes = Buffer.concat([Buffer.from(reason, 'utf8'), Buffer.alloc(paddingSize)]);

    return Buffer.concat([signature, offset, byteLength, reasonBytes]);
  };

  before(async () => {
    signers = await ethers.getSigners();
    rollupProvider = signers[0];
    addresses = await Promise.all(signers.map(async u => EthAddress.fromString(await u.getAddress())));
    ({ rollupProcessor, assets, assetAddresses } = await setupTestRollupProcessor(signers));
  });

  beforeEach(async () => {
    snapshot = await evmSnapshot();
  });

  afterEach(async () => {
    await evmRevert(snapshot);
  });

  it('process failed defi interaction that converts token to eth', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 1,
      outputAssetIdA: 0,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupToken(1, inputValue);

    await expectBalance(1, inputValue);
    await expectBalance(0, 0n);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(1, inputValue);
    await expectBalance(0, 0n);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('process failed defi interaction that converts eth to token', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 0,
      outputAssetIdA: 1,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupEth(inputValue);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('process failed defi interaction that converts eth and token to another token', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 0,
      inputAssetIdB: 2,
      outputAssetIdA: 1,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupEth(inputValue);
    await topupToken(2, inputValue);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);
    await expectBalance(2, inputValue);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);
    await expectBalance(2, inputValue);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('process failed defi interaction that converts two tokens to eth', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 1,
      inputAssetIdB: 2,
      outputAssetIdA: 0,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupToken(1, inputValue);
    await topupToken(2, inputValue);

    await expectBalance(0, 0n);
    await expectBalance(1, inputValue);
    await expectBalance(2, inputValue);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(0, 0n);
    await expectBalance(1, inputValue);
    await expectBalance(2, inputValue);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('process failed defi interaction that converts token and a virtual asset to eth', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 1,
      inputAssetIdB: 2 + virtualAssetIdFlag,
      outputAssetIdA: 0,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupToken(1, inputValue);

    await expectBalance(0, 0n);
    await expectBalance(1, inputValue);
    await expectBalance(2, 0n);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(0, 0n);
    await expectBalance(1, inputValue);
    await expectBalance(2, 0n);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('process failed defi interaction and emit the error as the last param of the event', async () => {
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 0,
      outputAssetIdA: 1,
      canConvert: false,
    });

    const inputValue = 10n;
    await topupEth(inputValue);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    const txHash = await rollupProcessor.sendTx(tx);

    await expectBalance(0, inputValue);
    await expectBalance(1, 0n);
    await expectFailedResult(bridgeCallData, inputValue, txHash, formatErrorMsg('MockDefiBridge: canConvert = false'));
  });

  it('revert if prev defiInteraction hash is wrong', async () => {
    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      previousDefiInteractionHash: randomBytes(32),
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    await expect(rollupProcessor.sendTx(tx)).to.be.revertedWith('INCORRECT_PREVIOUS_DEFI_INTERACTION_HASH');
  });

  it('revert if total input value is empty', async () => {
    const bridgeCallData = await mockBridge();
    const inputValue = 0n;
    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    await expect(rollupProcessor.sendTx(tx)).to.be.revertedWith('ZERO_TOTAL_INPUT_VALUE');
  });

  it('process defi interaction data fails if defiInteractionHash is max size', async () => {
    const outputValueA = 15n;
    const bridgeCallData = await mockBridge({
      inputAssetIdA: 1,
      outputAssetIdA: 0,
      outputValueA,
    });
    const inputValue = 20n;

    await topupToken(1, inputValue);

    await expectBalance(1, inputValue);
    await expectBalance(0, 0n);

    const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
      defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
    });

    await rollupProcessor.stubTransactionHashes(1023);
    // when processDefiInteractions is called, NUM_BRIDGE_CALLS will be popped off of the defiInteractionHashes array.
    // 1 defi interaction hash is then added due to the rollup proof containing a DefiInteractionData object.
    // if we then copy NUM_BRIDGE_CALLS async tx hashes into defiInteractionHashes, we should trigger the array overflow
    await rollupProcessor.stubAsyncTransactionHashes(RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK);
    const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
    await expect(rollupProcessor.sendTx(tx)).to.be.revertedWith('ARRAY_OVERFLOW');
  });

  describe('Cases using additional bridge implementations', () => {
    let reentryBridge: Contract;
    let failingAsyncBridge: Contract;
    let reentryBridgeAddressId: number;
    let failingAsyncBridgeAddressId: number;

    const formatCustomErrorMsg = (reason: string) => {
      return Buffer.from(keccak256(toUtf8Bytes(reason)).substring(2), 'hex').subarray(0, 4);
    };

    before(async () => {
      reentryBridge = await (
        await ethers.getContractFactory('ReentryBridge', rollupProvider)
      ).deploy(rollupProcessor.address.toString());
      expect(await rollupProcessor.setSupportedBridge(EthAddress.fromString(reentryBridge.address), 1000000));
      reentryBridgeAddressId = (await rollupProcessor.getSupportedBridges()).length;

      failingAsyncBridge = await (
        await ethers.getContractFactory('FailingAsyncBridge', rollupProvider)
      ).deploy(rollupProcessor.address.toString());
      expect(await rollupProcessor.setSupportedBridge(EthAddress.fromString(failingAsyncBridge.address), 1000000));
      failingAsyncBridgeAddressId = await rollupProcessor.getSupportedBridgesLength();

      await topupEth(10n * 10n ** 18n);
      await setEthBalance(EthAddress.fromString(reentryBridge.address), 10n * 10n ** 18n);
      await setEthBalance(EthAddress.fromString(failingAsyncBridge.address), 10n * 10n ** 18n);
    });

    it('process defi interaction that fails because it transfer insufficient eth', async () => {
      const bridgeCallData = new BridgeCallData(reentryBridgeAddressId, 0, 0);
      const inputValue = 1n;

      await reentryBridge.addAction(0, false, true, true, '0x', 2, 0);

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      await expectFailedResult(bridgeCallData, inputValue, txHash, formatCustomErrorMsg('INSUFFICIENT_ETH_PAYMENT()'));
    });

    it('process defi interaction that fails because finalize outputvalue > eth', async () => {
      const bridgeCallData = new BridgeCallData(failingAsyncBridgeAddressId, 0, 0);
      const inputValue = 1n;

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      await rollupProcessor.sendTx(tx);

      await failingAsyncBridge.setReturnValues(1, 0);

      await expect(rollupProcessor.processAsyncDefiInteraction(0)).to.be.revertedWith('INSUFFICIENT_ETH_PAYMENT()');
    });

    it('process defi interaction that fails because async but `outputValueA > 0`', async () => {
      const bridgeCallData = new BridgeCallData(failingAsyncBridgeAddressId, 0, 0);
      const inputValue = 1n;

      await failingAsyncBridge.setReturnValues(1, 0);

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      const errorBuffer = Buffer.alloc(4 + 32 + 32);
      formatCustomErrorMsg('ASYNC_NONZERO_OUTPUT_VALUES(uint256,uint256)').copy(errorBuffer, 0, 0, 4);
      errorBuffer[32 + 4 - 1] = 1;

      await expectFailedResult(bridgeCallData, inputValue, txHash, errorBuffer);
    });

    it('process defi interaction that fails because async but `outputValueB > 0`', async () => {
      const bridgeCallData = new BridgeCallData(failingAsyncBridgeAddressId, 0, 0);
      const inputValue = 1n;

      await failingAsyncBridge.setReturnValues(0, 1);

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      const errorBuffer = Buffer.alloc(4 + 32 + 32);
      formatCustomErrorMsg('ASYNC_NONZERO_OUTPUT_VALUES(uint256,uint256)').copy(errorBuffer, 0, 0, 4);
      errorBuffer[32 + 32 + 4 - 1] = 1;

      await expectFailedResult(bridgeCallData, inputValue, txHash, errorBuffer);
    });

    it('process defi interaction that fails because returns with `outputValueA` cannot be in 252 bits', async () => {
      const bridgeCallData = new BridgeCallData(reentryBridgeAddressId, 0, 0);
      const inputValue = 1n;
      const outputValueA = 2n ** 252n;

      await reentryBridge.addAction(0, false, true, true, '0x', outputValueA, 0);

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      const errorBuffer = Buffer.alloc(4 + 32);
      formatCustomErrorMsg('OUTPUT_A_EXCEEDS_252_BITS(uint256)').copy(errorBuffer, 0, 0, 4);
      toBufferBE(outputValueA, 32).copy(errorBuffer, 4, 0, 32);

      await expectFailedResult(bridgeCallData, inputValue, txHash, errorBuffer);
    });

    it('process defi interaction that fails because returns with `outputValueB` cannot be in 252 bits', async () => {
      const bridgeCallData = new BridgeCallData(reentryBridgeAddressId, 0, 0);
      const inputValue = 1n;
      const outputValueB = 2n ** 252n;

      await reentryBridge.addAction(0, false, true, true, '0x', 0, outputValueB);

      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        defiInteractionData: [new DefiInteractionData(bridgeCallData, inputValue)],
      });

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      const errorBuffer = Buffer.alloc(4 + 32);
      formatCustomErrorMsg('OUTPUT_B_EXCEEDS_252_BITS(uint256)').copy(errorBuffer, 0, 0, 4);
      toBufferBE(outputValueB, 32).copy(errorBuffer, 4, 0, 32);

      await expectFailedResult(bridgeCallData, inputValue, txHash, errorBuffer);
    });
  });
});
