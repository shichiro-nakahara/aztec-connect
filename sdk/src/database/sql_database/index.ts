import { AliasHash } from '@aztec/barretenberg/account_id';
import { GrumpkinAddress } from '@aztec/barretenberg/address';
import { TxHash } from '@aztec/barretenberg/tx_hash';
import { Connection, ConnectionOptions, MoreThan, MoreThanOrEqual, Repository } from 'typeorm';
import { Note } from '../../note';
import { AccountId, UserData } from '../../user';
import { UserAccountTx, UserDefiTx, UserJoinSplitTx } from '../../user_tx';
import { Claim } from '../claim';
import { Alias, Database, SigningKey } from '../database';
import { AccountTxDao } from './account_tx_dao';
import { AliasDao } from './alias_dao';
import { ClaimDao } from './claim_dao';
import { DefiTxDao } from './defi_tx_dao';
import { JoinSplitTxDao } from './join_split_tx_dao';
import { KeyDao } from './key_dao';
import { NoteDao } from './note_dao';
import { UserDataDao } from './user_data_dao';
import { UserKeyDao } from './user_key_dao';

export const getOrmConfig = (dbPath?: string): ConnectionOptions => ({
  name: 'aztec2-sdk',
  type: 'sqlite',
  database: dbPath === ':memory:' ? dbPath : `${dbPath || '.'}/aztec2-sdk.sqlite`,
  entities: [AccountTxDao, AliasDao, ClaimDao, DefiTxDao, JoinSplitTxDao, KeyDao, NoteDao, UserDataDao, UserKeyDao],
  synchronize: true,
  logging: false,
});

const toUserJoinSplitTx = (tx: JoinSplitTxDao) =>
  new UserJoinSplitTx(
    tx.txHash,
    tx.userId,
    tx.assetId,
    tx.publicInput,
    tx.publicOutput,
    tx.privateInput,
    tx.recipientPrivateOutput,
    tx.senderPrivateOutput,
    tx.inputOwner,
    tx.outputOwner,
    tx.ownedByUser,
    tx.created,
    tx.settled,
  );

const toUserAccountTx = (tx: AccountTxDao) =>
  new UserAccountTx(
    tx.txHash,
    tx.userId,
    tx.aliasHash,
    tx.newSigningPubKey1,
    tx.newSigningPubKey2,
    tx.migrated,
    tx.created,
    tx.settled,
  );

const toUserDefiTx = (tx: DefiTxDao) =>
  new UserDefiTx(
    tx.txHash,
    tx.userId,
    tx.bridgeId,
    tx.privateInput,
    tx.privateOutput,
    tx.depositValue,
    tx.created,
    tx.outputValueA,
    tx.outputValueB,
    tx.settled,
    tx.claimed,
  );

export class SQLDatabase implements Database {
  private accountTxRep: Repository<AccountTxDao>;
  private aliasRep: Repository<AliasDao>;
  private claimRep: Repository<ClaimDao>;
  private defiTxRep: Repository<DefiTxDao>;
  private joinSplitTxRep: Repository<JoinSplitTxDao>;
  private keyRep: Repository<KeyDao>;
  private noteRep: Repository<NoteDao>;
  private userDataRep: Repository<UserDataDao>;
  private userKeyRep: Repository<UserKeyDao>;

  constructor(private connection: Connection) {
    this.accountTxRep = this.connection.getRepository(AccountTxDao);
    this.aliasRep = this.connection.getRepository(AliasDao);
    this.claimRep = this.connection.getRepository(ClaimDao);
    this.defiTxRep = this.connection.getRepository(DefiTxDao);
    this.joinSplitTxRep = this.connection.getRepository(JoinSplitTxDao);
    this.keyRep = this.connection.getRepository(KeyDao);
    this.noteRep = this.connection.getRepository(NoteDao);
    this.userDataRep = this.connection.getRepository(UserDataDao);
    this.userKeyRep = this.connection.getRepository(UserKeyDao);
  }

  async init() {}

  async close() {
    await this.connection.close();
  }

  async clear() {
    await this.connection.synchronize(true);
  }

  async addNote(note: Note) {
    await this.noteRep.save(note);
  }

  async getNote(index: number) {
    return this.noteRep.findOne({ index });
  }

  async getNoteByNullifier(nullifier: Buffer) {
    return this.noteRep.findOne({ nullifier });
  }

  async nullifyNote(index: number) {
    await this.noteRep.update(index, { nullified: true });
  }

  async getUserNotes(userId: AccountId) {
    return this.noteRep.find({ where: { owner: userId, nullified: false } });
  }

  async addClaim(claim: Claim) {
    await this.claimRep.save(claim);
  }

  async getClaim(nullifier: Buffer) {
    return this.claimRep.findOne({ nullifier });
  }

  async getUser(userId: AccountId) {
    return this.userDataRep.findOne({ id: userId });
  }

  async addUser(user: UserData) {
    await this.userDataRep.save(user);
  }

  async getUsers() {
    return this.userDataRep.find();
  }

  async updateUser(user: UserData) {
    await this.userDataRep.update({ id: user.id }, user);
  }

  async removeUser(userId: AccountId) {
    const user = await this.getUser(userId);
    if (!user) return;

    await this.accountTxRep.delete({ userId });
    await this.joinSplitTxRep.delete({ userId });
    await this.userKeyRep.delete({ accountId: userId });
    await this.noteRep.delete({ owner: userId });
    await this.userDataRep.delete({ id: userId });
  }

  async resetUsers() {
    await this.aliasRep.clear();
    await this.noteRep.clear();
    await this.userKeyRep.clear();
    await this.accountTxRep.clear();
    await this.joinSplitTxRep.clear();
    await this.userDataRep.update({ syncedToRollup: MoreThan(-1) }, { syncedToRollup: -1 });
  }

