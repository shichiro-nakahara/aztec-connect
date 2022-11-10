import { toBigIntBE } from '@aztec/barretenberg/bigint_buffer';
import { TxHash, TxType } from '@aztec/barretenberg/blockchain';
import { ProofData } from '@aztec/barretenberg/client_proofs';
import { createLogger } from '@aztec/barretenberg/log';
import { DefiInteractionNote } from '@aztec/barretenberg/note_algorithms';
import { AssetMetricsDao, RollupDao, RollupProofDao, TxDao, BridgeMetricsDao } from '../entity/index.js';
import { SyncRollupDb } from './sync_rollup_db.js';
import { getNewAccountDaos } from './tx_dao_to_account_dao.js';

export class CachedRollupDb extends SyncRollupDb {
  private pendingTxCount!: number;
  private totalTxCount!: number;
  private rollups: RollupDao[] = [];
  private settledRollups: RollupDao[] = [];
  private unsettledTxs!: TxDao[];
  private settledNullifiers = new Set<bigint>();
  private unsettledNullifiers: Buffer[] = [];
  private log = createLogger('CachedRollupDb');

  public async init() {
    this.log('Loading rollup cache...');
    this.rollups = await super.getRollups();
    this.settledRollups = this.rollups.filter(rollup => rollup.mined);
    this.rollups
      .map(r => r.rollupProof.txs.map(tx => [tx.nullifier1, tx.nullifier2]).flat())
      .flat()
      .forEach(n => n && this.settledNullifiers.add(toBigIntBE(n)));
    this.log(`Loaded ${this.rollups.length} rollups and ${this.settledNullifiers.size} nullifiers from db...`);

    await this.refresh();
  }

  private async refresh() {
    const start = new Date().getTime();
    this.totalTxCount = await super.getTotalTxCount();
    this.pendingTxCount = await super.getPendingTxCount();
    this.unsettledTxs = await super.getUnsettledTxs();
    this.unsettledNullifiers = await super.getUnsettledNullifiers();
    this.log(`Refreshed db cache in ${new Date().getTime() - start}ms.`);
  }

  public getPendingTxCount() {
    return Promise.resolve(this.pendingTxCount);
  }

  public getRollup(id: number) {
    return Promise.resolve(this.rollups[id]);
  }

  public getRollups(take?: number, skip = 0, descending = false) {
    const rollups = descending ? this.rollups.slice().reverse() : this.rollups;
    return Promise.resolve(rollups.slice(skip, take ? skip + take : undefined));
  }

  public getSettledRollupsAfterTime(time: Date, descending = false) {
    const rollups = this.settledRollups.filter(x => x.mined !== undefined && x.mined.getTime() >= time.getTime());
    return Promise.resolve(descending ? rollups.slice().reverse() : rollups);
  }

  public getNumSettledRollups() {
    return Promise.resolve(this.settledRollups.length);
  }

  public getUnsettledTxCount() {
    return Promise.resolve(this.unsettledTxs.length);
  }

  public getUnsettledTxs() {
    return Promise.resolve(this.unsettledTxs);
  }

  public getUnsettledDepositTxs() {
    return Promise.resolve(this.unsettledTxs.filter(tx => tx.txType === TxType.DEPOSIT));
  }

  public getUnsettledAccounts() {
    return Promise.resolve(getNewAccountDaos(this.unsettledTxs));
  }

  public getUnsettledNullifiers() {
    return Promise.resolve(this.unsettledNullifiers);
  }

  public nullifiersExist(n1: Buffer, n2: Buffer) {
    return Promise.resolve(
      this.settledNullifiers.has(toBigIntBE(n1)) ||
        this.settledNullifiers.has(toBigIntBE(n2)) ||
        this.unsettledNullifiers.findIndex(b => b.equals(n1) || b.equals(n2)) != -1,
    );
  }

  public getSettledRollups(from = 0, take?: number) {
    return Promise.resolve(this.settledRollups.slice(from, take ? from + take : undefined));
  }

