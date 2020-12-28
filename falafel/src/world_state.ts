import { MemoryFifo } from 'barretenberg/fifo';
import { InnerProofData, RollupProofData } from 'barretenberg/rollup_proof';
import { WorldStateDb } from 'barretenberg/world_state_db';
import { toBigIntBE, toBufferBE } from 'bigint-buffer';
import { Block, Blockchain } from 'blockchain';
import { RollupDao } from './entity/rollup';
import { RollupProofDao } from './entity/rollup_proof';
import { TxDao } from './entity/tx';
import { RollupDb } from './rollup_db';
import { TxAggregator } from './tx_aggregator';

const innerProofDataToTxDao = (tx: InnerProofData, viewingKeys: Buffer[], created: Date) => {
  const txDao = new TxDao();
  txDao.id = tx.txId;
  txDao.proofData = tx.toBuffer();
  txDao.viewingKey1 = viewingKeys[0];
  txDao.viewingKey2 = viewingKeys[1];
  txDao.nullifier1 = tx.nullifier1;
  txDao.nullifier2 = tx.nullifier2;
  txDao.created = created;
  return txDao;
};

export class WorldState {
  private blockQueue = new MemoryFifo<Block>();

  constructor(
    public rollupDb: RollupDb,
    public worldStateDb: WorldStateDb,
    private blockchain: Blockchain,
    private txAggregator: TxAggregator,
  ) {}

  public async start() {
    await this.worldStateDb.start();

    await this.syncState();

    this.txAggregator.start();

    this.blockchain.on('block', block => this.blockQueue.put(block));
    await this.blockchain.start(await this.rollupDb.getNextRollupId());

    this.blockQueue.process(block => this.handleBlock(block));
  }

  public async stop() {
    this.blockQueue.cancel();
    this.blockchain.stop();
    await this.txAggregator.stop();
    this.worldStateDb.stop();
  }

  public flushTxs() {
    this.txAggregator.flushTxs();
  }

  /**
   * Called at startup to bring us back in sync.
   * Erases any orphaned rollup proofs and unsettled rollups from rollup db.
   * Processes all rollup blocks from the last settled rollup in the rollup db.
   */
  private async syncState() {
    this.printState();
    console.log('Syncing state...');

    const nextRollupId = await this.rollupDb.getNextRollupId();
    const blocks = await this.blockchain.getBlocks(nextRollupId);

    for (const block of blocks) {
      await this.updateDbs(block);
    }

    await this.rollupDb.deleteUnsettledRollups();
    await this.rollupDb.deleteOrphanedRollupProofs();

    console.log('Sync complete.');
  }

  public printState() {
    console.log(`Data size: ${this.worldStateDb.getSize(0)}`);
    console.log(`Data root: ${this.worldStateDb.getRoot(0).toString('hex')}`);
    console.log(`Null root: ${this.worldStateDb.getRoot(1).toString('hex')}`);
    console.log(`Root root: ${this.worldStateDb.getRoot(2).toString('hex')}`);
  }

  private async handleBlock(block: Block) {
    // Interrupt completion of any current rollup construction.
    await this.txAggregator.stop();

    await this.updateDbs(block);

    // Kick off asynchronously. Allows incoming block to be handled and interrupt completion.
    this.txAggregator.start();
  }

  /**
   * Inserts the rollup in the given block into the merkle tree and sql db.
   */
  private async updateDbs(block: Block) {
    const { rollupProofData: rawRollupData, viewingKeysData } = block;
    const rollupProofData = RollupProofData.fromBuffer(rawRollupData, viewingKeysData);
    const { rollupId, rollupHash, newDataRoot } = rollupProofData;

    console.log(`Processing rollup ${rollupId}: ${rollupHash.toString('hex')}...`);

    if (newDataRoot.equals(this.worldStateDb.getRoot(0))) {
      // This must be the rollup we just published. Commit the world state.
      await this.worldStateDb.commit();
    } else {
      // Someone elses rollup. Discard any of our world state modifications and update world state with new rollup.
      await this.worldStateDb.rollback();
      await this.addRollupToWorldState(rollupProofData);
    }

    await this.confirmOrAddRollupToDb(rollupProofData, block);

    await this.printState();
  }

  private async confirmOrAddRollupToDb(rollup: RollupProofData, block: Block) {
    const { txHash, rollupProofData: proofData, created } = block;

    if (await this.rollupDb.getRollupProof(rollup.rollupHash)) {
      await this.rollupDb.confirmMined(rollup.rollupId);
    } else {
      // Not a rollup we created. Add or replace rollup.
      const rollupProofDao = new RollupProofDao();
      rollupProofDao.id = rollup.rollupHash;
      rollupProofDao.rollupSize = rollup.rollupSize;
      rollupProofDao.dataStartIndex = rollup.dataStartIndex;
      rollupProofDao.proofData = proofData;
      rollupProofDao.txs = rollup.innerProofData.map((p, i) =>
        innerProofDataToTxDao(p, rollup.viewingKeys[i], created),
      );
      rollupProofDao.created = created;

      const rollupDao = new RollupDao({
        id: rollup.rollupId,
        dataRoot: rollup.newDataRoot,
        rollupProof: rollupProofDao,
        ethTxHash: txHash.toBuffer(),
        mined: true,
        created: new Date(),
        viewingKeys: Buffer.concat(rollup.viewingKeys.flat()),
      });

      await this.rollupDb.addRollup(rollupDao);
    }
  }

  private async addRollupToWorldState(rollup: RollupProofData) {
    const { rollupId, rollupSize, dataStartIndex, innerProofData } = rollup;
    for (let i = 0; i < innerProofData.length; ++i) {
      const tx = innerProofData[i];
      await this.worldStateDb.put(0, BigInt(dataStartIndex + i * rollupSize), tx.newNote1);
      await this.worldStateDb.put(0, BigInt(dataStartIndex + i * rollupSize + 1), tx.newNote2);
      await this.worldStateDb.put(1, toBigIntBE(tx.nullifier1), toBufferBE(1n, 64));
      await this.worldStateDb.put(1, toBigIntBE(tx.nullifier2), toBufferBE(1n, 64));
    }
    if (innerProofData.length < rollupSize) {
      await this.worldStateDb.put(0, BigInt(dataStartIndex + rollupSize * 2 - 1), Buffer.alloc(64, 0));
    }
    await this.worldStateDb.put(2, BigInt(rollupId + 1), this.worldStateDb.getRoot(0));

    await this.worldStateDb.commit();
  }
}
