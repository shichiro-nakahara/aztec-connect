import { EthAddress } from 'barretenberg/address';
import { Block } from 'barretenberg/block_source';
import { nullifierBufferToIndex } from 'barretenberg/client_proofs/join_split_proof';
import { MemoryFifo } from 'barretenberg/fifo';
import { existsAsync, readFileAsync, writeFileAsync, rmdirAsync } from 'barretenberg/fs_async';
import { HashPath } from 'barretenberg/merkle_tree';
import { RollupProofData } from 'barretenberg/rollup_proof';
import { WorldStateDb } from 'barretenberg/world_state_db';
import { toBufferBE } from 'bigint-buffer';
import { Blockchain } from 'blockchain';
import { GetHashPathsResponse, HashPathSource } from './hash_path_source';

interface ServerState {
  lastBlock: number;
  rollupContractAddress: EthAddress;
}

export default class Server implements HashPathSource {
  private queue = new MemoryFifo<() => Promise<void>>();
  private serverState: ServerState = { lastBlock: -1, rollupContractAddress: EthAddress.ZERO };

  public constructor(private worldStateDb: WorldStateDb, private blockchain: Blockchain) {}

  public async start() {
    console.log('Synchronising chain state...');

    await this.readState();
    await this.worldStateDb.start();

    // Processing all historical blocks.
    let blocks = await this.blockchain.getBlocks(this.serverState.lastBlock + 1);
    while (blocks.length) {
      for (const block of blocks) {
        await this.handleBlock(block);
      }
      blocks = await this.blockchain.getBlocks(this.serverState.lastBlock + 1);
    }

    await this.writeState();

    // Subscribe for new blocks.
    this.blockchain.on('block', (b: Block) =>
      this.queue.put(async () => {
        await this.handleBlock(b);
        this.printState();
      }),
    );
    this.blockchain.start(this.serverState.lastBlock + 1);

    this.queue.process(fn => fn());

    this.printState();
  }

  public async stop() {
    this.queue.cancel();
    this.blockchain.stop();
  }

  private async readState() {
    if (await existsAsync('./data/state')) {
      const state = await readFileAsync('./data/state');
      const { lastBlock, rollupContractAddress } = JSON.parse(state.toString('utf-8'));
      this.serverState = { lastBlock, rollupContractAddress: EthAddress.fromString(rollupContractAddress) };

      // if rollupContractAddress has changed, wipe the data dir and initiate full re-sync
      const providedContractAddress = this.blockchain.getRollupContractAddress();
      if (this.serverState.rollupContractAddress.toString() !== providedContractAddress.toString()) {
        await rmdirAsync('./data', { recursive: true });
        this.serverState.rollupContractAddress = providedContractAddress;
        this.serverState.lastBlock = -1;
      }
    }
  }

  private async writeState() {
    const dataToWrite = {
      lastBlock: this.serverState.lastBlock,
      rollupContractAddress: this.serverState.rollupContractAddress.toString(),
    };
    await writeFileAsync('./data/state', JSON.stringify(dataToWrite));
  }

  public async getStatus() {
    const status = await this.blockchain.getStatus();
    return { ...status, serviceName: 'sriracha' };
  }

  public async getTreeState(treeIndex: number) {
    const size = this.worldStateDb.getSize(treeIndex);
    const root = this.worldStateDb.getRoot(treeIndex);
    return { size, root };
  }

  public async getHashPath(treeIndex: number, index: bigint) {
    return new Promise<HashPath>(resolve => {
      this.queue.put(async () => resolve(await this.worldStateDb.getHashPath(treeIndex, index)));
    });
  }

  public async getHashPaths(treeIndex: number, additions: { index: bigint; value: Buffer }[]) {
    return new Promise<GetHashPathsResponse>(resolve => {
      this.queue.put(async () => resolve(await this.computeTempHashPaths(treeIndex, additions)));
    });
  }

  public async computeTempHashPaths(treeIndex: number, additions: { index: bigint; value: Buffer }[]) {
    const oldHashPaths: HashPath[] = [];
    const newHashPaths: HashPath[] = [];
    const newRoots: Buffer[] = [];
    const oldRoot: Buffer = this.worldStateDb.getRoot(treeIndex);

    for (const { index, value } of additions) {
      const oldHashPath = await this.worldStateDb.getHashPath(treeIndex, index);
      oldHashPaths.push(oldHashPath);
      await this.worldStateDb.put(treeIndex, index, value);
      const newHashPath = await this.worldStateDb.getHashPath(treeIndex, index);
      newHashPaths.push(newHashPath);
      newRoots.push(this.worldStateDb.getRoot(treeIndex));
    }

    await this.worldStateDb.rollback();

    return { oldHashPaths, newHashPaths, newRoots, oldRoot };
  }

  private async handleBlock(block: Block) {
    const { rollupSize, rollupProofData, viewingKeysData, rollupId } = block;
    const { dataStartIndex, innerProofData } = RollupProofData.fromBuffer(rollupProofData, viewingKeysData);

    console.log(`Processing rollup ${rollupId}...`);

    for (let i = 0; i < innerProofData.length; ++i) {
      const tx = innerProofData[i];
      await this.worldStateDb.put(0, BigInt(dataStartIndex + i * rollupSize), tx.newNote1);
      await this.worldStateDb.put(0, BigInt(dataStartIndex + i * rollupSize + 1), tx.newNote2);
      await this.worldStateDb.put(1, nullifierBufferToIndex(tx.nullifier1), toBufferBE(1n, 64));
      await this.worldStateDb.put(1, nullifierBufferToIndex(tx.nullifier2), toBufferBE(1n, 64));
    }
    if (innerProofData.length < rollupSize) {
      await this.worldStateDb.put(0, BigInt(dataStartIndex + rollupSize * 2 - 1), Buffer.alloc(64, 0));
    }
    await this.worldStateDb.put(2, BigInt(rollupId + 1), this.worldStateDb.getRoot(0));

    await this.worldStateDb.commit();

    this.serverState.lastBlock = rollupId;
    await this.writeState();
  }

  private printState() {
    console.log(`Data size: ${this.worldStateDb.getSize(0)}`);
    console.log(`Data root: ${this.worldStateDb.getRoot(0).toString('hex')}`);
    console.log(`Null root: ${this.worldStateDb.getRoot(1).toString('hex')}`);
    console.log(`Root root: ${this.worldStateDb.getRoot(2).toString('hex')}`);
  }
}