  async addJoinSplitTx(tx: UserJoinSplitTx) {
    await this.joinSplitTxRep.save(tx);
  }

  async getJoinSplitTx(userId: AccountId, txHash: TxHash) {
    const tx = await this.joinSplitTxRep.findOne({ txHash, userId });
    return tx ? toUserJoinSplitTx(tx) : undefined;
  }

  async getJoinSplitTxs(userId) {
    const txs = await this.joinSplitTxRep.find({ where: { userId }, order: { settled: 'DESC' } });
    const unsettled = txs.filter(tx => !tx.settled).sort((a, b) => (a.created < b.created ? 1 : -1));
    const settled = txs.filter(tx => tx.settled);
    return [...unsettled, ...settled].map(toUserJoinSplitTx);
  }

  async getJoinSplitTxsByTxHash(txHash: TxHash) {
    return (await this.joinSplitTxRep.find({ where: { txHash } })).map(toUserJoinSplitTx);
  }

  async settleJoinSplitTx(txHash: TxHash, settled: Date) {
    await this.joinSplitTxRep.update({ txHash }, { settled });
  }

  async addAccountTx(tx: UserAccountTx) {
    await this.accountTxRep.save(tx);
  }

  async getAccountTx(txHash: TxHash) {
    const tx = await this.accountTxRep.findOne({ txHash });
    return tx ? toUserAccountTx(tx) : undefined;
  }

  async getAccountTxs(userId) {
    const txs = await this.accountTxRep.find({ where: { userId }, order: { settled: 'DESC' } });
    const unsettled = txs.filter(tx => !tx.settled).sort((a, b) => (a.created < b.created ? 1 : -1));
    const settled = txs.filter(tx => tx.settled);
    return [...unsettled, ...settled].map(toUserAccountTx);
  }

  async settleAccountTx(txHash: TxHash, settled: Date) {
    await this.accountTxRep.update({ txHash }, { settled });
  }

  async addDefiTx(tx: UserDefiTx) {
    await this.defiTxRep.save({ ...tx }); // save() will mutate tx, changing undefined values to null.
  }

  async getDefiTx(txHash: TxHash) {
    const tx = await this.defiTxRep.findOne({ txHash });
    return tx ? toUserDefiTx(tx) : undefined;
  }

  async getDefiTxs(userId) {
    const txs = await this.defiTxRep.find({ where: { userId }, order: { settled: 'DESC' } });
    const unsettled = txs.filter(tx => !tx.settled).sort((a, b) => (a.created < b.created ? 1 : -1));
    const settled = txs.filter(tx => tx.settled);
    return [...unsettled, ...settled].map(toUserDefiTx);
  }

  async settleDefiTx(txHash: TxHash, outputValueA: bigint, outputValueB: bigint, settled: Date) {
    await this.defiTxRep.update({ txHash }, { outputValueA, outputValueB, settled });
  }

  async claimDefiTx(txHash: TxHash, claimed: Date) {
    await this.defiTxRep.update({ txHash }, { claimed });
  }

  async addUserSigningKey(signingKey: SigningKey) {
    await this.userKeyRep.save(signingKey);
  }

  async getUserSigningKeys(accountId: AccountId) {
    return await this.userKeyRep.find({ accountId });
  }

  async getUserSigningKeyIndex(accountId: AccountId, key: GrumpkinAddress) {
    const keyBuffer = key.toBuffer();
    const signingKey = await this.userKeyRep.findOne({ where: { accountId, key: keyBuffer.slice(0, 32) } });
    return signingKey ? signingKey.treeIndex : undefined;
  }

  async removeUserSigningKeys(accountId: AccountId) {
    await this.userKeyRep.delete({ accountId });
  }

  async setAlias(alias: Alias) {
    await this.aliasRep.save(alias);
  }

  async setAliases(aliases: Alias[]) {
    // TODO: Dedupe for bulk insert.
    for (const alias of aliases) {
      await this.aliasRep.save(alias);
    }
  }

  async getAlias(aliasHash: AliasHash, address: GrumpkinAddress) {
    return this.aliasRep.findOne({ aliasHash, address });
  }

  async getAliases(aliasHash: AliasHash) {
    return this.aliasRep.find({ aliasHash });
  }

  async getLatestNonceByAddress(address: GrumpkinAddress) {
    const alias = await this.aliasRep.findOne({ where: { address }, order: { latestNonce: 'DESC' } });
    return alias?.latestNonce;
  }

  async getLatestNonceByAliasHash(aliasHash: AliasHash) {
    const alias = await this.aliasRep.findOne({ where: { aliasHash }, order: { latestNonce: 'DESC' } });
    return alias?.latestNonce;
  }

  async getAliasHashByAddress(address: GrumpkinAddress, nonce?: number) {
    const alias = await this.aliasRep.findOne({
      where: { address, latestNonce: MoreThanOrEqual(nonce || 0) },
      order: { latestNonce: nonce !== undefined ? 'ASC' : 'DESC' },
    });
    return alias?.aliasHash;
  }

  async getAddressByAliasHash(aliasHash: AliasHash, nonce?: number) {
    const alias = await this.aliasRep.findOne({
      where: { aliasHash, latestNonce: MoreThanOrEqual(nonce || 0) },
      order: { latestNonce: nonce !== undefined ? 'ASC' : 'DESC' },
    });
    return alias?.address;
  }

  async addKey(name: string, value: Buffer) {
    await this.keyRep.save({ name, value });
  }

  async getKey(name: string) {
    const key = await this.keyRep.findOne({ name });
    return key ? key.value : undefined;
  }

  async deleteKey(name: string) {
    await this.keyRep.delete({ name });
  }
}
