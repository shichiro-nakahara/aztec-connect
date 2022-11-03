import { setBlockchainTime } from '@aztec/blockchain';
import { AztecSdk, EthAddress, EthereumRpc, WalletProvider } from '@aztec/sdk';
import {
  retrieveElementConfig,
  AgentElementConfig,
  formatTime,
  createElementBridgeData,
  ELEMENT_CHECKPOINTS,
  ELEMENT_START_TIME,
  getAssetRequirementsForElement,
} from './bridges.js';
import { ElementAgent } from './element_agent.js';
import { ManualPaymentAgent } from './manual_payment_agent.js';
import { ethDepositCost, ethTransferCost, purchaseAssets } from './assets.js';
import { EthAddressAndNonce } from './agent.js';
import { asyncMap } from './async_map.js';

export class ElementAgentManager {
  private elementConfig: AgentElementConfig[] = [];
  private elementAgents: ElementAgent[] = [];
  private manualAgent!: ManualPaymentAgent;
  private fundingAccount!: EthAddressAndNonce;

  public constructor(
    private sdk: AztecSdk,
    private provider: WalletProvider,
    private ethereumRpc: EthereumRpc,
    private fundingAddress: EthAddress,
    private numAgents: number,
    private numTxsPerAgent: number,
    private assets: number[],
  ) {}

  public async init() {
    const fundingNonce = await this.ethereumRpc.getTransactionCount(this.fundingAddress);
    this.fundingAccount = { address: this.fundingAddress, nonce: fundingNonce };

    // need a manual agent for manually flushing tranches
    this.manualAgent = await ManualPaymentAgent.create(this.fundingAccount, this.sdk, this.provider, this.numAgents, 2);

    // retrieve the element config, telling us the assets and expiry details
    this.elementConfig = await retrieveElementConfig(this.sdk, this.provider, this.assets);
    console.log(
      'using Element config',
      this.elementConfig.map(x => {
        return { asset: this.sdk.getAssetInfo(x.assetId).name, expiry: x.expiry };
      }),
    );

    // create the agents
    this.elementAgents = await asyncMap(Array.from({ length: this.numAgents }), async (_, i) => {
      return await ElementAgent.create(
        this.fundingAccount,
        this.sdk,
        this.provider,
        i,
        this.numTxsPerAgent,
        this.elementConfig,
        ELEMENT_CHECKPOINTS,
      );
    });
  }

  public async getRequiredFunds(gasPriceWei: number) {
    const requiredFundings = await asyncMap(this.elementAgents, async agent => {
      const agentFunding = await agent.getRequiredFunding();
      return ethTransferCost(gasPriceWei) + ethDepositCost(gasPriceWei) + agentFunding;
    });
    return requiredFundings.reduce((p, c) => p + c, 0n) + (await this.manualAgent.getFundingRequirement());
  }

