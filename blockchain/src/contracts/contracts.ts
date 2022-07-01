import { EthAddress } from '@aztec/barretenberg/address';
import {
  Asset,
  EthereumProvider,
  EthereumRpc,
  FeeData,
  PriceFeed,
  SendTxOptions,
  TxHash,
  TypedData,
} from '@aztec/barretenberg/blockchain';
import { Web3Provider } from '@ethersproject/providers';
import { Web3Signer } from '../signer';
import { EthAsset, TokenAsset } from './asset';
import { EthPriceFeed, GasPriceFeed, TokenPriceFeed } from './price_feed';
import { RollupProcessor } from './rollup_processor';

/**
 * Facade around all Aztec smart contract classes.
 * Provides a factory function `fromAddresses` to simplify construction of all contract classes.
 * Exposes a more holistic interface to clients, than having to deal with individual contract classes.
 */
export class Contracts {
  private readonly provider!: Web3Provider;
  private readonly ethereumRpc!: EthereumRpc;

  constructor(
    private readonly rollupProcessor: RollupProcessor,
    private assets: Asset[],
    private readonly gasPriceFeed: GasPriceFeed,
    private readonly priceFeeds: PriceFeed[],
    private readonly ethereumProvider: EthereumProvider,
    private readonly confirmations: number,
  ) {
    this.provider = new Web3Provider(ethereumProvider);
    this.ethereumRpc = new EthereumRpc(ethereumProvider);
  }

  static async fromAddresses(
    rollupContractAddress: EthAddress,
    priceFeedContractAddresses: EthAddress[],
    ethereumProvider: EthereumProvider,
    confirmations: number,
  ) {
    const rollupProcessor = new RollupProcessor(rollupContractAddress, ethereumProvider);

    const assets = [new EthAsset(ethereumProvider)];

    const [gasPriceFeedAddress, ...tokenPriceFeedAddresses] = priceFeedContractAddresses;
    const gasPriceFeed = new GasPriceFeed(gasPriceFeedAddress, ethereumProvider);
    const priceFeeds = [
      new EthPriceFeed(),
      ...tokenPriceFeedAddresses.map(a => new TokenPriceFeed(a, ethereumProvider)),
    ];

    const contracts = new Contracts(rollupProcessor, assets, gasPriceFeed, priceFeeds, ethereumProvider, confirmations);

    await contracts.updateAssets();
    return contracts;
  }

  public getProvider() {
    return this.ethereumProvider;
  }

  public async updateAssets() {
    const supportedAssets = await this.rollupProcessor.getSupportedAssets();
    const newAssets = await Promise.all(
      supportedAssets
        .slice(this.assets.length - 1)
        .map(({ address, gasLimit }) =>
          TokenAsset.fromAddress(address, this.ethereumProvider, gasLimit, this.confirmations),
        ),
    );
    this.assets = [...this.assets, ...newAssets];
  }

  public async getPerRollupState() {
    const defiInteractionHashes = await this.rollupProcessor.defiInteractionHashes();

    return {
      defiInteractionHashes,
    };
  }

  public async getPerBlockState() {
    const { escapeOpen, blocksRemaining } = await this.rollupProcessor.getEscapeHatchStatus();
    const allowThirdPartyContracts = await this.rollupProcessor.getThirdPartyContractStatus();

    return {
      escapeOpen,
      numEscapeBlocksRemaining: blocksRemaining,
      allowThirdPartyContracts,
    };
  }

  public getRollupBalance(assetId: number) {
    return this.assets[assetId].balanceOf(this.rollupProcessor.address);
  }

  public getRollupContractAddress() {
    return this.rollupProcessor.address;
  }

  public async getVerifierContractAddress() {
    return await this.rollupProcessor.verifier();
  }

  async createRollupTxs(dataBuf: Buffer, signatures: Buffer[], offchainTxData: Buffer[], txCallDataLimit: number) {
    return await this.rollupProcessor.createRollupTxs(dataBuf, signatures, offchainTxData, txCallDataLimit);
  }

  public async sendTx(data: Buffer, options: SendTxOptions = {}) {
    return await this.rollupProcessor.sendTx(data, options);
  }

  public async estimateGas(data: Buffer) {
    return await this.rollupProcessor.estimateGas(data);
  }

  public async getRollupBlocksFrom(rollupId: number, minConfirmations = this.confirmations) {
    return await this.rollupProcessor.getRollupBlocksFrom(rollupId, minConfirmations);
  }

  public async getRollupBlock(rollupId: number) {
    return await this.rollupProcessor.getRollupBlock(rollupId);
  }

  public async getUserPendingDeposit(assetId: number, account: EthAddress) {
    return await this.rollupProcessor.getUserPendingDeposit(assetId, account);
  }

  public async getTransactionByHash(txHash: TxHash) {
    return await this.ethereumRpc.getTransactionByHash(txHash);
  }

  public async getTransactionReceipt(txHash: TxHash) {
    return await this.provider.getTransactionReceipt(txHash.toString());
  }

  public async getChainId() {
    const { chainId } = await this.provider.getNetwork();
    return chainId;
  }

  public async getBlockNumber() {
    return await this.provider.getBlockNumber();
  }

  public async signPersonalMessage(message: Buffer, address: EthAddress) {
    const signer = new Web3Signer(this.ethereumProvider);
    return await signer.signPersonalMessage(message, address);
  }

  public async signMessage(message: Buffer, address: EthAddress) {
    const signer = new Web3Signer(this.ethereumProvider);
    return await signer.signMessage(message, address);
  }

  public async signTypedData(data: TypedData, address: EthAddress) {
    const signer = new Web3Signer(this.ethereumProvider);
    return await signer.signTypedData(data, address);
  }

  public getAsset(assetId: number) {
    return this.assets[assetId];
  }

  public async getAssetPrice(assetId: number) {
    return await this.priceFeeds[assetId].price();
  }

  public getPriceFeed(assetId: number) {
    if (!this.priceFeeds[assetId]) {
      throw new Error(`Unknown assetId: ${assetId}`);
    }
    return this.priceFeeds[assetId];
  }

  public getGasPriceFeed() {
    return this.gasPriceFeed;
  }

  public async getUserProofApprovalStatus(address: EthAddress, txId: Buffer) {
    return await this.rollupProcessor.getProofApprovalStatus(address, txId);
  }

  public async isContract(address: EthAddress) {
    return (await this.provider.getCode(address.toString())) !== '0x';
  }

  public async isEmpty(address: EthAddress) {
    return (
      !(await this.isContract(address)) &&
      (await this.provider.getBalance(address.toString())).toBigInt() == BigInt(0) &&
      (await this.provider.getTransactionCount(address.toString())) == 0
    );
  }

  public async getFeeData(): Promise<FeeData> {
    const { maxFeePerGas, maxPriorityFeePerGas, gasPrice } = await this.provider.getFeeData();
    return {
      maxFeePerGas: maxFeePerGas !== null ? BigInt(maxFeePerGas.toString()) : BigInt(0),
      maxPriorityFeePerGas: maxPriorityFeePerGas !== null ? BigInt(maxPriorityFeePerGas.toString()) : BigInt(0),
      gasPrice: gasPrice !== null ? BigInt(gasPrice.toString()) : BigInt(0),
    };
  }

  public getAssets() {
    return this.assets.map(a => a.getStaticInfo());
  }

  public async getSupportedBridges() {
    return await this.rollupProcessor.getSupportedBridges();
  }

  public async getRevertError(txHash: TxHash) {
    return await this.rollupProcessor.getRevertError(txHash);
  }
}
