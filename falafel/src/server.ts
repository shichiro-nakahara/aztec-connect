import { AliasHash } from '@aztec/barretenberg/account_id';
import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { Blockchain } from '@aztec/barretenberg/blockchain';
import { AccountVerifier, JoinSplitVerifier } from '@aztec/barretenberg/client_proofs';
import { Blake2s } from '@aztec/barretenberg/crypto';
import { InitHelpers } from '@aztec/barretenberg/environment';
import { createLogger } from '@aztec/barretenberg/log';
import { NoteAlgorithms } from '@aztec/barretenberg/note_algorithms';
import { InitialWorldState, RollupProviderStatus, RuntimeConfig } from '@aztec/barretenberg/rollup_provider';
import { BarretenbergWasm } from '@aztec/barretenberg/wasm';
import { WorldStateDb } from '@aztec/barretenberg/world_state_db';
import { CliProofGenerator, HttpJobServer, HttpJobServers, ProofGenerator } from 'halloumi/proof_generator';
import { BridgeResolver } from './bridge';
import { Configurator } from './configurator';
import { Metrics } from './metrics';
import { RollupDb } from './rollup_db';
import { RollupPipelineFactory } from './rollup_pipeline';
import { TxFeeResolver } from './tx_fee_resolver';
import { Tx, TxReceiver } from './tx_receiver';
import { WorldState } from './world_state';

export class Server {
  private blake: Blake2s;
  private worldState: WorldState;
  private txReceiver: TxReceiver;
  private txFeeResolver: TxFeeResolver;
  private pipelineFactory: RollupPipelineFactory;
  private proofGenerator: ProofGenerator;
  private bridgeResolver: BridgeResolver;
  private ready = false;

  constructor(
    private configurator: Configurator,
    private signingAddress: EthAddress,
    private blockchain: Blockchain,
    private rollupDb: RollupDb,
    worldStateDb: WorldStateDb,
    private metrics: Metrics,
    barretenberg: BarretenbergWasm,
    private log = createLogger('Server'),
  ) {
    const {
      proofGeneratorMode,
      numInnerRollupTxs,
      numOuterRollupProofs,
      proverless,
      rollupCallDataLimit,
      runtimeConfig: {
        publishInterval,
        flushAfterIdle,
        maxFeePerGas,
        maxPriorityFeePerGas,
        gasLimit,
        defaultDeFiBatchSize,
        bridgeConfigs,
        rollupBeneficiary = signingAddress,
      },
    } = configurator.getConfVars();

    const noteAlgo = new NoteAlgorithms(barretenberg);
    this.blake = new Blake2s(barretenberg);
    this.bridgeResolver = new BridgeResolver(bridgeConfigs, blockchain, defaultDeFiBatchSize);

    this.txFeeResolver = this.createTxFeeResolver();

    switch (proofGeneratorMode) {
      case 'split':
        this.proofGenerator = new HttpJobServers();
        break;
      case 'local':
        this.proofGenerator = new CliProofGenerator(
          2 ** 25,
          numInnerRollupTxs,
          numOuterRollupProofs,
          proverless,
          true,
          false,
          './data',
        );
        break;
      default:
        this.proofGenerator = new HttpJobServer();
    }

    this.pipelineFactory = new RollupPipelineFactory(
      this.proofGenerator,
      blockchain,
      rollupDb,
      worldStateDb,
      this.txFeeResolver,
      noteAlgo,
      metrics,
      rollupBeneficiary,
      publishInterval,
      flushAfterIdle,
      maxFeePerGas,
      maxPriorityFeePerGas,
      gasLimit,
      numInnerRollupTxs,
      numOuterRollupProofs,
      this.bridgeResolver,
      rollupCallDataLimit,
    );
    this.worldState = new WorldState(
      rollupDb,
      worldStateDb,
      blockchain,
      this.pipelineFactory,
      noteAlgo,
      metrics,
      this.txFeeResolver,
    );
    this.txReceiver = new TxReceiver(
      barretenberg,
      noteAlgo,
      rollupDb,
      blockchain,
      this.proofGenerator,
      new JoinSplitVerifier(),
      new AccountVerifier(),
      this.txFeeResolver,
      metrics,
      this.bridgeResolver,
    );
  }

