// eslint-disable-next-line @typescript-eslint/no-var-requires
const { solidity } = require('ethereum-waffle');
import chai from 'chai';

import { expect } from 'chai';
chai.use(solidity);

import { EthAddress } from '@aztec/barretenberg/address';
import { Asset, TxHash } from '@aztec/barretenberg/blockchain';
import { BridgeCallData } from '@aztec/barretenberg/bridge_call_data';
import {
  computeInteractionHashes,
  DefiInteractionNote,
  packInteractionNotes,
} from '@aztec/barretenberg/note_algorithms';
import { RollupProofData } from '@aztec/barretenberg/rollup_proof';
import { Signer } from 'ethers';
import { LogDescription } from 'ethers/lib/utils';
import { ethers } from 'hardhat';
import {
  createDefiDepositProof,
  createRollupProof,
  createSendProof,
  DefiInteractionData,
  mergeInnerProofs,
} from './fixtures/create_mock_proof';
import { mockAsyncBridge } from './fixtures/setup_defi_bridges';
import { setupTestRollupProcessor } from './fixtures/setup_upgradeable_test_rollup_processor';
import { TestRollupProcessor } from './fixtures/test_rollup_processor';
import { InnerProofOutput } from './fixtures/create_mock_proof';
import { evmSnapshot, evmRevert } from '../../ganache/hardhat_chain_manipulation';

const parseInteractionResultFromLog = (log: LogDescription) => {
  const {
    args: { encodedBridgeCallData, nonce, totalInputValue, totalOutputValueA, totalOutputValueB, result },
  } = log;
  return new DefiInteractionNote(
    BridgeCallData.fromBigInt(BigInt(encodedBridgeCallData)),
    nonce.toNumber(),
    BigInt(totalInputValue),
    BigInt(totalOutputValueA),
    BigInt(totalOutputValueB),
    result,
  );
};

