import { EthAddress } from '@polyaztec/barretenberg/address';
import { Blockchain, EthereumRpc, SendTxOptions, TxHash } from '@polyaztec/barretenberg/blockchain';
import { JoinSplitProofData } from '@polyaztec/barretenberg/client_proofs';
import { createLogger } from '@polyaztec/barretenberg/log';
import { sleep } from '@polyaztec/barretenberg/sleep';
import { fromBaseUnits } from '@polyaztec/blockchain';
import { RollupDao } from './entity/index.js';
import { Metrics } from './metrics/index.js';
import { RollupDb } from './rollup_db/index.js';
import { Notifier } from './notifier.js';

export class RollupPublisher {
  private ethereumRpc: EthereumRpc;

  constructor(
    private rollupDb: RollupDb,
    private blockchain: Blockchain,
    private maxFeePerGas: bigint,
    private maxPriorityFeePerGas: bigint,
    private gasLimit: number,
    private callDataLimit: number,
    private metrics: Metrics,
    private maxTxRetries: number,
    private log = createLogger('RollupPublisher'),
    private notifier = new Notifier('RollupPublisher')
  ) {
    this.ethereumRpc = new EthereumRpc(blockchain.getProvider());
  }

  private async awaitGasPriceBelowThresholdAndSufficientBalance(signerAddress: EthAddress, estimatedGas: number) {
    while (true) {
      // Get the previous blocks base fee.
      const { baseFeePerGas } = await this.ethereumRpc.getBlockByNumber('latest');
      // We expect to pay roughly the same, plus our priority fee.
      const estimatedFeePerGas = baseFeePerGas + this.maxPriorityFeePerGas;
      const currentBalance = await this.ethereumRpc.getBalance(signerAddress);
      // We need to have at least this balance to service our max fee.
      const requiredBalance = this.maxFeePerGas * BigInt(estimatedGas);
      // Assuming we just pay the base and priority fee, this would be our cost.
      const estimatedCost = estimatedFeePerGas * BigInt(estimatedGas);

      this.log(`Signer address: ${signerAddress.toString()}`);
      this.log(`Signer balance: ${fromBaseUnits(currentBalance, 18, 3)} MATIC`);
      this.log(`Max fee per gas: ${fromBaseUnits(this.maxFeePerGas, 9, 3)} gwei`);
      this.log(`Estimated fee per gas: ${fromBaseUnits(estimatedFeePerGas, 9, 3)} gwei`);
      this.log(`Estimated gas: ${estimatedGas}`);
      this.log(`Required balance: ${fromBaseUnits(requiredBalance, 18, 3)} MATIC`);
      this.log(`Estimated cost: ${fromBaseUnits(estimatedCost, 18, 3)} MATIC`);

      // Wait until gas price is below threshold.
      if (estimatedFeePerGas > this.maxFeePerGas) {
        this.log(`Gas price too high. Waiting till below max fee per gas...`);
        await sleep(60000);
        continue;
      }

      // Wait until we have enough funds to send all txs.
      if (currentBalance < requiredBalance) {
        this.log(`Insufficient funds. Awaiting top up...`);
        await sleep(60000);
        continue;
      }

      break;
    }
  }

