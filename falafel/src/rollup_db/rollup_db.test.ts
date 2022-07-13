import { AliasHash } from '@aztec/barretenberg/account_id';
import { GrumpkinAddress } from '@aztec/barretenberg/address';
import { TxHash, TxType } from '@aztec/barretenberg/blockchain';
import { randomBytes } from 'crypto';
import { Connection, createConnection } from 'typeorm';
import { AccountDao, AssetMetricsDao, BridgeMetricsDao, ClaimDao, RollupDao, RollupProofDao, TxDao } from '../entity';
import { RollupDb, TypeOrmRollupDb } from './';
import { randomAccountTx, randomClaim, randomRollup, randomRollupProof, randomTx } from './fixtures';
import { txDaoToAccountDao } from './tx_dao_to_account_dao';

describe('rollup_db', () => {
  let connection: Connection;
  let rollupDb: RollupDb;

  beforeEach(async () => {
    connection = await createConnection({
      type: 'sqlite',
      database: ':memory:',
      entities: [TxDao, RollupProofDao, RollupDao, AccountDao, ClaimDao, AssetMetricsDao, BridgeMetricsDao],
      dropSchema: true,
      synchronize: true,
      logging: false,
    });
    rollupDb = new TypeOrmRollupDb(connection);
  });

  afterEach(async () => {
    await connection.close();
  });

  it('should add tx with no rollup', async () => {
    const txDao = randomTx({ signature: randomBytes(32) });
    await rollupDb.addTx(txDao);

    const result = await rollupDb.getTx(txDao.id);
    expect(result!).toEqual(txDao);
  });

  it('should add account tx', async () => {
    const txDao = randomAccountTx();
    await rollupDb.addTx(txDao);

    expect(await rollupDb.getAccountTxCount()).toBe(1);
    expect(await rollupDb.getAccountCount()).toBe(1);
  });

  it('should count accounts that have unique account public key', async () => {
    const createAccountTxs = () => {
      const aliasHash = AliasHash.random();
      const accountPublicKey0 = GrumpkinAddress.random();
      const accountPublicKey1 = GrumpkinAddress.random();
      return {
        registerTx: randomAccountTx({ aliasHash, accountPublicKey: accountPublicKey0 }),
        addKeyTx: randomAccountTx({ aliasHash, accountPublicKey: accountPublicKey0, addKey: true }),
        migrateTx: randomAccountTx({ aliasHash, accountPublicKey: accountPublicKey1, migrate: true }),
      };
    };
    const accounts = [...Array(4)].map(() => createAccountTxs());
    const txs = [
      accounts[0].registerTx,
      accounts[0].migrateTx,
      accounts[0].addKeyTx,
      accounts[1].registerTx,
      accounts[2].registerTx,
      accounts[2].addKeyTx,
      accounts[3].migrateTx,
    ];
    for (const tx of txs) {
      await rollupDb.addTx(tx);
    }

    expect(await rollupDb.getAccountTxCount()).toBe(7);
    // New accounts are added with register tx and migrate tx.
    expect(await rollupDb.getAccountCount()).toBe(5);
  });

  it('should delete an account when its tx is deleted', async () => {
    const accountPublicKeys = [...Array(4)].map(() => GrumpkinAddress.random());
    const aliasHash0 = AliasHash.random();
    const aliasHash1 = AliasHash.random();
    const txs = [
      randomAccountTx({ accountPublicKey: accountPublicKeys[0], aliasHash: aliasHash0 }),
      randomAccountTx({ accountPublicKey: accountPublicKeys[1], aliasHash: aliasHash0, migrate: true }),
      randomAccountTx({ accountPublicKey: accountPublicKeys[2], aliasHash: aliasHash1 }),
      randomAccountTx({ accountPublicKey: accountPublicKeys[3], aliasHash: aliasHash1, migrate: true }),
    ];
    for (const tx of txs) {
      await rollupDb.addTx(tx);
    }
    const rollupProof = randomRollupProof([txs[0], txs[2]]);
    await rollupDb.addRollupProof(rollupProof);

    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[0], aliasHash0)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[1], aliasHash0)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[2], aliasHash1)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[3], aliasHash1)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[0], aliasHash1)).toBe(false);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[1], aliasHash1)).toBe(false);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[2], aliasHash0)).toBe(false);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[3], aliasHash0)).toBe(false);

    // txs[1] and txs[3] will be deleted.
    await rollupDb.deletePendingTxs();

    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[0], aliasHash0)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[1], aliasHash0)).toBe(false);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[2], aliasHash1)).toBe(true);
    expect(await rollupDb.isAliasRegisteredToAccount(accountPublicKeys[3], aliasHash1)).toBe(false);
  });

  it('should bulk add txs', async () => {
    const txs = [
      TxType.DEPOSIT,
      TxType.WITHDRAW_TO_WALLET,
      TxType.ACCOUNT,
      TxType.DEFI_DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.ACCOUNT,
      TxType.DEFI_CLAIM,
    ].map(txType => randomTx({ txType }));

    await rollupDb.addTxs(txs);

    expect(await rollupDb.getTotalTxCount()).toBe(8);
    expect(await rollupDb.getJoinSplitTxCount()).toBe(4);
    expect(await rollupDb.getDefiTxCount()).toBe(1);
    expect(await rollupDb.getAccountTxCount()).toBe(2);
    expect(await rollupDb.getAccountCount()).toBe(2);
  });

  it('should get rollup proof by id', async () => {
    const rollup = randomRollupProof([]);
    await rollupDb.addRollupProof(rollup);

    const rollupDao = (await rollupDb.getRollupProof(rollup.id))!;
    expect(rollupDao.id).toStrictEqual(rollup.id);
    expect(rollupDao.encodedProofData).toStrictEqual(rollup.encodedProofData);
    expect(rollupDao.created).toStrictEqual(rollup.created);
  });

  it('should get rollups by an array of rollup ids', async () => {
    const rollups: RollupDao[] = [];
    for (let i = 0; i < 6; ++i) {
      const rollupProof = randomRollupProof([]);
      await rollupDb.addRollupProof(rollupProof);
      const rollup = randomRollup(i, rollupProof);
      await rollupDb.addRollup(rollup);
      rollups.push(rollup);
    }

    const saved = await rollupDb.getRollupsByRollupIds([1, 2, 5]);
    expect(saved.length).toBe(3);
    expect(saved.map(r => r.id)).toEqual(expect.arrayContaining([1, 2, 5]));
  });

  it('should add rollup proof and insert its txs', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();

    const rollupProof = randomRollupProof([tx0, tx1]);
    await rollupDb.addRollupProof(rollupProof);

    const rollupProofDao = (await rollupDb.getRollupProof(rollupProof.id))!;
    const newTxDao0 = await rollupDb.getTx(tx0.id);
    expect(newTxDao0!.rollupProof).toStrictEqual(rollupProofDao);
    const newTxDao1 = await rollupDb.getTx(tx1.id);
    expect(newTxDao1!.rollupProof).toStrictEqual(rollupProofDao);
  });

  it('should add rollup proof and update the rollup id for its txs', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();
    const tx2 = randomTx();
    await rollupDb.addTx(tx0);
    await rollupDb.addTx(tx1);
    await rollupDb.addTx(tx2);

    expect(await rollupDb.getPendingTxCount()).toBe(3);

    {
      const rollupProof = randomRollupProof([tx0]);
      await rollupDb.addRollupProof(rollupProof);

      // Check the rollup proof is associated with tx0.
      const rollupDao = (await rollupDb.getRollupProof(rollupProof.id))!;
      const newTxDao0 = await rollupDb.getTx(tx0.id);
      expect(newTxDao0!.rollupProof).toStrictEqual(rollupDao);

      // Check tx1 is still pending.
      const newTxDao1 = await rollupDb.getTx(tx1.id);
      expect(newTxDao1!.rollupProof).toBeUndefined();

      expect(await rollupDb.getPendingTxCount()).toBe(2);
      expect(await rollupDb.getPendingTxs()).toStrictEqual([tx1, tx2]);
    }

    {
      // Add a new rollup proof containing tx0 and tx1.
      const rollupProof = randomRollupProof([tx0, tx1]);
      await rollupDb.addRollupProof(rollupProof);

      // Check the rollup proof is associated with tx0 and tx1.
      const rollupDao = (await rollupDb.getRollupProof(rollupProof.id))!;
      const newTxDao0 = await rollupDb.getTx(tx0.id);
      const newTxDao1 = await rollupDb.getTx(tx1.id);
      expect(newTxDao0!.rollupProof).toStrictEqual(rollupDao);
      expect(newTxDao1!.rollupProof).toStrictEqual(rollupDao);

      expect(await rollupDb.getPendingTxs()).toStrictEqual([tx2]);
    }
  });

  it('get nullifiers of unsettled txs', async () => {
    const tx0 = randomTx();
    tx0.nullifier2 = undefined;
    await rollupDb.addTx(tx0);

    const tx1 = randomTx();
    {
      await rollupDb.addTx(tx1);
      const rollupProof = randomRollupProof([tx1], 0);
      const rollup = randomRollup(0, rollupProof);
      await rollupDb.addRollup(rollup);
    }

    const tx2 = randomTx();
    {
      await rollupDb.addTx(tx2);
      const rollupProof = randomRollupProof([tx2], 1);
      const rollup = randomRollup(0, rollupProof);
      await rollupDb.addRollup(rollup);
      await rollupDb.confirmMined(rollup.id, 0, 0n, new Date(), TxHash.random(), [], [tx2.id], [], [], randomBytes(32));
    }

    const nullifiers = await rollupDb.getUnsettledNullifiers();
    expect(nullifiers.length).toBe(3);
    expect(nullifiers).toEqual(expect.arrayContaining([tx0.nullifier1, tx1.nullifier1, tx1.nullifier2]));
  });

  it('should update rollup id for txs when newer proof added', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();
    const tx2 = randomTx();
    const tx3 = randomTx();
    await rollupDb.addTx(tx0);
    await rollupDb.addTx(tx1);
    await rollupDb.addTx(tx2);
    await rollupDb.addTx(tx3);

    const rollupProof1 = randomRollupProof([tx0, tx1]);
    await rollupDb.addRollupProof(rollupProof1);

    const rollupProof2 = randomRollupProof([tx2, tx3]);
    await rollupDb.addRollupProof(rollupProof2);

    const rollupProof3 = randomRollupProof([tx0, tx1, tx2, tx3]);
    await rollupDb.addRollupProof(rollupProof3);

    expect((await rollupDb.getRollupProof(rollupProof1.id, true))!.txs).toHaveLength(0);
    expect((await rollupDb.getRollupProof(rollupProof2.id, true))!.txs).toHaveLength(0);
    expect((await rollupDb.getRollupProof(rollupProof3.id, true))!.txs).toHaveLength(4);

    const rollupDao = (await rollupDb.getRollupProof(rollupProof3.id))!;
    expect((await rollupDb.getTx(tx0.id))!.rollupProof).toStrictEqual(rollupDao);
    expect((await rollupDb.getTx(tx1.id))!.rollupProof).toStrictEqual(rollupDao);
    expect((await rollupDb.getTx(tx2.id))!.rollupProof).toStrictEqual(rollupDao);
    expect((await rollupDb.getTx(tx3.id))!.rollupProof).toStrictEqual(rollupDao);

    const rollupDaoWithTxs = (await rollupDb.getRollupProof(rollupProof3.id, true))!;
    expect(rollupDaoWithTxs.txs).toStrictEqual([tx0, tx1, tx2, tx3]);
  });

  it('should set tx rollup proof ids to null if rollup proof is deleted', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();

    const rollupProof = randomRollupProof([tx0, tx1]);
    await rollupDb.addRollupProof(rollupProof);

    await rollupDb.deleteRollupProof(rollupProof.id);

    const newTxDao0 = await rollupDb.getTx(tx0.id);
    expect(newTxDao0!.rollupProof).toBeUndefined();
    const newTxDao1 = await rollupDb.getTx(tx1.id);
    expect(newTxDao1!.rollupProof).toBeUndefined();
  });

  it('should delete orphaned rollup proof', async () => {
    const rollupProof = randomRollupProof([]);
    await rollupDb.addRollupProof(rollupProof);
    await rollupDb.deleteTxlessRollupProofs();
    const rollupDao = (await rollupDb.getRollupProof(rollupProof.id))!;
    expect(rollupDao).toBeUndefined();
  });

  it('should add and get rollup with txs', async () => {
    const txs = [
      TxType.DEPOSIT,
      TxType.WITHDRAW_TO_WALLET,
      TxType.ACCOUNT,
      TxType.DEFI_DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.ACCOUNT,
      TxType.DEFI_CLAIM,
    ].map(txType => randomTx({ txType }));
    const rollupProof = randomRollupProof(txs, 0);
    const rollup = randomRollup(0, rollupProof);
    await rollupDb.addRollup(rollup);

    const newRollup = (await rollupDb.getRollup(0))!;
    expect(newRollup).toStrictEqual(rollup);

    expect(await rollupDb.getTotalTxCount()).toBe(8);
    expect(await rollupDb.getJoinSplitTxCount()).toBe(4);
    expect(await rollupDb.getDefiTxCount()).toBe(1);
    expect(await rollupDb.getAccountTxCount()).toBe(2);
    expect(await rollupDb.getAccountCount()).toBe(2);
  });

  it('should add rollup with account txs that have already in db', async () => {
    const txs = [randomAccountTx(), randomAccountTx()];
    for (const tx of txs) {
      await rollupDb.addTx(tx);
    }

    const rollupProof = randomRollupProof(txs, 0);
    const rollup = randomRollup(0, rollupProof);

    expect(await rollupDb.getAccountCount()).toBe(2);

    await rollupDb.addRollup(rollup);

    const newRollup = (await rollupDb.getRollup(0))!;
    expect(newRollup).toStrictEqual(rollup);

    expect(await rollupDb.getAccountCount()).toBe(2);
  });

  it('should update existing rollup', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();

    {
      const txs = [tx0, tx1];
      const rollupProof = randomRollupProof(txs, 0);
      const rollup = randomRollup(0, rollupProof);

      await rollupDb.addRollup(rollup);

      const newRollup = (await rollupDb.getRollup(0))!;
      expect(newRollup).toStrictEqual(rollup);
    }

    {
      const rollupProof = randomRollupProof([tx0, tx1], 0);
      const rollup = randomRollup(0, rollupProof);

      await rollupDb.addRollup(rollup);

      const newRollup = (await rollupDb.getRollup(0))!;
      expect(newRollup).toStrictEqual(rollup);
    }
  });

  it('should get settled txs', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();
    const rollupProof = randomRollupProof([tx0, tx1], 0);
    const rollup = randomRollup(0, rollupProof);

    // Before adding to db, lets re-order the txs to ensure we get them back in "rollup order".
    {
      const [t0, t1] = rollup.rollupProof.txs;
      rollup.rollupProof.txs = [t1, t0];
    }

    await rollupDb.addRollup(rollup);

    const settledRollups1 = await rollupDb.getSettledRollups();
    expect(settledRollups1.length).toBe(0);

    await rollupDb.confirmMined(
      rollup.id,
      0,
      0n,
      new Date(),
      TxHash.random(),
      [],
      [tx0.id, tx1.id],
      [],
      [],
      randomBytes(32),
    );

    const settledRollups2 = await rollupDb.getSettledRollups();
    expect(settledRollups2.length).toBe(1);
    expect(settledRollups2[0].rollupProof).not.toBeUndefined();
    expect(settledRollups2[0].rollupProof.txs[0].id).toEqual(tx0.id);
    expect(settledRollups2[0].rollupProof.txs[1].id).toEqual(tx1.id);
  });

  it('should erase db', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();
    const rollupProof = randomRollupProof([tx0, tx1], 0);
    const rollup = randomRollup(0, rollupProof);

    await rollupDb.addRollup(rollup);

    {
      const txs = await rollupDb.getUnsettledTxs();
      const rollups = await rollupDb.getUnsettledRollups();
      expect(txs.length).toBe(2);
      expect(rollups.length).toBe(1);
    }

    await rollupDb.eraseDb();

    {
      const txs = await rollupDb.getUnsettledTxs();
      const rollups = await rollupDb.getUnsettledRollups();
      expect(txs.length).toBe(0);
      expect(rollups.length).toBe(0);
    }
  });

  it('should get unsettled tx count', async () => {
    const tx0 = randomTx();
    await rollupDb.addTx(tx0);

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);

    const rollupProof = randomRollupProof([tx0], 0);
    await rollupDb.addRollupProof(rollupProof);

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);

    const rollup = randomRollup(0, rollupProof);
    await rollupDb.addRollup(rollup);

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);

    await rollupDb.confirmMined(rollup.id, 0, 0n, new Date(), TxHash.random(), [], [tx0.id], [], [], randomBytes(32));

    expect(await rollupDb.getUnsettledTxCount()).toBe(0);
  });

  it('should get unsettled txs', async () => {
    const tx0 = randomTx();
    const tx1 = randomTx();
    const tx2 = randomTx();
    await rollupDb.addTx(tx0);
    await rollupDb.addTx(tx1);
    await rollupDb.addTx(tx2);

    const rollupProof0 = randomRollupProof([tx0], 0);
    const rollup0 = randomRollup(0, rollupProof0);
    const rollupProof1 = randomRollupProof([tx1], 1);
    const rollup1 = randomRollup(1, rollupProof1);

    await rollupDb.addRollup(rollup0);
    await rollupDb.addRollup(rollup1);

    await rollupDb.confirmMined(rollup0.id, 0, 0n, new Date(), TxHash.random(), [], [tx0.id], [], [], randomBytes(32));

    const unsettledTxs = await rollupDb.getUnsettledTxs();
    expect(unsettledTxs.length).toBe(2);
    expect(unsettledTxs.map(tx => tx.id)).toEqual(expect.arrayContaining([tx1.id, tx2.id]));
  });

  it('should get unsettled deposit txs', async () => {
    const txs = [
      TxType.DEFI_CLAIM,
      TxType.WITHDRAW_TO_WALLET,
      TxType.DEPOSIT,
      TxType.ACCOUNT,
      TxType.WITHDRAW_HIGH_GAS,
      TxType.DEPOSIT,
      TxType.DEFI_DEPOSIT,
      TxType.TRANSFER,
    ].map(txType => randomTx({ txType }));
    for (const tx of txs) {
      await rollupDb.addTx(tx);
    }

    const result = await rollupDb.getUnsettledDepositTxs();
    expect(result.length).toBe(2);
    expect(result).toEqual(expect.arrayContaining([txs[2], txs[5]]));
  });

  it('should delete unsettled rollups', async () => {
    const tx0 = randomTx();
    await rollupDb.addTx(tx0);

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);
    expect(await rollupDb.getUnsettledRollups()).toHaveLength(0);

    const rollupProof = randomRollupProof([tx0], 0);
    await rollupDb.addRollupProof(rollupProof);

    const rollup = randomRollup(0, rollupProof);
    await rollupDb.addRollup(rollup);

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);
    expect(await rollupDb.getUnsettledRollups()).toHaveLength(1);

    await rollupDb.deleteUnsettledRollups();

    expect(await rollupDb.getUnsettledTxCount()).toBe(1);
    expect(await rollupDb.getUnsettledRollups()).toHaveLength(0);
  });

  it('should add and get pending claims', async () => {
    const pendingClaims: ClaimDao[] = [];
    for (let i = 0; i < 16; ++i) {
      const claim = randomClaim();
      claim.interactionNonce = i % 2; // nonce is based on even or odd-ness
      if (i % 4 === 0) {
        // every 4th is fully claimed
        claim.claimed = new Date();
        claim.interactionResultRollupId = (claim.interactionNonce + 1) * 32;
      } else {
        // every odd is not ready for claim
        if (i % 2 === 0) {
          claim.interactionResultRollupId = (claim.interactionNonce + 1) * 32;
        }
        pendingClaims.push(claim);
      }
      await rollupDb.addClaim(claim);
    }

    // only those pending claims with a valid result are ready to rollup
    expect(await rollupDb.getClaimsToRollup()).toEqual(pendingClaims.filter(claim => claim.interactionResultRollupId));

    pendingClaims.forEach(claim => {
      if (claim.interactionResultRollupId) {
        return;
      }
      claim.interactionResultRollupId = (claim.interactionNonce + 1) * 32;
    });

    // now set the odds to be ready to rollup
    await rollupDb.updateClaimsWithResultRollupId(1, 64);

    // now, all claims in pending claims should be ready to rollup
    expect(await rollupDb.getClaimsToRollup()).toEqual(pendingClaims);

    // now confirm the first claim
    await rollupDb.confirmClaimed(pendingClaims[0].nullifier, new Date());

    // should no longer be ready to rollup
    expect(await rollupDb.getClaimsToRollup()).toEqual(pendingClaims.slice(1));
  });

  it('should delete unsettled claim txs', async () => {
    const claimedTxs: TxDao[] = [];
    const unclaimedTxs: TxDao[] = [];
    for (let i = 0; i < 8; ++i) {
      const claim = randomClaim();
      const tx = randomTx();
      tx.nullifier1 = claim.nullifier;
      if (i % 2) {
        tx.mined = new Date();
        claim.claimed = tx.mined;
        claimedTxs.push(tx);
      } else {
        unclaimedTxs.push(tx);
      }
      await rollupDb.addClaim(claim);
      await rollupDb.addTx(tx);
    }

    const txs = await rollupDb.getPendingTxs();
    expect(txs).toEqual(
      [...claimedTxs, ...unclaimedTxs].sort((a, b) => (a.created.getTime() > b.created.getTime() ? 1 : -1)),
    );

    await rollupDb.deleteUnsettledClaimTxs();

    const saved = await rollupDb.getPendingTxs();
    expect(saved).toEqual(claimedTxs.sort((a, b) => (a.created.getTime() > b.created.getTime() ? 1 : -1)));
  });

  it('should get unsettled accounts', async () => {
    const accountKeys = [...Array(4)].map(() => GrumpkinAddress.random());
    const aliasHashes = [...Array(2)].map(() => AliasHash.random());
    const txs = [
      randomAccountTx({ accountPublicKey: accountKeys[0], aliasHash: aliasHashes[0] }),
      randomTx({ txType: TxType.DEPOSIT }),
      randomAccountTx({ accountPublicKey: accountKeys[0], aliasHash: aliasHashes[0], addKey: true }),
      randomAccountTx({ accountPublicKey: accountKeys[1], aliasHash: aliasHashes[0], migrate: true }), // settled
      randomTx({ txType: TxType.DEPOSIT }), // settled
      randomAccountTx({ accountPublicKey: accountKeys[2], aliasHash: aliasHashes[1] }), // rollup created
      randomTx({ txType: TxType.DEPOSIT }), // rollup created
      randomTx({ txType: TxType.TRANSFER }),
      randomAccountTx({ accountPublicKey: accountKeys[3], aliasHash: aliasHashes[1], migrate: true }),
      randomAccountTx({ accountPublicKey: accountKeys[3], aliasHash: aliasHashes[1], addKey: true }),
      randomTx({ txType: TxType.DEFI_CLAIM }),
    ];
    await rollupDb.addTxs(txs);

    const rollupProof0 = randomRollupProof([txs[3], txs[4]], 0);
    await rollupDb.addRollupProof(rollupProof0);

    const rollupProof1 = randomRollupProof([txs[5], txs[6]], 0);
    await rollupDb.addRollupProof(rollupProof1);

    const rollup = randomRollup(0, rollupProof0);
    await rollupDb.addRollup(rollup);
    const settledTxIds = rollupProof0.txs.map(tx => tx.id);
    await rollupDb.confirmMined(
      rollup.id,
      0,
      0n,
      new Date(),
      TxHash.random(),
      [],
      settledTxIds,
      [],
      [],
      randomBytes(32),
    );

    const expectedAccounts = [txs[8], txs[5], txs[0]].map(txDaoToAccountDao);
    expect(await rollupDb.getUnsettledAccounts()).toEqual(expectedAccounts);
  });

  it('should delete txs by id', async () => {
    const txs = Array.from({ length: 20 }).map(() => randomTx());
    for (const tx of txs) {
      await rollupDb.addTx(tx);
    }
    const pendingTxs = await rollupDb.getPendingTxs();
    expect(pendingTxs).toEqual(txs.sort((a, b) => (a.created.getTime() > b.created.getTime() ? 1 : -1)));

    const idsToDelete = [txs[4], txs[7], txs[12], txs[15], txs[16], txs[19]].map(tx => tx.id);
    await rollupDb.deleteTxsById(idsToDelete);
    const newPendingTxs = await rollupDb.getPendingTxs();
    const expectedTxs = txs.filter(tx => !idsToDelete.some(id => tx.id.equals(id)));
    expect(newPendingTxs).toEqual(expectedTxs.sort((a, b) => (a.created.getTime() > b.created.getTime() ? 1 : -1)));
  });
});
