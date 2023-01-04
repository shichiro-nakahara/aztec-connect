import { EthAddress } from '@aztec/barretenberg/address';
import {
  Blockchain,
  BlockchainStatus,
  EthereumProvider,
  Receipt,
  SendTxOptions,
  TxHash,
  TypedData,
} from '@aztec/barretenberg/blockchain';
import { Block } from '@aztec/barretenberg/block_source';
import { createLogger } from '@aztec/barretenberg/log';
import { BridgeCallData } from '@aztec/barretenberg/bridge_call_data';
import { InitHelpers } from '@aztec/barretenberg/environment';
import { RollupProofData } from '@aztec/barretenberg/rollup_proof';
import { Timer } from '@aztec/barretenberg/timer';
import { WorldStateConstants } from '@aztec/barretenberg/world_state';
import { EventEmitter } from 'events';
import { Contracts } from './contracts/index.js';
import { validateSignature } from './validate_signature.js';

export interface EthereumBlockchainConfig {
  console?: boolean;
  gasLimit?: number;
  minConfirmation?: number;
  minConfirmationEHW?: number;
  pollInterval?: number;
}

/**
 * Implementation of primary blockchain interface.
 * Provides higher level functionality above directly interfacing with contracts, e.g.:
 * - An asynchronous interface for subscribing to rollup events.
 * - A status query method for providing a complete snapshot of current rollup blockchain state.
 * - Abstracts away chain re-org concerns by ensuring appropriate confirmations for given situations.
 */
export class EthereumBlockchain extends EventEmitter implements Blockchain {
  private running = false;
  private runningPromise?: Promise<void>;
  private latestEthBlock = -1;
  private latestRollupId = -1;
  private status!: BlockchainStatus;
  private log = createLogger('EthereumBlockchain');

  private static readonly DEFAULT_MIN_CONFIRMATIONS = 3;
  private static readonly DEFAULT_MIN_CONFIRMATIONS_EHW = 12;

  constructor(private config: EthereumBlockchainConfig, private contracts: Contracts) {
    super();
    if (config.console === false) {
      this.log = () => {};
    }
  }

  static async new(
    config: EthereumBlockchainConfig,
    rollupContractAddress: EthAddress,
    permitHelperContractAddress: EthAddress,
    bridgeDataProvider: EthAddress,
    priceFeedContractAddresses: EthAddress[],
    provider: EthereumProvider,
  ) {
    const confirmations = config.minConfirmation || EthereumBlockchain.DEFAULT_MIN_CONFIRMATIONS;
    const contracts = await Contracts.fromAddresses(
      rollupContractAddress,
      permitHelperContractAddress,
      priceFeedContractAddresses,
      bridgeDataProvider,
      provider,
      confirmations,
    );
    const eb = new EthereumBlockchain(config, contracts);
    await eb.init();
    return eb;
  }

  public getProvider() {
    return this.contracts.getProvider();
  }

  /**
   * Initialises the status object. Requires querying for the latest rollup block from the blockchain.
   * Once we have the latest block we can populate the correct state variables.
   */
  public async init() {
    this.log('Initializing blockchain status...');
    this.status = {
      chainId: await this.contracts.getChainId(),
      rollupContractAddress: this.contracts.getRollupContractAddress(),
      permitHelperContractAddress: this.contracts.getPermitHelperContractAddress(),
      verifierContractAddress: await this.contracts.getVerifierContractAddress(),
      bridgeDataProvider: this.contracts.getBridgeDataProviderAddress(),
      ...(await this.getPerRollupState()),
      ...(await this.getPerEthBlockState()),
    };

    this.log('Seeking latest rollup...');
    const latestBlock = await this.contracts.getRollupBlock(-1, this.getRequiredConfirmations());
    if (latestBlock) {
      this.log(`Found latest rollup id ${latestBlock.rollupId}.`);
      this.latestRollupId = latestBlock.rollupId;
    } else {
      this.log('No rollup found, assuming pristine state.');
    }

    this.status = {
      ...this.status,
      ...(await this.getPerRollupState(latestBlock)),
    };

    this.log(`Initialized.`);
  }