describe('rollup_processor: multi async defi', () => {
  let rollupProcessor: TestRollupProcessor;
  let assets: Asset[];
  let signers: Signer[];
  let addresses: EthAddress[];
  let rollupProvider: Signer;
  let assetAddresses: EthAddress[];

  let snapshot: string;

  const topupToken = (assetId: number, amount: bigint) =>
    assets[assetId].mint(amount, rollupProcessor.address, { signingAddress: addresses[0] });

  const dummyProof = () => createSendProof();

  const fetchResult = async (txHash: TxHash, eventName = 'DefiBridgeProcessed') => {
    const receipt = await ethers.provider.getTransactionReceipt(txHash.toString());
    return receipt.logs
      .filter(l => l.address === rollupProcessor.address.toString())
      .map(l => rollupProcessor.contract.interface.parseLog(l))
      .filter(e => e.eventFragment.name === eventName)
      .map(parseInteractionResultFromLog);
  };

  const expectHashes = async (expectedResult: DefiInteractionNote[]) => {
    const expectedHashes = computeInteractionHashes(expectedResult);
    expect(await rollupProcessor.defiInteractionHashes()).to.be.eql(expectedHashes);
  };

  const expectAsyncHashes = async (expectedResult: DefiInteractionNote[]) => {
    const expectedHashes = computeInteractionHashes(expectedResult);
    expect(await rollupProcessor.asyncDefiInteractionHashes()).to.be.eql(expectedHashes);
  };

  const expectResult = async (txHash: TxHash, expectedResult: DefiInteractionNote[]) => {
    expect(await fetchResult(txHash)).to.be.eql(expectedResult);
  };

  const expectBalance = async (assetId: number, balance: bigint) =>
    expect(await assets[assetId].balanceOf(rollupProcessor.address)).to.be.eq(balance);

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

  it('process multiple async defi interactions', async () => {
    const outputValueA = 12n;
    const outputValueB = 7n;
    const numAdditionalInteractions = 3;
    const rollupSize = 32;
    const bridgeCallDatas: BridgeCallData[] = [];
    for (let i = 0; i < RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK; i++) {
      const { bridgeCallData } = await mockAsyncBridge(rollupProvider, rollupProcessor, assetAddresses, {
        inputAssetIdA: 1,
        outputAssetIdA: 0,
        outputAssetIdB: 2,
        outputValueA,
        outputValueB,
      });
      bridgeCallDatas.push(bridgeCallData);
    }

    const initialBalance = 5000n;
    await topupToken(1, initialBalance);

    await expectBalance(0, 0n);
    await expectBalance(1, initialBalance);
    await expectBalance(2, 0n);

    const numAsyncInteractions = RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK + numAdditionalInteractions; // Plus 3, because we want to test the system can handle an async queue larger than numberOfBridgeCalls.
    let rollupId = 0;

    // convert
    let totalInputValue = 0n;
    const defiDepositProofs: InnerProofOutput[] = [];
    const defiInteractionData: DefiInteractionData[] = [];
    for (let i = 0; i < numAsyncInteractions; i++) {
      const inputValue = BigInt(i + 1);
      totalInputValue += inputValue;
      const defiDepositProof = createDefiDepositProof(
        bridgeCallDatas[i % RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK],
        inputValue,
      );
      defiDepositProofs.push(defiDepositProof);
      defiInteractionData.push(
        new DefiInteractionData(bridgeCallDatas[i % RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK], inputValue),
      );
    }

    while (defiDepositProofs.length) {
      const { encodedProofData } = createRollupProof(
        rollupProvider,
        mergeInnerProofs(defiDepositProofs.splice(0, RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK)),
        {
          rollupId,
          rollupSize,
          defiInteractionData: defiInteractionData.splice(0, RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK),
        },
      );
      rollupId++;

      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);

      await expectResult(txHash, []);
      await expectHashes([]);
      await expectAsyncHashes([]);
    }

    await expectBalance(0, 0n);
    await expectBalance(1, initialBalance - totalInputValue);
    await expectBalance(2, 0n);

    // finalise
    const expectedAsyncResult: DefiInteractionNote[] = [];
    for (let i = 0; i < numAsyncInteractions; ++i) {
      const inputValue = BigInt(i + 1);
      const interactionNonce = i;
      const txHash = await rollupProcessor.processAsyncDefiInteraction(interactionNonce);
      const asyncResult = new DefiInteractionNote(
        bridgeCallDatas[i % RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK],
        interactionNonce,
        inputValue,
        outputValueA,
        outputValueB,
        true,
      );
      expectedAsyncResult.push(asyncResult);
      await expectResult(txHash, [asyncResult]);
      await expectHashes([]);
      await expectAsyncHashes(expectedAsyncResult);
    }

    await expectBalance(0, outputValueA * BigInt(numAsyncInteractions));
    await expectBalance(1, initialBalance - totalInputValue);
    await expectBalance(2, outputValueB * BigInt(numAsyncInteractions));

    // populate hashes with asyncHashes
    {
      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        rollupId,
        rollupSize,
      });
      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);
      rollupId++;

      await expectResult(txHash, []);
      await expectHashes(expectedAsyncResult);
      await expectAsyncHashes([]);
    }

    // process the last `RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK` interaction hashes
    {
      const defiInteractionData = expectedAsyncResult.slice(-RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK);
      const previousDefiInteractionHash = packInteractionNotes(defiInteractionData);
      const { encodedProofData } = createRollupProof(rollupProvider, dummyProof(), {
        rollupId,
        rollupSize,
        previousDefiInteractionHash,
      });
      const tx = await rollupProcessor.createRollupProofTx(encodedProofData, [], []);
      const txHash = await rollupProcessor.sendTx(tx);
      rollupId++;

      await expectResult(txHash, []);
      await expectHashes(
        expectedAsyncResult.slice(0, expectedAsyncResult.length - RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK),
      );
      await expectAsyncHashes([]);
    }
  });
});
