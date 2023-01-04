import {
  AztecSdk,
  createAztecSdk,
  EthAddress,
  GrumpkinAddress,
  Signer,
  TxSettlementTime,
  WalletProvider,
} from '@aztec/sdk';
import createDebug from 'debug';
import { EventEmitter } from 'events';
import { asyncMap } from './async_map.js';
import { createFundedWalletProvider } from './create_funded_wallet_provider.js';
import { addUsers } from './sdk_utils.js';
import { jest } from '@jest/globals';

jest.setTimeout(5 * 60 * 1000);
EventEmitter.defaultMaxListeners = 30;

const { ETHEREUM_HOST = 'http://localhost:8545', ROLLUP_HOST = 'http://localhost:8081' } = process.env;

/**
 * Run the following:
 * contracts: ./scripts/start_e2e.sh
 * kebab: yarn start:e2e
 * halloumi: yarn start:e2e
 * falafel: yarn start:e2e
 * end-to-end: yarn test e2e_non_fee_assets.test.ts
 */

describe('end-to-end non fee paying asset tests', () => {
  let provider: WalletProvider;
  let sdk: AztecSdk;
  let addresses: EthAddress[] = [];
  let userIds: GrumpkinAddress[] = [];
  let signers: Signer[] = [];
  const assetId = 2;
  const initialTokenBalance = { assetId, value: 10n ** 10n };
  const debug = createDebug('bb:e2e_non_fee_asset');

  const debugBalance = async (userId: GrumpkinAddress) => {
    const userIndex = userIds.findIndex(id => id.equals(userId));
    debug(
      `user ${userIndex} public / private balance: ${sdk.fromBaseUnits(
        await sdk.getPublicBalance(addresses[userIndex], assetId),
        true,
      )} / ${await sdk.getFormattedBalance(userId, assetId, true, 6)}`,
    );
  };

  beforeAll(async () => {
    debug(`funding initial ETH accounts...`);
    const initialBalance = 2n * 10n ** 16n; // 0.02
    provider = await createFundedWalletProvider(ETHEREUM_HOST, 2, 2, undefined, initialBalance);
    addresses = provider.getAccounts();

    sdk = await createAztecSdk(provider, {
      serverUrl: ROLLUP_HOST,
      pollInterval: 1000,
      memoryDb: true,
      minConfirmation: 1,
    });
    await sdk.run();
    await sdk.awaitSynchronised();

    debug('minting non-fee-paying asset...');
    await asyncMap(addresses, address => sdk.mint(initialTokenBalance, address, { signingAddress: addresses[0] }));

    debug(`adding users...`);
    const shieldEthValue = sdk.toBaseUnits(0, '0.01');
    ({ userIds, signers } = await addUsers(sdk, addresses, shieldEthValue, ...addresses));
  });

  afterAll(async () => {
    await sdk.destroy();
  });

  it('should deposit, withdraw and transfer non fee paying asset', async () => {
    const depositValue = initialTokenBalance;
    const withdrawalValue = { assetId, value: 10n ** 9n };
    const transferValue = { assetId, value: 6n * 10n ** 9n };

    const depositFees = await sdk.getDepositFees(assetId);
    const withdrawalFees = await sdk.getWithdrawFees(assetId);
    const transferFees = await sdk.getTransferFees(assetId);
    expect(depositFees[0].assetId).toBe(0);
    expect(withdrawalFees[0].assetId).toBe(0);
    expect(transferFees[0].assetId).toBe(0);

    // Rollup 1: Deposits
    {
      const controllers = await asyncMap(userIds, async (userId, i) => {
        const address = addresses[i];
        const fee = depositFees[i == userIds.length - 1 ? TxSettlementTime.INSTANT : TxSettlementTime.NEXT_ROLLUP];
        debug(
          `shielding ${sdk.fromBaseUnits(depositValue, true)} (fee: ${sdk.fromBaseUnits(
            fee,
            true,
          )}) from ${address.toString()} to account ${i}...`,
        );

        const requireFeeController = fee.assetId !== depositValue.assetId;
        const depoistFee = !requireFeeController ? fee : { assetId, value: BigInt(0) };
        const userSpendingKeyRequired = false;
        const controller = sdk.createDepositController(
          address,
          depositValue,
          depoistFee,
          userId,
          userSpendingKeyRequired,
        );
        await controller.createProof();
        await controller.approve();
        await controller.depositFundsToContract();
        await controller.awaitDepositFundsToContract();
        await controller.sign();
        if (!requireFeeController) {
          return controller;
        } else {
          const feeController = sdk.createFeeController(userIds[i], signers[i], controller.exportProofTxs(), fee);
          await feeController.createProof();
          return feeController;
        }
      });

      for (const controller of controllers) {
        await controller.send();
      }

      debug(`waiting to settle...`);
      await asyncMap(userIds, async (userId, i) => {
        const controller = controllers[i];
        await controller.awaitSettlement();
        await debugBalance(userId);
        expect(await sdk.getBalance(userId, assetId)).toEqual(depositValue);
      });
    }

    // Rollup 2: Withdrawals and transfers.
    {
      // user0 withdraw to address1.
      const recipient = addresses[1];
      const withdrawalFee = (await sdk.getWithdrawFees(assetId, { recipient }))[TxSettlementTime.NEXT_ROLLUP];
      const transferFee = transferFees[TxSettlementTime.INSTANT];

      debug(
        `withdrawing ${sdk.fromBaseUnits(withdrawalValue, true)} (fee: ${sdk.fromBaseUnits(
          withdrawalFee,
        )}) from account 0 to ${recipient.toString()}...`,
      );
      const withdrawController = sdk.createWithdrawController(
        userIds[0],
        signers[0],
        withdrawalValue,
        withdrawalFee,
        recipient,
      );
      await withdrawController.createProof();
      await withdrawController.send();

      // user1 transfers to user0.
      debug(
        `transferring ${sdk.fromBaseUnits(transferValue, true)} (fee: ${sdk.fromBaseUnits(
          transferFee,
        )}) from account 1 to account 0...`,
      );
      const transferController = sdk.createTransferController(
        userIds[1],
        signers[1],
        transferValue,
        transferFee,
        userIds[0],
      );
      await transferController.createProof();
      await transferController.send();

      debug(`waiting to settle...`);
      await Promise.all([withdrawController, transferController].map(c => c.awaitSettlement()));
      await asyncMap(userIds, userId => debugBalance(userId));

      expect((await sdk.getPublicBalance(addresses[0], assetId)).value).toBe(0n);
      expect((await sdk.getPublicBalance(addresses[1], assetId)).value).toBe(withdrawalValue.value);
      expect((await sdk.getBalance(userIds[0], assetId)).value).toBe(
        depositValue.value - withdrawalValue.value + transferValue.value,
      );
      expect((await sdk.getBalance(userIds[1], assetId)).value).toBe(depositValue.value - transferValue.value);
    }
  });
});
