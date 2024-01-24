import { createDebugLogger } from '@polyaztec/barretenberg/log';
import { configurator } from './configurator.js';

const debug = createDebugLogger('surge');

class Surge {
  private pendingTxCount = 0;

  // Has to be initialized in world_state constructor 
  public init(getPendingTxFn: Function) {
    setInterval(async () => {
      try {
        this.pendingTxCount = await getPendingTxFn();
        debug(`Pending tx count updated: ${this.pendingTxCount}`);
      }
      catch (e) {}
    }, 10000);
  }

  public getPendingTxCount() {
    return this.pendingTxCount;
  }

  public getMultiplier() {
    const { surgeFeeGasPriceMultiplier } = configurator.getConfVars().runtimeConfig;

    let result = 1;
    for (let i = surgeFeeGasPriceMultiplier.length - 1; i >= 0; i--) {
      const { pendingTxThreshold, multiplier } = surgeFeeGasPriceMultiplier[i];
      if (this.pendingTxCount >= pendingTxThreshold) {
        result = multiplier;
        break;
      }
    }

    debug(`${this.pendingTxCount} pendingTx, multiplier ${result}x`);

    return result;
  }
}

const surge = new Surge();

export default surge;