  /**
   * Start polling for RollupProcessed events.
   * All historical blocks will have been emitted before this function returns.
   */
  public async start(fromRollup = 0) {
    this.log(`Ethereum blockchain starting from rollup: ${fromRollup}`);

    const getBlocks = async (fromRollup: number) => {
      while (true) {
        try {
          return await this.getBlocks(fromRollup);
        } catch (err: any) {
          this.log(`getBlocks failed, will retry: ${err.message}`);
          await new Promise(resolve => setTimeout(resolve, 5000));
        }
      }
    };

    const emitBlocks = async () => {
      const latestBlock = await this.contracts.getBlockNumber().catch(err => {
        this.log(`getBlockNumber failed: ${err.code}`);
        return this.latestEthBlock;
      });
      if (latestBlock === this.latestEthBlock) {
        return;
      }
      this.latestEthBlock = latestBlock;
      await this.updatePerEthBlockState();

      const blocks = await getBlocks(fromRollup);
      if (blocks.length) {
        await this.updatePerRollupState(blocks[blocks.length - 1]);
      }

      for (const block of blocks) {
        this.log(`Block received: ${block.rollupId}`);
        this.latestRollupId = block.rollupId;
        this.emit('block', block);
        fromRollup = block.rollupId + 1;
      }
    };

    // We must have emitted all historical blocks before returning.
    await emitBlocks();

    // After which, we asynchronously kick off a polling loop for the latest blocks.
    this.running = true;
    this.runningPromise = (async () => {
      while (this.running) {
        await new Promise(resolve => setTimeout(resolve, this.config.pollInterval || 10000));
        await emitBlocks().catch(this.log);
      }
    })();

    this.log('Ethereum blockchain started.');
  }

  /**
   * Stop polling for RollupProcessed events
   */
  public async stop() {
    this.running = false;
    this.removeAllListeners();
    await this.runningPromise;
  }

  /**
   * Get the status of the rollup contract
   */
  public getBlockchainStatus() {
    return this.status;
  }

  private async getPerRollupState(block?: Block) {
    const state = await this.contracts.getPerRollupState();
    if (block) {
      const rollupProofData = RollupProofData.decode(block.encodedRollupProofData);
      return {
        ...state,
        nextRollupId: rollupProofData.rollupId + 1,
        dataSize: rollupProofData.dataStartIndex + rollupProofData.rollupSize,
        dataRoot: rollupProofData.newDataRoot,
        nullRoot: rollupProofData.newNullRoot,
        rootRoot: rollupProofData.newDataRootsRoot,
        defiRoot: rollupProofData.newDefiRoot,
      };
    } else {
      // No rollups yet.
      const chainId = await this.contracts.getChainId();
      const {
        dataTreeSize,
        roots: { dataRoot, nullRoot, rootsRoot },
      } = InitHelpers.getInitData(chainId);
      return {
        ...state,
        nextRollupId: 0,
        dataSize: dataTreeSize,
        dataRoot,
        nullRoot,
        rootRoot: rootsRoot,
        defiRoot: WorldStateConstants.EMPTY_DEFI_ROOT,
      };
    }
  }

  private async getPerEthBlockState() {
    return {
      ...(await this.contracts.getPerBlockState()),
      assets: this.contracts.getAssets(),
      bridges: await this.contracts.getSupportedBridges(),
    };
  }

  private async updatePerRollupState(block?: Block) {
    this.status = {
      ...this.status,
      ...(await this.getPerRollupState(block)),
    };
  }

  private async updatePerEthBlockState() {
    await this.contracts.updatePerEthBlockState();
    this.status = {
      ...this.status,
      ...(await this.getPerEthBlockState()),
    };
  }

  public getLatestRollupId() {
    return Promise.resolve(this.latestRollupId);
  }

  public async getUserPendingDeposit(assetId: number, account: EthAddress) {
    return await this.contracts.getUserPendingDeposit(assetId, account);
  }

  public async getUserProofApprovalStatus(account: EthAddress, txId: Buffer) {
    return await this.contracts.getUserProofApprovalStatus(account, txId);
  }

  async createRollupTxs(dataBuf: Buffer, signatures: Buffer[], offchainTxData: Buffer[], txCallDataLimit: number) {
    return await this.contracts.createRollupTxs(dataBuf, signatures, offchainTxData, txCallDataLimit);
  }

  public sendTx(tx: Buffer, options: SendTxOptions = {}) {
    options = { ...options, gasLimit: options.gasLimit || this.config.gasLimit };
    return this.contracts.sendTx(tx, options);
  }

  private getRequiredConfirmations() {
    const { escapeOpen, numEscapeBlocksRemaining } = this.status;
    const {
      minConfirmation = EthereumBlockchain.DEFAULT_MIN_CONFIRMATIONS,
      minConfirmationEHW = EthereumBlockchain.DEFAULT_MIN_CONFIRMATIONS_EHW,
    } = this.config;
    return escapeOpen || numEscapeBlocksRemaining <= minConfirmationEHW ? minConfirmationEHW : minConfirmation;
  }

