import {
  AccountId,
  AztecSdk,
  BitConfig,
  BridgeId,
  createAztecSdk,
  DefiController,
  DefiSettlementTime,
  EthAddress,
  toBaseUnits,
  TxSettlementTime,
  WalletProvider,
} from '@aztec/sdk';
import { EventEmitter } from 'events';
import { createFundedWalletProvider } from './create_funded_wallet_provider';
import createDebug from 'debug';

jest.setTimeout(20 * 60 * 1000);
EventEmitter.defaultMaxListeners = 30;

const {
  ETHEREUM_HOST = 'http://localhost:8545',
  ROLLUP_HOST = 'http://localhost:8081',
  PRIVATE_KEY = '',
} = process.env;

/**
 * Run the following:
 * blockchain: yarn start:ganache
 * halloumi: yarn start:e2e
 * falafel: yarn start:e2e
 * end-to-end: yarn test e2e_defi
 */
describe('end-to-end defi tests', () => {
  let provider: WalletProvider;
  let sdk: AztecSdk;
  let accounts: EthAddress[] = [];
  const userIds: AccountId[] = [];
  const debug = createDebug('bb:e2e_defi');

  const flushClaim = async () => {
    const signer = sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(accounts[3])!);
    await sdk.flushRollup(userIds[3], signer);
  };

  beforeAll(async () => {
    debug(`funding initial ETH accounts...`);
    const privateKey = Buffer.from(PRIVATE_KEY, 'hex');
    provider = await createFundedWalletProvider(ETHEREUM_HOST, 4, 4, privateKey, toBaseUnits('0.2', 18));
    accounts = provider.getAccounts();

    sdk = await createAztecSdk(provider, ROLLUP_HOST, {
      syncInstances: false,
      pollInterval: 1000,
      saveProvingKey: false,
      clearDb: true,
      memoryDb: true,
      minConfirmation: 1,
      minConfirmationEHW: 1,
    });
    await sdk.init();
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
    const debugBalance = (assetId: number, account: number) =>
      debug(`account ${account} balance: ${sdk.fromBaseUnits(sdk.getBalanceAv(assetId, userIds[account]), true)}`);

    const shieldValue = sdk.toBaseUnits(0, '0.08');
    const ethToDaiBridge = new BridgeId(1, 0, 1, 0, 0, new BitConfig(false, false, false, false, false, false), 0);
    const daiToEthBridge = new BridgeId(2, 1, 0, 0, 0, new BitConfig(false, false, false, false, false, false), 0);
    const ethToDaiFees = await sdk.getDefiFees(ethToDaiBridge);
    const daiToEthFees = await sdk.getDefiFees(daiToEthBridge);

    // Rollup 0.
    // Shield.
    {
      const promises: Promise<void>[] = [];
      const depositFees = await sdk.getDepositFees(shieldValue.assetId);

      for (let i = 0; i < accounts.length; i++) {
        const depositor = accounts[i];
        debug(`shielding ${sdk.fromBaseUnits(shieldValue, true)} from ${depositor.toString()}...`);

        const signer = sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(depositor)!);
        // Last deposit pays for instant rollup to flush.
        const fee = depositFees[i == accounts.length - 1 ? TxSettlementTime.INSTANT : TxSettlementTime.NEXT_ROLLUP];
        const controller = sdk.createDepositController(userIds[i], signer, shieldValue, fee, depositor);
        await controller.createProof();
        await controller.sign();
        const txHash = await controller.depositFundsToContract();
        await sdk.getTransactionReceipt(txHash);
        await controller.send();
        promises.push(controller.awaitSettlement());
      }

      debug(`waiting for shields to settle...`);
      await Promise.all(promises);
    }

    // Rollup 1.
    // Account 0 swaps partial ETH to DAI.
    {
      const signer = sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(accounts[0])!);
      const { inputAssetIdA, outputAssetIdA } = ethToDaiBridge;

      debugBalance(inputAssetIdA, 0);
      debugBalance(outputAssetIdA, 0);

      const depositValue = sdk.toBaseUnits(inputAssetIdA, '0.05');
      const fee = ethToDaiFees[DefiSettlementTime.INSTANT];

      debug(
        `account 0 swapping ${sdk.fromBaseUnits(depositValue, true)} (fee: ${sdk.fromBaseUnits(fee)}) for ${
          sdk.getAssetInfo(outputAssetIdA).symbol
        }...`,
      );

      const controller = sdk.createDefiController(userIds[0], signer, ethToDaiBridge, depositValue, fee);
      await controller.createProof();
      await controller.send();

      debug(`waiting for defi interaction to complete...`);
      await controller.awaitDefiInteraction();

      debug('waiting for claim to settle...');
      await flushClaim();
      await controller.awaitSettlement();

      const [defiTx] = await sdk.getDefiTxs(userIds[0]);
      const expectedInputBalance = shieldValue.value - depositValue.value - fee.value;
      expect(defiTx).toMatchObject({ bridgeId: ethToDaiBridge, depositValue, fee, outputValueB: 0n });
      expect(sdk.getBalance(inputAssetIdA, userIds[0])).toBe(expectedInputBalance);
      expect(sdk.getBalance(outputAssetIdA, userIds[0])).toBe(defiTx.outputValueA);
    }

    // Rollup 2.
    // Account 0 swaps DAI to ETH.
    // Accounts 1 and 2 swap ETH to DAI.
    {
      const defiControllers: DefiController[] = [];
      const defiVerifications: Array<() => Promise<void>> = [];
      {
        const { inputAssetIdA, outputAssetIdA } = daiToEthBridge;

        debugBalance(inputAssetIdA, 0);
        debugBalance(outputAssetIdA, 0);
        const initialEthBalance = sdk.getBalanceAv(0, userIds[0]);
        const initialDaiBalance = sdk.getBalanceAv(1, userIds[0]);
        const fee = daiToEthFees[DefiSettlementTime.NEXT_ROLLUP];

        debug(
          `account 0 swapping ${sdk.fromBaseUnits(initialDaiBalance, true)} (fee: ${sdk.fromBaseUnits(fee)}) for ${
            sdk.getAssetInfo(outputAssetIdA).symbol
          }...`,
        );

        const depositValue = { assetId: inputAssetIdA, value: initialDaiBalance.value - fee.value };
        const signer = sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(accounts[0])!);
        const controller = sdk.createDefiController(userIds[0], signer, daiToEthBridge, depositValue, fee);
        await controller.createProof();
        await controller.send();
        defiControllers.push(controller);

        const verification = async () => {
          const [defiTx] = await sdk.getDefiTxs(userIds[0]);
          expect(defiTx).toMatchObject({ bridgeId: daiToEthBridge, depositValue, fee, outputValueB: 0n });
          expect(sdk.getBalance(0, userIds[0])).toBe(initialEthBalance.value + defiTx.outputValueA);
          expect(sdk.getBalance(1, userIds[0])).toBe(0n);
          debugBalance(inputAssetIdA, 0);
          debugBalance(outputAssetIdA, 0);
        };
        defiVerifications.push(verification);
      }

      for (let i = 1; i < 3; i++) {
        const { inputAssetIdA, outputAssetIdA } = ethToDaiBridge;

        debugBalance(inputAssetIdA, i);
        debugBalance(outputAssetIdA, i);
        const depositValue = sdk.toBaseUnits(inputAssetIdA, '0.05');
        const fee = ethToDaiFees[i == 2 ? DefiSettlementTime.INSTANT : DefiSettlementTime.NEXT_ROLLUP];

        debug(
          `account ${i} swapping ${sdk.fromBaseUnits(depositValue, true)} (fee: ${sdk.fromBaseUnits(fee)}) for ${
            sdk.getAssetInfo(outputAssetIdA).symbol
          }...`,
        );

        const signer = sdk.createSchnorrSigner(provider.getPrivateKeyForAddress(accounts[i])!);
        const controller = sdk.createDefiController(userIds[i], signer, ethToDaiBridge, depositValue, fee);
        await controller.createProof();
        await controller.send();
        defiControllers.push(controller);

        const verification = async () => {
          const [defiTx] = await sdk.getDefiTxs(userIds[i]);
          const expectedInputBalance = shieldValue.value - depositValue.value - fee.value;
          expect(defiTx).toMatchObject({ bridgeId: ethToDaiBridge, depositValue, fee, outputValueB: 0n });
          expect(sdk.getBalance(inputAssetIdA, userIds[i])).toBe(expectedInputBalance);
          expect(sdk.getBalance(outputAssetIdA, userIds[i])).toBe(defiTx.outputValueA);
          debugBalance(inputAssetIdA, i);
          debugBalance(outputAssetIdA, i);
        };
        defiVerifications.push(verification);
      }

      debug(`waiting for defi interactions to complete...`);
      await Promise.all(defiControllers.map(controller => controller.awaitDefiInteraction()));

      debug(`waiting for claims to settle...`);
      await flushClaim();
      await Promise.all(defiControllers.map(controller => controller.awaitSettlement()));

      // Check results.
      await Promise.all(defiVerifications.map(x => x()));
    }
  });
});