  public async publishRollup(rollup: RollupDao, estimatedGas: number) {
    this.log(`Publishing rollup: ${rollup.id}`);
    const endTimer = this.metrics.publishTimer();

    const rollupTxs = await this.createTxData(rollup);
    const { rollupProofTx, offchainDataTxs } = rollupTxs;
    await this.rollupDb.setCallData(rollup.id, rollupProofTx);

    // TODO: We need to ensure a rollup provider always publishes the broadcast data, otherwise they could just
    // publish the rollup proof, and get all the fees, but without the broadcast data no clients are actually
    // able to find their txs. This is acceptable for now because we're the only provider.
    // WARNING: If you restart the server at the wrong time (in-between sending broadcast data and rollup proof),
    // you will pay twice for broadcast data.

    type TxStatus = { success: boolean; txHash?: TxHash; tx: Buffer; name: string };
    const txStatuses: TxStatus[] = [
      ...offchainDataTxs.map((tx, i) => ({
        success: false,
        tx,
        name: `broadcast data ${i + 1}/${offchainDataTxs.length}`,
      })),
      { success: false, tx: rollupProofTx, name: 'rollup proof' },
    ];
    const [defaultSignerAddress] = await this.ethereumRpc.getAccounts();

    // Send pre-publish notification
    const requiredBalance = this.maxFeePerGas * BigInt(estimatedGas);
    const currentBalance = await this.ethereumRpc.getBalance(defaultSignerAddress);

    let prePublishMessage = `Publishing rollup #${rollup.id}`;
    prePublishMessage += `\n\n<b>Required Balance</b>\n${fromBaseUnits(requiredBalance, 18, 3)} MATIC`;
    prePublishMessage += `\n\n<b>Wallet Balance</b>\n${fromBaseUnits(currentBalance, 18, 3)} MATIC`;
    await this.notifier.send(prePublishMessage);

    let txRetries = 0;

    mainLoop: while (true) {
      if (txRetries >= this.maxTxRetries) {
        this.log(`Could not publish rollup after ${txRetries} retries, restarting pipeline`);
        throw new Error('restart-pipeline');
      }
      txRetries++;

      await this.awaitGasPriceBelowThresholdAndSufficientBalance(defaultSignerAddress, estimatedGas);

      let nonce = await this.ethereumRpc.getTransactionCount(defaultSignerAddress);

      // Send each tx (if we haven't already successfully received receipt).
      for (let i = 0; i < txStatuses.length; i++) {
        const { tx, success, name } = txStatuses[i];
        if (success) {
          continue;
        }
        this.log(`Sending ${name} of size ${tx.length} with nonce ${nonce}...`);
        txStatuses[i].txHash = await this.sendTx(tx, {
          nonce: nonce++,
          gasLimit: this.gasLimit,
          maxFeePerGas: this.maxFeePerGas,
          maxPriorityFeePerGas: this.maxPriorityFeePerGas,
          signingAddress: defaultSignerAddress
        });
      }

      const unsuccessfulTx = txStatuses.find((txStatus) => !txStatus.txHash);
      if (unsuccessfulTx) {
        this.log(`Transaction ${unsuccessfulTx.name} did not send, resending after 60s...`);
        await sleep(60000);
        continue;
      }

      // All txs have been sent. Save the last txHash.
      await this.rollupDb.confirmSent(rollup.id, txStatuses[txStatuses.length - 1].txHash!);

      // Check receipts.
      for (let i = 0; i < txStatuses.length; i++) {
        const { txHash, success, name } = txStatuses[i];
        if (success) {
          continue;
        }

        const receipt = await this.getTransactionReceipt(txHash!);

        if (receipt.status) {
          txStatuses[i].success = true;
        } else {
          this.log(`Transaction failed (${name}): ${txHash!.toString()}`);
          if (receipt.revertError) {
            this.log(`Revert Error: ${receipt.revertError.name}(${receipt.revertError.params.join(', ')})`);

            // We no no longer continue trying to publish if contract state changed.
            if (receipt.revertError.name === 'INCORRECT_STATE_HASH') {
              this.log('Publish failed. Contract state changed underfoot.');
              await this.notifier.send(`\u{274C} Failed to publish rollup #${rollup.id}\n\nINCORRECT_STATE_HASH`);
              return false;
            }
          }
          await sleep(60000);

          // We will loop back around, to resend any unsuccessful txs.
          continue mainLoop;
        }
      }

      // All succeeded.
      endTimer();
      this.log('Rollup successfully published.');

      // Send post-publish notification
      let postPublishMessage = `Publish complete for rollup #${rollup.id}`;
      for (let i = 0; i < txStatuses.length; i++) {
        const { txHash, success, name } = txStatuses[i];

        postPublishMessage += `\n\n<b>Transaction #${i + 1}</b>`;
        postPublishMessage += `\n<i>${name}</i>\n`;
        postPublishMessage += `${success ? 'Success \u{2705}' : 'Failure \u{274C}'}`;

        if (txHash) postPublishMessage += `\n{{ ${txHash.toString()} }}`;
      }
      await this.notifier.send(postPublishMessage);

      return true;
    }
  }

  private async createTxData(rollup: RollupDao) {
    const txs = rollup.rollupProof.txs;
    const offchainTxData = txs.map(tx => tx.offchainTxData);
    const jsTxs = txs.filter(tx => tx.signature);
    const signatures: Buffer[] = [];
    for (const tx of jsTxs) {
      const { publicOwner, txId } = JoinSplitProofData.fromBuffer(tx.proofData);
      const proofApproval = await this.blockchain.getUserProofApprovalStatus(publicOwner, txId);
      if (!proofApproval) {
        signatures.push(tx.signature!);
      }
    }
    return await this.blockchain.createRollupTxs(
      rollup.rollupProof.encodedProofData,
      signatures,
      offchainTxData,
      this.callDataLimit,
    );
  }

  private async sendTx(txData: Buffer, options: SendTxOptions) {
    try {
      return await this.blockchain.sendTx(txData, options);
    } catch (err: any) {
      this.log(err.message.slice(0, 500));
      return undefined;
    }
  }

  private async getTransactionReceipt(txHash: TxHash) {
    while (true) {
      try {
        return await this.blockchain.getTransactionReceiptSafe(txHash, 300);
      } catch (err) {
        this.log(err);
        await sleep(60000);
      }
    }
  }
}
