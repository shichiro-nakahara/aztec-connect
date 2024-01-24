import { Blockchain, PriceFeed } from '@polyaztec/barretenberg/blockchain';
import { EthPriceFeed } from '@polyaztec/blockchain';
import { PriceTracker } from './price_tracker.js';
import { jest } from '@jest/globals';

type Mockify<T> = {
  [P in keyof T]: ReturnType<typeof jest.fn>;
};

describe('price tracker', () => {
  const assetIds = [0, 1];
  const refreshInterval = 500;
  const recordDuration = 1000;
  const initialTime = 1618222000000;
  let dateSpy: any;
  let gasPriceFeed: Mockify<PriceFeed>;
  let tokenPriceFeed: Mockify<PriceFeed>;
  let blockchain: Mockify<Blockchain>;
  let priceTracker!: PriceTracker;

  beforeEach(() => {
    dateSpy = jest.spyOn(Date, 'now').mockReturnValue(initialTime);

    gasPriceFeed = {
      price: jest.fn<() => Promise<bigint>>().mockResolvedValue(123n),
    } as Mockify<PriceFeed>;

    tokenPriceFeed = {
      price: jest.fn<() => Promise<bigint>>().mockResolvedValue(456n),
    } as Mockify<PriceFeed>;

    blockchain = {
      getGasPriceFeed: jest.fn().mockReturnValue(gasPriceFeed),
      getPriceFeed: jest.fn((assetId: number) => {
        if (assetId === 0) {
          return new EthPriceFeed();
        }
        return tokenPriceFeed;
      }),
    } as Mockify<Blockchain>;

    priceTracker = new PriceTracker(blockchain, assetIds, refreshInterval, recordDuration);
  });

  afterEach(async () => {
    dateSpy.mockRestore();
    await priceTracker.stop();
  });

  it('new price tracker with dai only', async () => {
    const tempTracker = new PriceTracker(blockchain, [1], refreshInterval, recordDuration);
    await tempTracker.start();
    expect(tempTracker.getAssetPrice(1)).toBe(456n);
  });

  it('record prices', async () => {
    await priceTracker.start();
    expect(priceTracker.getGasPrice()).toBe(123n);
    expect(priceTracker.getAssetPrice(0)).toBe(10n ** 18n);
    expect(priceTracker.getAssetPrice(1)).toBe(456n);
  });

  it('record and get historical prices', async () => {
    await priceTracker.start();
    gasPriceFeed.price.mockResolvedValue(789n);
    tokenPriceFeed.price.mockResolvedValue(876n);
    dateSpy.mockImplementation(() => initialTime + refreshInterval);
    await new Promise(resolve => setTimeout(resolve, refreshInterval));

    expect(priceTracker.getGasPrice()).toBe(789n);
    expect(priceTracker.getAssetPrice(0)).toBe(10n ** 18n);
    expect(priceTracker.getAssetPrice(1)).toBe(876n);

    const msAgo = refreshInterval;
    expect(priceTracker.getGasPrice(msAgo)).toBe(123n);
    expect(priceTracker.getAssetPrice(0, msAgo)).toBe(10n ** 18n);
    expect(priceTracker.getAssetPrice(1, msAgo)).toBe(456n);
  });

  it('return 0 if historical price is not set', async () => {
    await priceTracker.start();
    gasPriceFeed.price.mockResolvedValue(789n);
    tokenPriceFeed.price.mockResolvedValue(876n);
    dateSpy.mockImplementation(() => initialTime + refreshInterval);
    await new Promise(resolve => setTimeout(resolve, refreshInterval));

    const msAgo = refreshInterval + 1;
    expect(priceTracker.getGasPrice(msAgo)).toBe(0n);
    expect(priceTracker.getAssetPrice(0, msAgo)).toBe(0n);
    expect(priceTracker.getAssetPrice(1, msAgo)).toBe(0n);
  });

  it('return 0 if not started yet', () => {
    expect(priceTracker.getGasPrice()).toBe(0n);
    expect(priceTracker.getAssetPrice(0)).toBe(0n);
    expect(priceTracker.getAssetPrice(1)).toBe(0n);
  });

  it('return min historical price within record duration', async () => {
    expect(priceTracker.getMinGasPrice()).toBe(0n);
    expect(priceTracker.getMinAssetPrice(1)).toBe(0n);

    gasPriceFeed.price.mockResolvedValue(10n);
    tokenPriceFeed.price.mockResolvedValue(3n);
    await priceTracker.start();

    expect(priceTracker.getMinGasPrice()).toBe(10n);
    expect(priceTracker.getMinAssetPrice(1)).toBe(3n);

    gasPriceFeed.price.mockResolvedValue(30n);
    tokenPriceFeed.price.mockResolvedValue(2n);
    await new Promise(resolve => setTimeout(resolve, refreshInterval));

    expect(priceTracker.getMinGasPrice()).toBe(10n);
    expect(priceTracker.getMinAssetPrice(1)).toBe(2n);

    gasPriceFeed.price.mockResolvedValue(20n);
    tokenPriceFeed.price.mockResolvedValue(4n);
    await new Promise(resolve => setTimeout(resolve, refreshInterval));

    expect(priceTracker.getMinGasPrice()).toBe(10n);
    expect(priceTracker.getMinAssetPrice(1)).toBe(2n);

    gasPriceFeed.price.mockResolvedValue(40n);
    tokenPriceFeed.price.mockResolvedValue(3n);
    await new Promise(resolve => setTimeout(resolve, refreshInterval));

    expect(priceTracker.getMinGasPrice()).toBe(20n);
    expect(priceTracker.getMinAssetPrice(1)).toBe(2n);
  });
});
