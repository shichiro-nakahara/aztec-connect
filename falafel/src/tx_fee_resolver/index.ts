import { AssetId } from '@aztec/barretenberg/asset';
import { Blockchain, BlockchainAsset, TxType } from '@aztec/barretenberg/blockchain';
import { AssetFeeQuote } from '@aztec/barretenberg/rollup_provider';
import { TxDao } from '../entity/tx';
import { FeeCalculator } from './fee_calculator';
import { PriceTracker } from './price_tracker';

export class TxFeeResolver {
  private assets!: BlockchainAsset[];
  private priceTracker!: PriceTracker;
  private feeCalculator!: FeeCalculator;

  constructor(
    private readonly blockchain: Blockchain,
    private baseTxGas: number,
    private maxFeeGasPrice: bigint,
    private feeGasPriceMultiplier: number,
    private readonly txsPerRollup: number,
    private publishInterval: number,
    private readonly surplusRatios = [1, 0.9, 0.5, 0],
    private readonly feeFreeAssets: AssetId[] = [],
    private readonly freeTxTypes = [TxType.ACCOUNT],
    private readonly numSignificantFigures = 2,
    private readonly refreshInterval = 5 * 60 * 1000, // 5 mins
    private readonly minFeeDuration = refreshInterval * 2, // 10 mins
  ) {}

  public setConf(
    baseTxGas: number,
    maxFeeGasPrice: bigint,
    feeGasPriceMultiplier: number,
    publishInterval: number,
  ) {
    this.baseTxGas = baseTxGas;
    this.maxFeeGasPrice = maxFeeGasPrice;
    this.feeGasPriceMultiplier = feeGasPriceMultiplier;
    this.publishInterval = publishInterval;
  }

  async start() {
    const { assets } = await this.blockchain.getBlockchainStatus();
    this.assets = assets;
    const assetIds = assets.map((_, id) => id);
    this.priceTracker = new PriceTracker(this.blockchain, assetIds, this.refreshInterval, this.minFeeDuration);
    this.feeCalculator = new FeeCalculator(
      this.priceTracker,
      this.assets,
      this.baseTxGas,
      this.maxFeeGasPrice,
      this.feeGasPriceMultiplier,
      this.txsPerRollup,
      this.publishInterval,
      this.surplusRatios,
      this.feeFreeAssets,
      this.freeTxTypes,
      this.numSignificantFigures,
    );
    await this.priceTracker.start();
  }

  async stop() {
    await this.priceTracker?.stop();
  }

  getMinTxFee(assetId: number, txType: TxType) {
    if (!this.feeCalculator) {
      return 0n;
    }
    return this.feeCalculator.getMinTxFee(assetId, txType);
  }

  getFeeQuotes(assetId: number): AssetFeeQuote {
    if (!this.feeCalculator) {
      return { feeConstants: [], baseFeeQuotes: [] };
    }
    return this.feeCalculator.getFeeQuotes(assetId);
  }

  computeSurplusRatio(txs: TxDao[]) {
    if (!this.feeCalculator) {
      return 1;
    }
    return this.feeCalculator.computeSurplusRatio(txs);
  }

  getGasPaidForByFee(assetId: AssetId, fee: bigint) {
    if (!this.feeCalculator) {
      return 0n;
    }
    return this.feeCalculator.getGasPaidForByFee(assetId, fee);
  }

  getBaseTxGas() {
    return this.baseTxGas;
  }

  getTxGas(assetId: AssetId, txType: TxType) {
    return BigInt(this.baseTxGas) + BigInt(this.assets[assetId].gasConstants[txType]);
  }
}
