import { createDebugLogger } from '@aztec/barretenberg/log';

const debug = createDebugLogger('pending_tx');

class PendingTx {
  private count = 0;

  public start(getPendingTxFn: Function) {
    setInterval(async () => {
      this.count = await getPendingTxFn();
      debug(`Pending tx count updated: ${this.count}`);
    }, 10000);
  }

  public getCount() {
    return this.count;
  }
}

const pending_tx = new PendingTx();

export default pending_tx;