import { emptyDir } from 'fs-extra';
import { RollupProofData } from 'barretenberg/rollup_proof';
import { RollupProviderStatus } from 'barretenberg/rollup_provider';
import { WorldStateDb } from 'barretenberg/world_state_db';
import { EthereumProvider } from 'blockchain';
import { Duration } from 'moment';
import { RollupDb } from './rollup_db';
import { Tx, TxReceiver } from './tx_receiver';
import { WorldState } from './world_state';
import moment from 'moment';
import { Metrics } from './metrics';
import { Blockchain } from 'barretenberg/blockchain';
import { Block } from 'barretenberg/block_source';
import { toBigIntBE } from 'bigint-buffer';
import { TxHash } from 'barretenberg/tx_hash';
import { ProofGenerator, ServerProofGenerator } from 'halloumi/proof_generator';
import { RollupPipelineFactory } from './rollup_pipeline';

export interface ServerConfig {
  readonly halloumiHost: string;
  readonly numInnerRollupTxs: number;
  readonly numOuterRollupProofs: number;
  readonly publishInterval: Duration;
  readonly feeLimit: bigint;
  readonly minFees: bigint[];
}

export class Server {
  private worldState: WorldState;
  private txReceiver: TxReceiver;
  private pipelineFactory: RollupPipelineFactory;
  private proofGenerator: ProofGenerator;
  private ready = false;

  constructor(
    private config: ServerConfig,
    private blockchain: Blockchain,
    private rollupDb: RollupDb,
    worldStateDb: WorldStateDb,
    private metrics: Metrics,
    provider: EthereumProvider,
  ) {
    const { numInnerRollupTxs, numOuterRollupProofs, publishInterval, feeLimit } = config;

    this.proofGenerator = new ServerProofGenerator(config.halloumiHost);
    this.pipelineFactory = new RollupPipelineFactory(
      this.proofGenerator,
      blockchain,
      rollupDb,
      worldStateDb,
      metrics,
      provider,
      publishInterval,
      feeLimit,
      numInnerRollupTxs,
      numOuterRollupProofs,
    );
    this.worldState = new WorldState(rollupDb, worldStateDb, blockchain, this.pipelineFactory, metrics);
    this.txReceiver = new TxReceiver(rollupDb, blockchain, this.proofGenerator, config.minFees);
  }

  public async start() {
    console.log('Server initializing...');
    console.log('Waiting until halloumi is ready...');
    await this.proofGenerator.awaitReady();
    await this.worldState.start();
    // The tx receiver depends on the proof generator to have been initialized to gain access to vks.
    await this.txReceiver.init();
    this.ready = true;
    console.log('Server ready to receive txs.');
  }

  public async stop() {
    console.log('Server stop...');
    this.ready = false;
    await this.txReceiver.destroy();
    await this.worldState.stop();
  }

  public isReady() {
    return this.ready;
  }

  public async removeData() {
    console.log('Removing data dir and signal to shutdown...');
    await emptyDir('./data');
    process.kill(process.pid, 'SIGINT');
  }

  public async resetPipline() {
    console.log('Resetting pipeline...');
    await this.worldState.resetPipeline();
  }

  public async getStatus(): Promise<RollupProviderStatus> {
    const status = await this.blockchain.getBlockchainStatus();

    return {
      blockchainStatus: status,
      minFees: this.config.minFees,
    };
  }

  public async getNextPublishTime() {
    const pendingTxs = await this.rollupDb.getPendingTxCount();
    if (!pendingTxs) {
      return;
    }

    const lastPublished = await this.rollupDb.getSettledRollups(0, true, 1);
    if (!lastPublished.length) {
      return;
    }

    return moment(lastPublished[0].created).add(this.config.publishInterval).toDate();
  }

  public async getPendingNoteNullifiers() {
    return this.rollupDb.getPendingNoteNullifiers();
  }

  public async getBlocks(from: number): Promise<Block[]> {
    const { nextRollupId } = await this.blockchain.getBlockchainStatus();
    if (from >= nextRollupId) {
      return [];
    }

    const rollups = await this.rollupDb.getSettledRollups(from);
    return rollups.map(dao => ({
      txHash: new TxHash(dao.ethTxHash!),
      created: dao.created,
      rollupId: dao.id,
      rollupSize: RollupProofData.getRollupSizeFromBuffer(dao.rollupProof.proofData!),
      rollupProofData: dao.rollupProof.proofData!,
      viewingKeysData: dao.viewingKeys,
      gasPrice: toBigIntBE(dao.gasPrice),
      gasUsed: dao.gasUsed,
    }));
  }

  public async getLatestRollupId() {
    return (await this.rollupDb.getNextRollupId()) - 1;
  }

  public async getLatestRollups(count: number) {
    return this.rollupDb.getRollups(count);
  }

  public async getLatestTxs(count: number) {
    return this.rollupDb.getLatestTxs(count);
  }

  public async getRollup(id: number) {
    return this.rollupDb.getRollup(id);
  }

  public async getTxs(txIds: Buffer[]) {
    return this.rollupDb.getTxsByTxIds(txIds);
  }

  public async getTx(txId: Buffer) {
    return this.rollupDb.getTx(txId);
  }

  public async receiveTx(tx: Tx) {
    const end = this.metrics.receiveTxTimer();
    const start = new Date().getTime();
    const result = await this.txReceiver.receiveTx(tx);
    console.log(`Received tx in ${new Date().getTime() - start}ms`);
    end();
    return result;
  }

  public flushTxs() {
    console.log('Flushing queued transactions...');
    this.worldState.flushTxs();
  }

  public setTopology(numInnerRollupTxs: number, numOuterRollupProofs: number) {
    this.pipelineFactory.setTopology(numInnerRollupTxs, numOuterRollupProofs);
  }
}