  /**
   * Get all created rollup blocks from `rollupId`.
   */
  public async getBlocks(rollupId: number) {
    const minConfirmations = this.getRequiredConfirmations();
    return await this.contracts.getRollupBlocksFrom(rollupId, minConfirmations);
  }

  public async callbackRollupBlocksFrom(rollupId: number, cb: (block: Block) => Promise<void>) {
    const minConfirmations = this.getRequiredConfirmations();
    return await this.contracts.callbackRollupBlocksFrom(rollupId, minConfirmations, cb);
  }

  /**
   * Wait for given transaction to be mined, and return receipt.
   * Timeout is only considered for pending txs. i.e. If there is at least 1 confirmation, the timeout disables.
   * Timeout can be detected because Receipt blockNum will be undefined.
   */
  public async getTransactionReceipt(
    txHash: TxHash,
    timeoutSeconds?: number,
    confs = this.config.minConfirmation || EthereumBlockchain.DEFAULT_MIN_CONFIRMATIONS,
  ): Promise<Receipt> {
    const timer = new Timer();
    this.log(`Getting tx receipt for ${txHash}... (${confs} confs)`);

    let tx = await this.contracts.getTransactionByHash(txHash);
    while (!tx) {
      if (timeoutSeconds !== undefined && timer.s() > timeoutSeconds) {
        return { status: false };
      }
      await new Promise(resolve => setTimeout(resolve, 1000));
      tx = await this.contracts.getTransactionByHash(txHash);
    }

    let txReceipt = await this.contracts.getTransactionReceipt(txHash);
    while (!txReceipt || txReceipt.confirmations < confs) {
      await new Promise(resolve => setTimeout(resolve, 1000));
      txReceipt = await this.contracts.getTransactionReceipt(txHash);
    }
    const receipt: Receipt = { status: !!txReceipt.status, blockNum: txReceipt.blockNumber };
    if (!receipt.status) {
      receipt.revertError = await this.contracts.getRevertError(txHash);
    }
    return receipt;
  }

  public async getTransactionReceiptSafe(txHash: TxHash, timeoutSeconds?: number) {
    const confs = this.getRequiredConfirmations();
    return await this.getTransactionReceipt(txHash, timeoutSeconds, confs);
  }

  /**
   * Validate locally that a signature was produced by a publicOwner
   */
  public validateSignature(publicOwner: EthAddress, signature: Buffer, signingData: Buffer) {
    return validateSignature(publicOwner, signature, signingData);
  }

  public async signPersonalMessage(message: Buffer, address: EthAddress) {
    return await this.contracts.signPersonalMessage(message, address);
  }

  public async signMessage(message: Buffer, address: EthAddress) {
    return await this.contracts.signMessage(message, address);
  }

  public async signTypedData(data: TypedData, address: EthAddress) {
    return await this.contracts.signTypedData(data, address);
  }

  public getAsset(assetId: number) {
    return this.contracts.getAsset(assetId);
  }

  public async getAssetPrice(assetId: number) {
    return await this.contracts.getAssetPrice(assetId);
  }

  public getPriceFeed(assetId: number) {
    return this.contracts.getPriceFeed(assetId);
  }

  public getGasPriceFeed() {
    return this.contracts.getGasPriceFeed();
  }

  public async isContract(address: EthAddress) {
    return await this.contracts.isContract(address);
  }

  public async isEmpty(address: EthAddress) {
    return await this.contracts.isEmpty(address);
  }

  public async estimateGas(data: Buffer) {
    return await this.contracts.estimateGas(data);
  }

  public async getChainId() {
    return await this.contracts.getChainId();
  }

  public getRollupBalance(assetId: number) {
    return this.contracts.getRollupBalance(assetId);
  }

  public async getFeeData() {
    return await this.contracts.getFeeData();
  }

  public getBridgeGas(bridgeCallData: bigint) {
    const { bridgeAddressId } = BridgeCallData.fromBigInt(bridgeCallData);
    const { gasLimit } = this.status.bridges.find(bridge => bridge.id == bridgeAddressId) || {};
    if (!gasLimit) {
      throw new Error(`Failed to retrieve bridge cost for bridge ${bridgeCallData.toString()}`);
    }
    return gasLimit;
  }

  public async getBridgeSubsidy(bridgeCallData: bigint) {
    return await this.contracts.getBridgeSubsidy(bridgeCallData);
  }

  public async getBridgeData(bridgeAddressId: number) {
    return await this.contracts.getBridgeData(bridgeAddressId);
  }
}