  public async run() {
    // get the assets requirements and purchase the assets
    const assetRequirements = await getAssetRequirementsForElement(this.sdk, this.provider, this.assets);
    const acquiredAssets = await purchaseAssets(
      this.sdk,
      this.fundingAddress,
      this.provider,
      assetRequirements,
      10n ** 9n,
    );

    await this.manualAgent.start();

    // now that all assets have been purchased, update the nonce of the funding account
    this.fundingAccount.nonce = await this.ethereumRpc.getTransactionCount(this.fundingAddress);

    // update the configuration with the purchased quantities
    for (const asset of acquiredAssets.usableBalances) {
      const expiriesForAsset = this.elementConfig.filter(x => x.assetId == asset.assetId);
      if (expiriesForAsset.length == 0) {
        continue;
      }
      const quantityPerAgentPerExpiry = asset.value / BigInt(expiriesForAsset.length * this.numAgents);
      expiriesForAsset.forEach(x => (x.assetQuantity = quantityPerAgentPerExpiry));
    }

    // create an ordered array of time checkpoints that we will use for agent orchestration
    const latestTrancheExpiry = this.elementConfig[this.elementConfig.length - 1].expiry!;
    const checkpoints = [ELEMENT_CHECKPOINTS[0]];
    for (let i = 1; i < ELEMENT_CHECKPOINTS.length; i++) {
      if (checkpoints[i - 1] >= latestTrancheExpiry) {
        break;
      }
      checkpoints.push(ELEMENT_CHECKPOINTS[i]);
    }
    console.log('using expiry checkpoints', checkpoints);
    // start the agents
    this.elementAgents.forEach(x => x.updateAssetQuantites(this.elementConfig));
    const runPromises = this.elementAgents.map(a => a.run());

    // this function will cause us to wait until all agents have completed the current batch of deposits
    const waitOnCheckpoint = async () => {
      while (this.elementAgents.some(a => !a.isOnCheckpoint())) {
        await new Promise<void>(resolve => setTimeout(() => resolve(), 10000));
      }
    };

    // this function will set the blockchain time
    const setChainTime = async (timestamp: number) => {
      console.log(`setting blockchain time to ${formatTime(timestamp)}`);
      await setBlockchainTime(timestamp, this.provider);
    };

    // this function will work our what interactions still need to be finalised and finalise them manually
    const finaliseRemainingNonces = async () => {
      console.log(`working out what to finalise`);
      const bridgeData = await createElementBridgeData(this.sdk, this.provider);
      const interactionNonces = new Set<number>();
      // get all of the agent controllers
      for (const controller of this.elementAgents.flatMap(a => a.controllers)) {
        const nonce = await controller.getInteractionNonce();
        if (nonce === undefined) {
          continue;
        }
        // if this nonce has been finalised then there is nothing to do
        if (await bridgeData!.hasFinalised(BigInt(nonce!))) {
          continue;
        }
        interactionNonces.add(nonce);
      }
      // finalise all outstanding nonces
      for (const nonce of interactionNonces) {
        console.log(`finalising nonce ${nonce}`);
        await this.sdk.processAsyncDefiInteraction(nonce);
      }
      // we now need to send 2 flush txs
      // the first to generate the claims
      // the second to flush them through the system
      console.log('submitting first flush');
      const firstTx = await this.manualAgent.transfer();
      await this.sdk.awaitSettlement(firstTx);
      console.log('submitting second flush');
      const secondTx = await this.manualAgent.transfer();
      await this.sdk.awaitSettlement(secondTx);
    };

    // set the chain time to before any expiries
    await setChainTime(ELEMENT_START_TIME);
    // loop until all checkpoints have been passed
    // for each checkpoint, we wait until the agents have reached it
    // then move the chain time
    // then trigger the agents to continue their deposits and wait for them to reach the next checkpoint etc
    for (let i = 0; i < checkpoints.length; i++) {
      await waitOnCheckpoint();
      // all agents on checkpoint, set the time and trigger the agents
      console.log(`agent manager checkpoint reached`);
      await setChainTime(checkpoints[i] + 1);
      this.elementAgents.forEach(a => a.triggerNextCheckpoint());
    }
    // finalise the outstanding nonces
    await finaliseRemainingNonces();
    console.log('nonces finalised, triggering final checkpoint');
    this.elementAgents.forEach(a => a.triggerNextCheckpoint());
    await Promise.all(runPromises);
    // now have a look at the final balances
    for (let i = 0; i < acquiredAssets.originalBalances.length; i++) {
      const assetValue = acquiredAssets.originalBalances[i];
      const assetInfo = this.sdk.getAssetInfo(assetValue.assetId);
      const balance = await this.sdk.getPublicBalance(this.fundingAddress, assetValue.assetId);
      console.log(
        `funding address ${this.fundingAddress} original balance of ${assetInfo.name} was ${assetValue.value}, new balance is ${balance.value}`,
      );
    }
  }
}
