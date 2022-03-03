import { NoteAlgorithms } from '@aztec/barretenberg/note_algorithms';
import { RollupProofData } from '@aztec/barretenberg/rollup_proof';
import { RollupTreeId, WorldStateDb } from '@aztec/barretenberg/world_state_db';
import { ClaimProofCreator } from '../claim_proof_creator';
import { RollupAggregator } from '../rollup_aggregator';
import { RollupCreator } from '../rollup_creator';
import { RollupDb, parseInteractionResult } from '../rollup_db';
import { RollupPublisher } from '../rollup_publisher';
import { TxFeeResolver } from '../tx_fee_resolver';
import { BridgeResolver } from '../bridge';
import { PublishTimeManager } from './publish_time_manager';
import { RollupCoordinator, TxPoolProfile } from './rollup_coordinator';
import debug from 'debug';

export class PipelineCoordinator {
  private flush = false;
  private running = false;
  private runningPromise!: Promise<void>;
  private publishTimeManager!: PublishTimeManager;
  private rollupCoordinator!: RollupCoordinator;
  private log = debug('pipeline_coordinator');
  private txPoolProfile!: TxPoolProfile;

  constructor(
    private rollupCreator: RollupCreator,
    private rollupAggregator: RollupAggregator,
    private rollupPublisher: RollupPublisher,
    private claimProofCreator: ClaimProofCreator,
    private feeResolver: TxFeeResolver,
    private worldStateDb: WorldStateDb,
    private rollupDb: RollupDb,
    private noteAlgo: NoteAlgorithms,
    private numInnerRollupTxs: number,
    private numOuterRollupProofs: number,
    private publishInterval: number,
    private bridgeResolver: BridgeResolver,
  ) {
    this.publishTimeManager = new PublishTimeManager(this.publishInterval, this.bridgeResolver);
  }

  public getNextPublishTime() {
    return this.publishTimeManager.calculateNextTimeouts();
  }

  public getTxPoolProfile() {
    return this.txPoolProfile;
  }

  /**
   * Starts monitoring for txs, and once conditions are met, creates a rollup.
   * Stops monitoring once a rollup has been successfully published or `stop` called.
   */
  public start() {
    if (this.running) {
      throw new Error('Pipeline coordinator has started running.');
    }

    this.running = true;

    const fn = async () => {
      await this.reset();

      await this.claimProofCreator.create(this.numInnerRollupTxs * this.numOuterRollupProofs);

      while (this.running) {
        this.log('Getting pending txs...');
        const pendingTxs = await this.rollupDb.getPendingTxs();

        this.log('Processing pending txs...');
        this.txPoolProfile = await this.rollupCoordinator.processPendingTxs(pendingTxs, this.flush);

        if (
          this.txPoolProfile.nextRollupProfile.published || // rollup has been published so we exit this loop
          (!this.txPoolProfile.nextRollupProfile.totalTxs && this.flush)
        ) {
          console.log('Rollup published or we are in a flush state, exiting.');
          // we are in a flush state and this iteration produced no rollup-able txs, so we exit
          this.running = false;
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 1000 * +this.running));
      }
    };

    return (this.runningPromise = fn());
  }

  /**
   * Interrupts any current rollup generation, stops monitoring for txs, and blocks until fully stopped.
   */
  public async stop() {
    if (!this.running) {
      return;
    }

    this.running = false;
    this.claimProofCreator.interrupt();
    this.rollupCoordinator?.interrupt();
    await this.runningPromise;
  }

  public flushTxs() {
    this.flush = true;
  }

  private async reset() {
    this.flush = false;

    // Erase any outstanding rollups and proofs to release unsettled txs.
    await this.rollupDb.deleteUnsettledRollups();
    await this.rollupDb.deleteOrphanedRollupProofs();
    await this.rollupDb.deleteUnsettledClaimTxs();
    const lastRollup = await this.rollupDb.getLastSettledRollup();
    const rollupId = lastRollup ? lastRollup.id + 1 : 0;

    const oldDefiRoot = this.worldStateDb.getRoot(RollupTreeId.DEFI);
    const oldDefiPath = await this.worldStateDb.getHashPath(
      RollupTreeId.DEFI,
      BigInt(Math.max(0, rollupId - 1) * RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK),
    );

    const defiInteractionNotes = lastRollup ? parseInteractionResult(lastRollup.interactionResult!) : [];
    let interactionNoteIndex = BigInt(Math.max(0, rollupId - 1)) * BigInt(RollupProofData.NUM_BRIDGE_CALLS_PER_BLOCK);
    for (const note of defiInteractionNotes) {
      await this.worldStateDb.put(
        RollupTreeId.DEFI,
        interactionNoteIndex,
        this.noteAlgo.defiInteractionNoteCommitment(note),
      );
      interactionNoteIndex++;
    }

    this.rollupCoordinator = new RollupCoordinator(
      this.publishTimeManager,
      this.rollupCreator,
      this.rollupAggregator,
      this.rollupPublisher,
      this.numInnerRollupTxs,
      this.numOuterRollupProofs,
      oldDefiRoot,
      oldDefiPath,
      this.bridgeResolver,
      this.feeResolver,
      defiInteractionNotes,
    );
  }
}
