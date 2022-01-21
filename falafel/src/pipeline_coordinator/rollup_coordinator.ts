import { AssetId } from '@aztec/barretenberg/asset';
import { isAccountCreation, isDefiDeposit, TxType } from '@aztec/barretenberg/blockchain';
import { DefiDepositProofData, ProofData } from '@aztec/barretenberg/client_proofs';
import { HashPath } from '@aztec/barretenberg/merkle_tree';
import { DefiInteractionNote } from '@aztec/barretenberg/note_algorithms';
import { RollupProofData } from '@aztec/barretenberg/rollup_proof';
import { BridgeResolver } from '../bridge';
import { RollupProofDao } from '../entity/rollup_proof';
import { TxDao } from '../entity/tx';
import { RollupAggregator } from '../rollup_aggregator';
import { RollupCreator } from '../rollup_creator';
import { RollupPublisher } from '../rollup_publisher';
import { TxFeeResolver } from '../tx_fee_resolver';
import { BridgeTxQueue, createDefiRollupTx, createRollupTx, RollupTx } from './bridge_tx_queue';
import { PublishTimeManager, RollupTimeouts } from './publish_time_manager';
import { emptyProfile, profileRollup, RollupProfile } from './rollup_profiler';

export class RollupCoordinator {
  private innerProofs: RollupProofDao[] = [];
  private txs: RollupTx[] = [];
  private rollupBridgeIds: bigint[] = [];
  private rollupAssetIds: Set<AssetId> = new Set();
  private published = false;
  private bridgeQueues = new Map<bigint, BridgeTxQueue>();

  constructor(
    private publishTimeManager: PublishTimeManager,
    private rollupCreator: RollupCreator,
    private rollupAggregator: RollupAggregator,
    private rollupPublisher: RollupPublisher,
    private numInnerRollupTxs: number,
    private numOuterRollupProofs: number,
    private oldDefiRoot: Buffer,
    private oldDefiPath: HashPath,
    private bridgeResolver: BridgeResolver,
    private feeResolver: TxFeeResolver,
    private defiInteractionNotes: DefiInteractionNote[] = [],
  ) {}

  private initialiseBridgeQueues(rollupTimeouts: RollupTimeouts) {
    this.bridgeQueues = new Map<bigint, BridgeTxQueue>();
    for (const bc of this.bridgeResolver.getBridgeConfigs()) {
      const bt = rollupTimeouts.bridgeTimeouts.get(bc.bridgeId);
      this.bridgeQueues.set(bc.bridgeId, new BridgeTxQueue(bc, bt));
    }
  }

  get processedTxs() {
    return this.txs.map(rollupTx => rollupTx.tx);
  }

  interrupt() {
    this.rollupCreator.interrupt();
    this.rollupAggregator.interrupt();
    this.rollupPublisher.interrupt();
  }

  async processPendingTxs(pendingTxs: TxDao[], flush = false) {
    if (this.published) {
      return emptyProfile(this.numInnerRollupTxs * this.numOuterRollupProofs);
    }

    const rollupTimeouts = this.publishTimeManager.calculateLastTimeouts();
    this.initialiseBridgeQueues(rollupTimeouts);
    const bridgeIds = [...this.rollupBridgeIds];
    const assetIds = new Set<AssetId>(this.rollupAssetIds);
    const txs = this.getNextTxsToRollup(pendingTxs, flush, assetIds, bridgeIds);
    try {
      const rollupProfile = await this.aggregateAndPublish(txs, rollupTimeouts, flush);
      this.published = rollupProfile.published;
      return rollupProfile;
    } catch (e) {
      // Probably being interrupted.
      return emptyProfile(this.numInnerRollupTxs * this.numOuterRollupProofs);
    }
  }

