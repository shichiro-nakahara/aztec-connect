import { getTokenBalance, MainnetAddresses, TokenStore } from '@aztec/blockchain';
import {
  AccountId,
  AztecSdk,
  createNodeAztecSdk,
  DepositController,
  EthAddress,
  toBaseUnits,
  TransferController,
  TxSettlementTime,
  WalletProvider,
  WithdrawController,
} from '@aztec/sdk';
import { EventEmitter } from 'events';
import { createFundedWalletProvider } from './create_funded_wallet_provider';
import { depositTokensToAztec, sendTokens, withdrawTokens } from './sdk_utils';
import createDebug from 'debug';

jest.setTimeout(5 * 60 * 1000);
EventEmitter.defaultMaxListeners = 30;

const { ETHEREUM_HOST = 'http://localhost:8545', ROLLUP_HOST = 'http://localhost:8081' } = process.env;

/**
 * Run the following:
 * blockchain: yarn start:ganache
 * halloumi: yarn start:e2e
 * falafel: yarn start:e2e
 * end-to-end: yarn test e2e_non_fee_assets
 */

describe('end-to-end async defi tests', () => {
  let provider: WalletProvider;
  let sdk: AztecSdk;
  let accounts: EthAddress[] = [];
  const userIds: AccountId[] = [];
  const awaitSettlementTimeout = 600;
  const ethDepositedToUsersAccount = toBaseUnits('0.2', 18);
  const ethAvailableForDefi = toBaseUnits('0.2', 15);
  const ethAssetId = 0;
  const debug = createDebug('bb:e2e_non_fee_asset');

  const debugBalance = async (assetId: number, account: number) => {
    const asset = sdk.getAssetInfo(assetId);
    debug(`account ${account} balance of ${asset.name}: ${(await sdk.getBalanceAv(assetId, userIds[account])).value}`);
  };

  const getAssetName = (assetAddress: EthAddress) => {
    return sdk.getAssetInfo(sdk.getAssetIdByAddress(assetAddress)).name;
  };

  beforeAll(async () => {
    debug(`funding initial ETH accounts...`);
    provider = await createFundedWalletProvider(ETHEREUM_HOST, 2, undefined, undefined, ethDepositedToUsersAccount);
    accounts = provider.getAccounts();

    sdk = await createNodeAztecSdk(provider, {
      serverUrl: ROLLUP_HOST,
      pollInterval: 1000,
      memoryDb: true,
      minConfirmation: 1,
      minConfirmationEHW: 1,
    });
    await sdk.run();
    await sdk.awaitSynchronised();

    for (let i = 0; i < accounts.length; i++) {
      const user = await sdk.addUser(provider.getPrivateKeyForAddress(accounts[i])!);
      userIds.push(user.id);
    }
  });

  afterAll(async () => {
    await sdk.destroy();
  });

  it('should make a defi deposit', async () => {
    // Shield
    const daiEthAddress = EthAddress.fromString(MainnetAddresses.Tokens['DAI']);
    const usdcEthAddress = EthAddress.fromString(MainnetAddresses.Tokens['USDC']);
    const daiAssetId = sdk.getAssetIdByAddress(daiEthAddress);
    const usdcAssetId = sdk.getAssetIdByAddress(usdcEthAddress);
    const user1 = userIds[0];
    const user2 = userIds[1];

    debug(`shielding ETH...`);
    const shieldValue = sdk.toBaseUnits(0, '0.08');
    let expectedUser0EthBalance = shieldValue.value;
    let expectedUser1EthBalance = shieldValue.value;
    const depositControllers: DepositController[] = [];
    for (let i = 0; i < accounts.length; i++) {
      const depositor = accounts[i];
      debug(`shielding ${sdk.fromBaseUnits(shieldValue, true)} from ${depositor.toString()}...`);
      const signer = await sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(depositor)!);
      // flush this transaction through by paying for all the slots in the rollup
      const fee = (await sdk.getDepositFees(ethAssetId))[
        i == accounts.length - 1 ? TxSettlementTime.INSTANT : TxSettlementTime.NEXT_ROLLUP
      ];
      const controller = sdk.createDepositController(userIds[i], signer, shieldValue, fee, depositor);
      await controller.createProof();
      await controller.sign();
      const txHash = await controller.depositFundsToContract();
      await sdk.getTransactionReceipt(txHash);
      depositControllers.push(controller);
    }

    await Promise.all(depositControllers.map(controller => controller.send()));
    debug(`waiting for shields to settle...`);
    await Promise.all(depositControllers.map(controller => controller.awaitSettlement(awaitSettlementTimeout)));

    await debugBalance(ethAssetId, 0);
    await debugBalance(ethAssetId, 1);
    expect(await sdk.getBalance(ethAssetId, user1)).toEqual(expectedUser0EthBalance);
    expect(await sdk.getBalance(ethAssetId, user2)).toEqual(expectedUser1EthBalance);

    // user 0 purchases some DAI and USDC and deposits it into the system
    const usersEthereumAddress = accounts[0];
    const tokenStore = await TokenStore.create(provider);

    const quantityOfDaiRequested = 2n * 10n ** 12n;
    const quantityOfUsdcRequested = 10n ** 4n;
    const daiQuantityPurchased = await tokenStore.purchase(
      usersEthereumAddress,
      usersEthereumAddress,
      { erc20Address: daiEthAddress, amount: quantityOfDaiRequested },
      ethAvailableForDefi,
    );
    debug(
      `purchased ${daiQuantityPurchased} of ${getAssetName(
        daiEthAddress,
      )} for account ${usersEthereumAddress.toString()}...`,
    );
    const usdcQuantityPurchased = await tokenStore.purchase(
      usersEthereumAddress,
      usersEthereumAddress,
      { erc20Address: usdcEthAddress, amount: quantityOfUsdcRequested },
      ethAvailableForDefi,
    );
    debug(
      `purchased ${usdcQuantityPurchased} of ${getAssetName(
        usdcEthAddress,
      )} for account ${usersEthereumAddress.toString()}...`,
    );

    debug(`depositing ${daiQuantityPurchased} of ${getAssetName(daiEthAddress)} to account 0...`);

    // make the token deposits and wait for settlement
    const daiDepositController = await depositTokensToAztec(
      usersEthereumAddress,
      user1,
      daiEthAddress,
      daiQuantityPurchased,
      TxSettlementTime.NEXT_ROLLUP,
      sdk,
      provider,
    );

    debug(`depositing ${usdcQuantityPurchased} of ${getAssetName(usdcEthAddress)} to account 0...`);
    const usdcDepositController = await depositTokensToAztec(
      usersEthereumAddress,
      user1,
      usdcEthAddress,
      usdcQuantityPurchased,
      TxSettlementTime.INSTANT,
      sdk,
      provider,
    );
    debug('waiting for token deposits to settle...');
    const tokenDepositControllers: DepositController[] = [daiDepositController, usdcDepositController];
    await Promise.all(tokenDepositControllers.map(controller => controller.awaitSettlement(awaitSettlementTimeout)));

    expectedUser0EthBalance -= (await sdk.getDepositFees(daiAssetId))[TxSettlementTime.NEXT_ROLLUP].value;
    expectedUser0EthBalance -= (await sdk.getDepositFees(usdcAssetId))[TxSettlementTime.INSTANT].value;
    expect(await sdk.getBalance(ethAssetId, user1)).toEqual(expectedUser0EthBalance);
    expect(await sdk.getBalance(daiAssetId, user1)).toEqual(daiQuantityPurchased);
    expect(await sdk.getBalance(usdcAssetId, user1)).toEqual(usdcQuantityPurchased);

    await debugBalance(daiAssetId, 0);
    await debugBalance(usdcAssetId, 0);

    debug(`account 0 sending ${daiQuantityPurchased} of ${getAssetName(daiEthAddress)} to account 1...`);

    // now user 0 transfers all their DAI and USDC to user 1
    const daiTransferController = await sendTokens(
      user1,
      user2,
      daiEthAddress,
      daiQuantityPurchased,
      TxSettlementTime.NEXT_ROLLUP,
      sdk,
    );

    debug(`account 0 sending ${usdcQuantityPurchased} of ${getAssetName(usdcEthAddress)} to account 1...`);
    const usdcTransferController = await sendTokens(
      user1,
      user2,
      usdcEthAddress,
      usdcQuantityPurchased,
      TxSettlementTime.INSTANT,
      sdk,
    );
    debug('waiting for token transfers to settle');
    const tokenTransferControllers: TransferController[] = [daiTransferController, usdcTransferController];
    await Promise.all(tokenTransferControllers.map(controller => controller.awaitSettlement()));

    // check the new balances
    expectedUser0EthBalance -= (await sdk.getTransferFees(daiAssetId))[TxSettlementTime.NEXT_ROLLUP].value;
    expectedUser0EthBalance -= (await sdk.getTransferFees(usdcAssetId))[TxSettlementTime.INSTANT].value;
    expect(await sdk.getBalance(ethAssetId, user1)).toEqual(expectedUser0EthBalance);
    expect(await sdk.getBalance(daiAssetId, user2)).toEqual(daiQuantityPurchased);
    expect(await sdk.getBalance(usdcAssetId, user2)).toEqual(usdcQuantityPurchased);
    expect(await sdk.getBalance(daiAssetId, user1)).toEqual(0n);
    expect(await sdk.getBalance(usdcAssetId, user1)).toEqual(0n);

    await debugBalance(daiAssetId, 0);
    await debugBalance(daiAssetId, 1);
    await debugBalance(usdcAssetId, 0);
    await debugBalance(usdcAssetId, 1);

    debug(`account 1 withdrawing ${daiQuantityPurchased} of ${getAssetName(daiEthAddress)}...`);
    // now user 1 withdraws both assets to a wallet
    const daiWithdrawController = await withdrawTokens(
      user2,
      accounts[1],
      daiEthAddress,
      daiQuantityPurchased,
      TxSettlementTime.NEXT_ROLLUP,
      sdk,
    );
    debug(`account 1 withdrawing ${usdcQuantityPurchased} of ${getAssetName(usdcEthAddress)}...`);
    const usdcWithdrawController = await withdrawTokens(
      user2,
      accounts[1],
      usdcEthAddress,
      usdcQuantityPurchased,
      TxSettlementTime.INSTANT,
      sdk,
    );
    debug('waiting for withdrawals to settle...');
    const tokenWithdrawControllers: WithdrawController[] = [daiWithdrawController, usdcWithdrawController];
    await Promise.all(tokenWithdrawControllers.map(controller => controller.awaitSettlement()));

    //check the new balances
    await debugBalance(ethAssetId, 0);
    await debugBalance(daiAssetId, 0);
    await debugBalance(usdcAssetId, 0);
    await debugBalance(ethAssetId, 1);
    await debugBalance(daiAssetId, 1);
    await debugBalance(usdcAssetId, 1);
    expectedUser1EthBalance -= (await sdk.getWithdrawFees(daiAssetId))[TxSettlementTime.NEXT_ROLLUP].value;
    expectedUser1EthBalance -= (await sdk.getWithdrawFees(usdcAssetId))[TxSettlementTime.INSTANT].value;
    expect(await sdk.getBalance(ethAssetId, user2)).toEqual(expectedUser1EthBalance);
    expect(await sdk.getBalance(daiAssetId, user2)).toEqual(0n);
    expect(await sdk.getBalance(usdcAssetId, user2)).toEqual(0n);
    expect(await sdk.getBalance(daiAssetId, user1)).toEqual(0n);
    expect(await sdk.getBalance(usdcAssetId, user1)).toEqual(0n);
    expect(await getTokenBalance(daiEthAddress, accounts[1], provider)).toEqual(daiQuantityPurchased);
    expect(await getTokenBalance(usdcEthAddress, accounts[1], provider)).toEqual(usdcQuantityPurchased);
  });
});