  public async start() {
    this.log('Initializing...');

    await this.proofGenerator.start();
    await this.txFeeResolver.start();
    await this.worldState.start();
    await this.txReceiver.init();

    this.ready = true;
    this.log('Ready to receive txs.');
  }

  public async stop() {
    this.log('Stop...');
    this.ready = false;

    await this.proofGenerator.stop();
    await this.txReceiver.destroy();
    await this.worldState.stop();
    await this.txFeeResolver.stop();

    this.log('Stopped.');
  }

  public isReady() {
    return this.ready && this.configurator.getConfVars().runtimeConfig.acceptingTxs;
  }

  public getUnsettledTxCount() {
    return this.rollupDb.getUnsettledTxCount();
  }

  public async setRuntimeConfig(config: Partial<RuntimeConfig>) {
    this.log('Updating runtime config...');
    this.configurator.saveRuntimeConfig(config);
    const {
      runtimeConfig: {
        publishInterval,
        flushAfterIdle,
        maxFeePerGas,
        maxPriorityFeePerGas,
        gasLimit,
        defaultDeFiBatchSize,
        bridgeConfigs,
        rollupBeneficiary = this.signingAddress,
      },
    } = this.configurator.getConfVars();

    await this.txFeeResolver.stop();
    this.txFeeResolver = this.createTxFeeResolver();
    await this.txFeeResolver.start();

    this.worldState.setTxFeeResolver(this.txFeeResolver);
    this.txReceiver.setTxFeeResolver(this.txFeeResolver);
    this.bridgeResolver.setConf(defaultDeFiBatchSize, bridgeConfigs);
    this.pipelineFactory.setConf(
      this.txFeeResolver,
      publishInterval,
      flushAfterIdle,
      maxFeePerGas,
      maxPriorityFeePerGas,
      gasLimit,
      rollupBeneficiary,
    );
    this.metrics.rollupBeneficiary = rollupBeneficiary;

    await this.worldState.restartPipeline();
  }

  private createTxFeeResolver() {
    const {
      numInnerRollupTxs,
      numOuterRollupProofs,
      rollupCallDataLimit,
      runtimeConfig: {
        verificationGas,
        maxFeeGasPrice,
        feeGasPriceMultiplier,
        feeRoundUpSignificantFigures,
        feePayingAssetIds,
        gasLimit,
      },
    } = this.configurator.getConfVars();

    return new TxFeeResolver(
      this.blockchain,
      this.bridgeResolver,
      verificationGas,
      maxFeeGasPrice,
      feeGasPriceMultiplier,
      numInnerRollupTxs * numOuterRollupProofs,
      feePayingAssetIds,
      rollupCallDataLimit,
      gasLimit,
      feeRoundUpSignificantFigures,
    );
  }

  public removeData() {
    this.log('Removing data dir and signal to shutdown...');
    process.kill(process.pid, 'SIGUSR1');
  }

  public async resetPipline() {
    this.log('Resetting pipeline...');
    await this.worldState.resetPipeline();
  }