  private handleNewDefiTx(
    tx: TxDao,
    remainingTxSlots: number,
    txsForRollup: RollupTx[],
    flush: boolean,
    assetIds: Set<AssetId>,
    bridgeIds: bigint[],
  ): RollupTx[] {
    // we have a new defi interaction, we need to determine if it can be accepted and if so whether it gets queued or goes straight on chain.
    const proof = new ProofData(tx.proofData);
    const defiProof = new DefiDepositProofData(proof);
    const rollupTx = createDefiRollupTx(tx, defiProof);

    const addTxs = (txs: RollupTx[]) => {
      for (const tx of txs) {
        txsForRollup.push(tx);
        if (this.feeResolver.isFeePayingAsset(tx.feeAsset)) {
          assetIds.add(tx.feeAsset);
        }
        if (!tx.bridgeId) {
          // this shouldn't be possible
          console.log(`Adding a tx that should be DEFI but it has no bridge id!`);
          continue;
        }
        if (!bridgeIds.some(id => id === tx.bridgeId)) {
          bridgeIds.push(tx.bridgeId);
        }
      }
    };

    if (bridgeIds.some(id => id === rollupTx.bridgeId)) {
      // we already have txs for this bridge in the rollup, add it straight in
      addTxs([rollupTx]);
      return txsForRollup;
    }

    if (bridgeIds.length === RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK) {
      // this rollup doesn't have any txs for this bridge and can't take any more
      return txsForRollup;
    }

    if (flush) {
      // we have been told to flush, add it straight into the rollup
      addTxs([rollupTx]);
      return txsForRollup;
    }

    const bridgeQueue = this.bridgeQueues.get(defiProof.bridgeId.toBigInt());
    if (!bridgeQueue) {
      // We don't have a bridge config for this!!
      console.log(
        `Received transaction for bridge: ${defiProof.bridgeId.toString()} but we have no config for this bridge!!`,
      );
      return txsForRollup;
    }

    //if we are beyond the timeout interval for this bridge then add it straight in
    if (bridgeQueue.transactionHasTimedOut(rollupTx)) {
      addTxs([rollupTx]);
      return txsForRollup;
    }

    // Add this tx to the queue for this bridge and work out if we can put any more txs into the current batch or create a new one
    bridgeQueue.addDefiTx(rollupTx);
    const newTxs = bridgeQueue.getTxsToRollup(
      this.feeResolver,
      remainingTxSlots,
      assetIds,
      RollupProofData.NUMBER_OF_ASSETS,
    );
    addTxs(newTxs);
    return txsForRollup;
  }

  private getNextTxsToRollup(pendingTxs: TxDao[], flush: boolean, assetIds: Set<AssetId>, bridgeIds: bigint[]) {
    const remainingTxSlots = this.numInnerRollupTxs * (this.numOuterRollupProofs - this.innerProofs.length);
    let txs: RollupTx[] = [];

    const sortedTxs = [...pendingTxs].sort((a, b) =>
      a.txType === TxType.DEFI_CLAIM && a.txType !== b.txType ? -1 : 1,
    );
    const discardedCommitments: Buffer[] = [];
    for (let i = 0; i < sortedTxs.length && txs.length < remainingTxSlots; ++i) {
      const tx = sortedTxs[i];
      const proofData = new ProofData(tx.proofData);
      const assetId = proofData.txFeeAssetId.readUInt32BE(28);

      if (isAccountCreation(tx.txType)) {
        txs.push(createRollupTx(tx, proofData));
        continue;
      }

      const discardTx = () => {
        discardedCommitments.push(proofData.noteCommitment1);
        discardedCommitments.push(proofData.noteCommitment2);
      };

      const addTx = () => {
        if (this.feeResolver.isFeePayingAsset(assetId)) {
          assetIds.add(assetId);
        }
        txs.push(createRollupTx(tx, proofData));
      };

      if (
        this.feeResolver.isFeePayingAsset(assetId) &&
        !assetIds.has(assetId) &&
        assetIds.size === RollupProofData.NUMBER_OF_ASSETS
      ) {
        discardTx();
        continue;
      }

      if (
        !proofData.backwardLink.equals(Buffer.alloc(32)) &&
        discardedCommitments.some(c => c.equals(proofData.backwardLink))
      ) {
        discardTx();
        continue;
      }

      if (!isDefiDeposit(tx.txType)) {
        addTx();
      } else {
        txs = this.handleNewDefiTx(tx, remainingTxSlots - txs.length, txs, flush, assetIds, bridgeIds);
      }
    }
    return txs;
  }