  public getLastSettledRollup() {
    return Promise.resolve(
      this.settledRollups.length ? this.settledRollups[this.settledRollups.length - 1] : undefined,
    );
  }

  public getNextRollupId() {
    if (this.settledRollups.length === 0) {
      return Promise.resolve(0);
    }
    return Promise.resolve(this.settledRollups[this.settledRollups.length - 1].id + 1);
  }

  public getTotalTxCount() {
    return Promise.resolve(this.totalTxCount);
  }

  public async addTx(txDao: TxDao) {
    await super.addTx(txDao);

    const { nullifier1, nullifier2 } = new ProofData(txDao.proofData);
    [nullifier1, nullifier2].filter(n => !!toBigIntBE(n)).forEach(n => this.unsettledNullifiers.push(n));

    this.unsettledTxs.push(txDao);
    this.pendingTxCount++;
    this.totalTxCount++;
  }

  public async addTxs(txs: TxDao[]) {
    await super.addTxs(txs);

    txs
      .map(tx => new ProofData(tx.proofData))
      .map(p => [p.nullifier1, p.nullifier2])
      .flat()
      .filter(n => !!toBigIntBE(n))
      .forEach(n => this.unsettledNullifiers.push(n));

    this.unsettledTxs.push(...txs);
    this.pendingTxCount += txs.length;
    this.totalTxCount += txs.length;
  }

  public async deleteTxsById(ids: Buffer[]) {
    await super.deleteTxsById(ids);
    await this.refresh();
  }

  public async addRollupProof(rollupDao: RollupProofDao) {
    await super.addRollupProof(rollupDao);
    await this.refresh();
  }

  public async addRollupProofs(rollupDaos: RollupProofDao[]) {
    await super.addRollupProofs(rollupDaos);
    await this.refresh();
  }

  public async addRollup(rollup: RollupDao) {
    await super.addRollup(rollup);
    this.rollups[rollup.id] = rollup;

    if (rollup.mined) {
      this.settledRollups[rollup.id] = rollup;
      rollup.rollupProof.txs
        .map(tx => [tx.nullifier1, tx.nullifier2])
        .flat()
        .forEach(n => n && this.settledNullifiers.add(toBigIntBE(n)));
    }

    await this.refresh();
  }

  public async confirmMined(
    id: number,
    gasUsed: number,
    gasPrice: bigint,
    mined: Date,
    ethTxHash: TxHash,
    interactionResult: DefiInteractionNote[],
    txIds: Buffer[],
    assetMetrics: AssetMetricsDao[],
    bridgeMetrics: BridgeMetricsDao[],
    subtreeRoot: Buffer,
  ) {
    const rollup = await super.confirmMined(
      id,
      gasUsed,
      gasPrice,
      mined,
      ethTxHash,
      interactionResult,
      txIds,
      assetMetrics,
      bridgeMetrics,
      subtreeRoot,
    );
    this.rollups[rollup.id] = rollup;
    this.settledRollups[rollup.id] = rollup;
    rollup.rollupProof.txs
      .map(tx => [tx.nullifier1, tx.nullifier2])
      .flat()
      .forEach(n => n && this.settledNullifiers.add(toBigIntBE(n)));
    await this.refresh();
    return rollup;
  }

  public async deletePendingTxs() {
    await super.deletePendingTxs();
    await this.refresh();
  }

  public async deleteRollupProof(id: Buffer) {
    await super.deleteRollupProof(id);
    await this.refresh();
  }

  public async deleteOrphanedRollupProofs() {
    await super.deleteOrphanedRollupProofs();
    await this.refresh();
  }

  public async deleteUnsettledRollups() {
    await super.deleteUnsettledRollups();
    this.rollups = this.settledRollups.slice();
  }

  public async deleteUnsettledClaimTxs() {
    await super.deleteUnsettledClaimTxs();
    await this.refresh();
  }

  public async eraseDb() {
    await super.eraseDb();
    await this.refresh();
  }
}