  public async getStatus(): Promise<RollupProviderStatus> {
    const blockchainStatus = this.blockchain.getBlockchainStatus();
    const nextPublish = this.worldState.getNextPublishTime();
    const txPoolProfile = await this.worldState.getTxPoolProfile();
    const { runtimeConfig, proverless, numInnerRollupTxs, numOuterRollupProofs } = this.configurator.getConfVars();

    const { bridgeConfigs, defaultDeFiBatchSize } = runtimeConfig;
    const thirdPartyBridgeConfigs = txPoolProfile.pendingBridgeStats
      .filter(({ bridgeId }) => !bridgeConfigs.find(bc => bc.bridgeId === bridgeId))
      .map(({ bridgeId }) => ({
        bridgeId,
        numTxs: defaultDeFiBatchSize,
        gas: this.blockchain.getBridgeGas(bridgeId),
        rollupFrequency: 0,
      }));
    const bridgeStatus = [...bridgeConfigs, ...thirdPartyBridgeConfigs].map(
      ({ bridgeId, numTxs, gas, rollupFrequency }) => {
        const rt = nextPublish.bridgeTimeouts.get(bridgeId);
        const stat = txPoolProfile.pendingBridgeStats.find(s => s.bridgeId === bridgeId);
        return {
          bridgeId,
          numTxs,
          gasThreshold: gas,
          gasAccrued: stat?.gasAccrued || 0,
          rollupFrequency,
          nextRollupNumber: rt?.rollupNumber,
          nextPublishTime: rt?.timeout,
        };
      },
    );

    return {
      blockchainStatus,
      runtimeConfig,
      numTxsPerRollup: numInnerRollupTxs * numOuterRollupProofs,
      numUnsettledTxs: txPoolProfile.numTxs,
      numTxsInNextRollup: txPoolProfile.numTxsInNextRollup,
      pendingTxCount: txPoolProfile.pendingTxCount,
      nextPublishTime: nextPublish.baseTimeout ? nextPublish.baseTimeout.timeout : new Date(0),
      nextPublishNumber: nextPublish.baseTimeout ? nextPublish.baseTimeout.rollupNumber : 0,
      bridgeStatus,
      proverless,
      rollupSize: this.worldState.getRollupSize(),
    };
  }

  public getTxFees(assetId: number) {
    return this.txFeeResolver.getTxFees(assetId);
  }

  public getDefiFees(bridgeId: bigint) {
    return this.txFeeResolver.getDefiFees(bridgeId);
  }

  public async getInitialWorldState(): Promise<InitialWorldState> {
    const chainId = await this.blockchain.getChainId();
    const accountFileName = InitHelpers.getAccountDataFile(chainId);
    const initialAccounts = accountFileName ? await InitHelpers.readData(accountFileName) : Buffer.alloc(0);
    return { initialAccounts, initialSubtreeRoots: this.worldState.getInitialStateSubtreeRoots() };
  }

  public async getUnsettledTxs() {
    return await this.rollupDb.getUnsettledTxs();
  }

  public async getUnsettledNullifiers() {
    return await this.rollupDb.getUnsettledNullifiers();
  }

  public async isAccountRegistered(accountPublicKey: GrumpkinAddress) {
    return await this.rollupDb.isAccountRegistered(accountPublicKey);
  }

  public async isAliasRegistered(alias: string) {
    const aliasHash = AliasHash.fromAlias(alias, this.blake);
    return await this.rollupDb.isAliasRegistered(aliasHash);
  }

  public async isAliasRegisteredToAccount(accountPublicKey: GrumpkinAddress, alias: string) {
    const aliasHash = AliasHash.fromAlias(alias, this.blake);
    return await this.rollupDb.isAliasRegisteredToAccount(accountPublicKey, aliasHash);
  }

  public async getUnsettledDepositTxs() {
    return await this.rollupDb.getUnsettledDepositTxs();
  }

  public getBlockBuffers(from: number, take?: number) {
    return this.worldState.getBlockBuffers(from, take);
  }

  public async getLatestRollupId() {
    return (await this.rollupDb.getNextRollupId()) - 1;
  }

  public async receiveTxs(txs: Tx[]) {
    const { maxUnsettledTxs } = this.configurator.getConfVars().runtimeConfig;
    const unsettled = await this.getUnsettledTxCount();
    if (maxUnsettledTxs && unsettled >= maxUnsettledTxs) {
      throw new Error('Too many transactions awaiting settlement. Try again later.');
    }

    const start = new Date().getTime();
    const end = this.metrics.receiveTxTimer();
    const result = await this.txReceiver.receiveTxs(txs);
    end();
    this.log(`Received tx in ${new Date().getTime() - start}ms.`);
    return result;
  }

  public flushTxs() {
    this.log('Flushing queued transactions...');
    this.worldState.flushTxs();
  }
}
