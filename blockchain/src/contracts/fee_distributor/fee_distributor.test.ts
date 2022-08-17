// eslint-disable-next-line @typescript-eslint/no-var-requires
const { solidity } = require('ethereum-waffle');
import chai from 'chai';

import { expect } from 'chai';
chai.use(solidity);

import { BigNumber, Contract, Signer } from 'ethers';
import { ethers } from 'hardhat';
import { EthAddress } from '@aztec/barretenberg/address';
import { Asset } from '@aztec/barretenberg/blockchain';
import { setupAssets } from '../asset/fixtures/setup_assets';
import { setupFeeDistributor } from './fixtures/setup_fee_distributor';
import { setupUniswap } from './fixtures/setup_uniswap';
import { FeeDistributor } from './fee_distributor';
import { evmRevert, evmSnapshot } from '../../ganache/hardhat_chain_manipulation';

describe('fee_distributor', () => {
  let feeDistributor: FeeDistributor;
  let uniswapRouter: Contract;
  let assets: Asset[];
  let signers: Signer[];
  let addresses: EthAddress[];
  let createPair: (asset: Asset, initialTotalSupply: bigint) => Promise<Contract>;
  let fakeRollupProccessor: EthAddress;

  const initialUserTokenBalance = 10n ** 18n;
  const initialTotalSupply = 10n * 10n ** 18n;

  let snapshot: string;

  before(async () => {
    signers = await ethers.getSigners();
    addresses = await Promise.all(signers.map(async u => EthAddress.fromString(await u.getAddress())));
    fakeRollupProccessor = addresses[3];
    ({ uniswapRouter, createPair } = await setupUniswap(signers[0]));
    ({ feeDistributor } = await setupFeeDistributor(
      signers[0],
      fakeRollupProccessor,
      EthAddress.fromString(uniswapRouter.address),
    ));

    assets = await setupAssets(signers[0], signers, initialUserTokenBalance);
    await Promise.all(assets.slice(1).map(a => createPair(a, initialTotalSupply)));
  });

  beforeEach(async () => {
    snapshot = await evmSnapshot();
  });

  afterEach(async () => {
    await evmRevert(snapshot);
  });

  it('deposit eth to fee distributor', async () => {
    await assets[0].transfer(100n, addresses[0], feeDistributor.address);
    expect(BigInt(await feeDistributor.txFeeBalance(EthAddress.ZERO))).to.be.eq(100n);
  });

  it('deposit token asset to fee distributor', async () => {
    const asset = assets[1];
    const assetAddress = asset.getStaticInfo().address;
    const amount = 100n;

    await asset.transfer(amount, addresses[0], feeDistributor.address);
    expect(await feeDistributor.txFeeBalance(assetAddress)).to.be.eq(amount);
  });

  it('only owner can change convertConstant', async () => {
    const convertConstant = 100n;
    expect(await feeDistributor.convertConstant()).not.to.be.eq(convertConstant);
    await expect(
      feeDistributor.setConvertConstant(convertConstant, { signingAddress: addresses[1] }),
    ).to.be.revertedWith('Ownable: caller is not the owner');
    await feeDistributor.setConvertConstant(convertConstant);
    expect(BigInt(await feeDistributor.convertConstant())).to.be.eq(convertConstant);
  });

  it('only owner can change feeClaimer', async () => {
    const userAddress = addresses[1];
    expect(await feeDistributor.aztecFeeClaimer()).not.to.be.eq(userAddress);
    await expect(feeDistributor.setFeeClaimer(userAddress, { signingAddress: addresses[1] })).to.be.revertedWith(
      'Ownable: caller is not the owner',
    );
    await feeDistributor.setFeeClaimer(userAddress);
    expect((await feeDistributor.aztecFeeClaimer()).toString()).to.be.eq(userAddress.toString());
  });

  it('reimburse eth to fee claimer if fee claimer is below threshold', async () => {
    const ethAsset = assets[0];
    const userAddress = addresses[1];
    const initialFeeDistributorBalance = 10n ** 18n;
    const toSend = 1000n;
    const feeLimit = await feeDistributor.feeLimit();
    const { maxFeePerGas } = await signers[0].getFeeData();
    const gasPrice = maxFeePerGas!.toBigInt();

    const initialUserBalance = await ethAsset.balanceOf(userAddress);

    // simulate a rollup, paying the feeDistributor
    await ethAsset.transfer(toSend, fakeRollupProccessor, feeDistributor.address);
    expect(initialUserBalance).to.be.eq(await ethAsset.balanceOf(userAddress));

    // drain the fee claimer address
    await ethAsset.transfer(initialUserBalance - 25000n * gasPrice, userAddress, EthAddress.ZERO);
    const drainedUserBalance = await ethAsset.balanceOf(userAddress);
    expect(BigNumber.from(drainedUserBalance)).to.be.lt(BigNumber.from(await feeDistributor.feeLimit()));

    await feeDistributor.setFeeClaimer(userAddress);
    expect((await feeDistributor.aztecFeeClaimer()).toString()).to.be.eq(userAddress.toString());

    await ethAsset.transfer(initialFeeDistributorBalance, addresses[0], feeDistributor.address);

    // simulate a rollup, paying the feeDistributor
    await ethAsset.transfer(toSend, fakeRollupProccessor, feeDistributor.address);

    expect(await ethAsset.balanceOf(feeDistributor.address)).to.be.eq(
      initialFeeDistributorBalance - feeLimit + toSend + toSend,
    );
    const expectedBalance =
      drainedUserBalance + // starting balance
      feeLimit; // feeLimit balance as now transfered;

    expect(await ethAsset.balanceOf(userAddress)).to.be.eq(expectedBalance);
  });

  it('convert asset balance to eth', async () => {
    const asset = assets[1];
    const assetAddr = asset.getStaticInfo().address;

    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(0n);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);

    const balance = 10n;

    await asset.transfer(balance, addresses[0], feeDistributor.address);

    const fee = 1n;
    const minOutputValue = 9n;
    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(balance);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);
    await feeDistributor.convert(assetAddr, minOutputValue);

    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(0n);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(balance - fee);
  });

  it('convert weth balance to eth', async () => {
    const weth = await ethers.getContractAt('IWETH', (await feeDistributor.WETH()).toString(), signers[0]);

    const balance = 10n;
    await weth.deposit({ value: balance.toString() });

    expect(await feeDistributor.txFeeBalance(EthAddress.fromString(weth.address))).to.be.eq(0n);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);

    await weth.transfer(feeDistributor.address.toString(), balance);

    const minOutputValue = 9n;
    expect(await feeDistributor.txFeeBalance(EthAddress.fromString(weth.address))).to.be.eq(balance);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);

    await feeDistributor.convert(EthAddress.fromString(weth.address), minOutputValue);

    expect(await feeDistributor.txFeeBalance(EthAddress.fromString(weth.address))).to.be.eq(0n);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(balance);
  });

  it('revert if non-owner tries to convert asset balance to eth', async () => {
    const asset = assets[1];
    const assetAddr = asset.getStaticInfo().address;

    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(0n);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);

    const balance = 10n;

    await asset.transfer(balance, addresses[0], feeDistributor.address);

    const minOutputValue = 9n;
    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(balance);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);
    await expect(
      feeDistributor.convert(assetAddr, minOutputValue, { signingAddress: addresses[1] }),
    ).to.be.revertedWith('Ownable: caller is not the owner');

    expect(await feeDistributor.txFeeBalance(assetAddr)).to.be.eq(balance);
    expect(await feeDistributor.txFeeBalance(EthAddress.ZERO)).to.be.eq(0n);
  });

  it('revert if output will be less than minOutputValue', async () => {
    const asset = assets[1];
    const assetAddr = assets[1].getStaticInfo().address;
    const balance = 10n;

    await asset.transfer(balance, addresses[0], feeDistributor.address);

    const minOutputValue = balance;
    await expect(feeDistributor.convert(assetAddr, minOutputValue)).to.be.revertedWith('INSUFFICIENT_OUTPUT_AMOUNT');
  });

  it('cannot convert eth to eth', async () => {
    await expect(feeDistributor.convert(EthAddress.ZERO, 0n)).to.be.revertedWith('NOT_A_TOKEN_ASSET');
  });

  it('revert convert if asset balance is empty', async () => {
    const assetAddr = assets[1].getStaticInfo().address;
    await expect(feeDistributor.convert(assetAddr, 1n)).to.be.revertedWith('EMPTY_BALANCE');
  });
});
