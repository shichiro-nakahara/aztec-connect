import { Web3Provider } from '@ethersproject/providers';
import { EthAddress } from 'barretenberg/address';
import { Blockchain } from 'barretenberg/blockchain';
import { RollupProofData } from 'barretenberg/rollup_proof';
import { TxHash } from 'barretenberg/tx_hash';
import { toBufferBE } from 'bigint-buffer';
import { EthereumProvider } from 'blockchain';
import { Signer, utils } from 'ethers';
import { RollupDao } from './entity/rollup';
import { Metrics } from './metrics';
import moment from 'moment';
import { Duration } from 'moment';
import { RollupDb } from './rollup_db';

export class RollupPublisher {
  private interrupted = false;
  private interruptPromise = Promise.resolve();
  private interruptResolve = () => {};
  private signer: Signer;

  constructor(
    private rollupDb: RollupDb,
    private blockchain: Blockchain,
    private publishInterval: Duration,
    private feeLimit: bigint,
    private feeGasPrice: bigint,
    private feeGasPriceMultiplier: number,
    provider: EthereumProvider,
    private metrics: Metrics,
  ) {
    this.signer = new Web3Provider(provider).getSigner();
    this.interruptPromise = new Promise(resolve => (this.interruptResolve = resolve));
  }

  public async publishRollup(rollup: RollupDao) {
    const txData = await this.createTxData(rollup);
    await this.rollupDb.setCallData(rollup.id, txData);

    while (!this.interrupted) {
      const end = this.metrics.publishTimer();
      const txHash = await this.sendRollupProof(txData);
      if (!txHash) {
        break;
      }

      await this.rollupDb.confirmSent(rollup.id, txHash);

      const receipt = await this.getTransactionReceipt(txHash);
      if (!receipt) {
        break;
      }

      if (receipt.status) {
        end();
        return true;
      }

      const { nextRollupId } = await this.blockchain.getBlockchainStatus();
      if (nextRollupId > rollup.id) {
        console.log('Publish failed. Contract changed underfoot.');
        break;
      }

      console.log(`Transaction status failed: ${txHash}`);
      await this.sleepOrInterrupted(60000);
    }

    return false;
  }

  /**
   * Calling `interrupt` will cause any in progress call to `publishRollup` to return `false` asap.
   * Be warned, the call may return false even if the tx subsequently gets successfully mined.
   * In practice this shouldn't matter, as we'll only ever be calling `interrupt` when we know it's going to fail.
   * A call to `clearInterrupt` is required before you can continue publishing.
   */
  public interrupt() {
    this.interrupted = true;
    this.interruptResolve();
  }

  private async createTxData(rollup: RollupDao) {
    const proof = rollup.rollupProof.proofData;
    const txs = rollup.rollupProof.txs;
    const viewingKeys = txs
      .map(tx => [tx.viewingKey1, tx.viewingKey2])
      .flat()
      .map(vk => vk.toBuffer());
    const signatures = txs.map(tx => tx.signature!).filter(s => !!s);
    const providerAddress = EthAddress.fromString(await this.signer.getAddress());
    const { feeDistributorContractAddress } = await this.blockchain.getBlockchainStatus();
    const providerSignature = await this.generateSignature(
      proof,
      providerAddress,
      this.feeLimit,
      feeDistributorContractAddress,
    );

    return await this.blockchain.createRollupProofTx(
      proof,
      signatures,
      viewingKeys,
      providerSignature,
      providerAddress,
      providerAddress,
      this.feeLimit,
    );
  }

  public async getNextPublishTime() {
    const unsettledTxs = await this.rollupDb.getUnsettledTxCount();
    if (!unsettledTxs) {
      // No txs, report publish time is in publishInterval seconds (not necessarily true).
      return moment().add(this.publishInterval).toDate();
    }
    const lastRollup = await this.rollupDb.getLastSettledRollup();
    if (!lastRollup) {
      // We have a tx, but have not rolled up before. Rollup now.
      return new Date();
    }
    // We have rolled up before. Rollup in publishInterval seconds from rollup, clamped no earlier than now.
    const nextRollupTime = moment(lastRollup.mined).add(this.publishInterval);
    return nextRollupTime.isSameOrBefore() ? new Date() : nextRollupTime.toDate();
  }

  private async generateSignature(
    rollupProof: Buffer,
    feeReceiver: EthAddress,
    feeLimit: bigint,
    feeDistributorAddress: EthAddress,
  ) {
    const publicInputs = rollupProof.slice(0, RollupProofData.LENGTH_ROLLUP_PUBLIC);
    const msgHash = utils.solidityKeccak256(
      ['bytes'],
      [
        Buffer.concat([
          publicInputs,
          feeReceiver.toBuffer(),
          toBufferBE(feeLimit, 32),
          feeDistributorAddress.toBuffer(),
        ]),
      ],
    );
    const digest = utils.arrayify(msgHash);
    const signature = await this.signer.signMessage(digest);
    let signatureBuf = Buffer.from(signature.slice(2), 'hex');
    const v = signatureBuf[signatureBuf.length - 1];
    if (v <= 1) {
      signatureBuf = Buffer.concat([signatureBuf.slice(0, -1), Buffer.from([v + 27])]);
    }

    return signatureBuf;
  }

  private async sendRollupProof(txData: Buffer) {
    while (!this.interrupted) {
      try {
        const multiplier = BigInt(Math.floor(this.feeGasPriceMultiplier * 100));
        const reportedPrice = ((await this.blockchain.getGasPrice()) * multiplier) / 100n;
        const gasPrice = reportedPrice < this.feeGasPrice ? reportedPrice : this.feeGasPrice;
        return await this.blockchain.sendTx(txData, { gasPrice });
      } catch (err) {
        console.log(err.message.slice(0, 200));
        await this.sleepOrInterrupted(60000);
      }
    }
  }

  private async getTransactionReceipt(txHash: TxHash) {
    while (!this.interrupted) {
      try {
        return await this.blockchain.getTransactionReceiptSafe(txHash);
      } catch (err) {
        console.log(err);
        await this.sleepOrInterrupted(60000);
      }
    }
  }

  private async sleepOrInterrupted(ms: number) {
    await Promise.race([new Promise(resolve => setTimeout(resolve, ms)), this.interruptPromise]);
  }
}