  private printRollupState(rollupProfile: RollupProfile, timeout: boolean, flush: boolean) {
    console.log(
      `New rollup - size: ${rollupProfile.rollupSize}, numTxs: ${rollupProfile.totalTxs}, timeout/flush: ${timeout}/${flush}, gas balance: ${rollupProfile.gasBalance}, inner/outer chains: ${rollupProfile.innerChains}/${rollupProfile.outerChains}`,
    );
    for (const bp of rollupProfile.bridgeProfiles) {
      console.log(
        `Defi bridge published. Id: ${bp.bridgeId.toString()}, numTxs: ${bp.numTxs}, gas balance: ${
          bp.totalGasEarnt - bp.totalGasCost
        }`,
      );
    }
  }

  private async buildInnerRollup(pendingTxs: RollupTx[]) {
    const txs = pendingTxs.splice(0, this.numInnerRollupTxs);
    const rollupProofDao = await this.rollupCreator.create(
      txs.map(rollupTx => rollupTx.tx),
      this.rollupBridgeIds,
      this.rollupAssetIds,
    );
    this.txs = [...this.txs, ...txs];
    this.innerProofs.push(rollupProofDao);
    return pendingTxs;
  }

  private async aggregateAndPublish(txs: RollupTx[], rollupTimeouts: RollupTimeouts, flush: boolean) {
    let pendingTxs = [...txs];

    const numRemainingSlots = (this.numOuterRollupProofs - this.innerProofs.length) * this.numInnerRollupTxs;
    if (pendingTxs.length > numRemainingSlots) {
      // this shouldn't happen!
      console.log(
        `ERROR: Number of pending txs is larger than the number of remaining slots! Num txs: ${pendingTxs.length}, remaining slots: ${numRemainingSlots}`,
      );
    }

    // start by rolling up all of the full inners that we can
    const numInnersBefore = this.innerProofs.length;
    while (pendingTxs.length >= this.numInnerRollupTxs && this.innerProofs.length < this.numOuterRollupProofs) {
      pendingTxs = await this.buildInnerRollup(pendingTxs);
    }

    const allRollupTxs = [...this.txs, ...pendingTxs];
    const rollupProfile = profileRollup(
      allRollupTxs,
      this.feeResolver,
      this.numInnerRollupTxs,
      this.numInnerRollupTxs * this.numOuterRollupProofs,
    );

    if (!rollupProfile.totalTxs) {
      // no txs at all
      return rollupProfile;
    }

    const profit = rollupProfile.gasBalance >= 0n;
    const timedout = rollupTimeouts.baseTimeout
      ? rollupProfile.earliestTx.getTime() <= rollupTimeouts.baseTimeout.timeout.getTime()
      : false;
    const shouldPublish = flush || profit || timedout;

    if (this.innerProofs.length < this.numOuterRollupProofs) {
      // we have built all of the full inner rollups that we can but the rollup isn't full
      if (this.innerProofs.length !== numInnersBefore) {
        // we built at least 1 inner rollup in this iteration
        // we will exit without doing anything further
        // building inners takes time and there may well be additional txs available to include in this rollup
        return rollupProfile;
      }
      // we haven't been able to build a full inner rollup on this iteration but we may have a condition that means we need to publish immediately
      // this could be that at least one tx has 'timed out', we have been told to 'flush' or the rollup is profitable
      if (!shouldPublish) {
        // no need to publish early, exit here
        return rollupProfile;
      }
      // we need to publish early, rollup any stragglers before doing so
      if (pendingTxs.length) {
        pendingTxs = await this.buildInnerRollup(pendingTxs);
        if (pendingTxs.length) {
          // should now be empty
          console.log('ERROR: Pending Txs should be empty as we just built the last inner rollup before publish!');
        }
      }
    }

    // here we either have
    // 1. no rollup at all
    // 2. a partial rollup that we need to publish
    // 3. a full rollup
    if (!this.innerProofs.length) {
      // nothing to publish
      return rollupProfile;
    }

    const rollupDao = await this.rollupAggregator.aggregateRollupProofs(
      this.innerProofs,
      this.oldDefiRoot,
      this.oldDefiPath,
      this.defiInteractionNotes,
      this.rollupBridgeIds.concat(
        Array(RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK - this.rollupBridgeIds.length).fill(0n),
      ),
      [...this.rollupAssetIds],
    );
    rollupProfile.published = await this.rollupPublisher.publishRollup(rollupDao);
    this.printRollupState(rollupProfile, timedout, flush);
    return rollupProfile;
  }
}
