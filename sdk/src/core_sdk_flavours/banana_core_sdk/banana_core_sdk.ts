import { WorkerPool } from '@aztec/barretenberg/wasm';
import { createDebugLogger } from '@aztec/barretenberg/log';
import { CoreSdkDispatch, SdkEvent } from '../../core_sdk';
import { JobQueueWorker } from '../job_queue';
import { DispatchMsg } from '../transport';

const debug = createDebugLogger('bb:banana_core_sdk');

export class BananaCoreSdk extends CoreSdkDispatch {
  constructor(
    dispatch: (msg: DispatchMsg) => Promise<any>,
    private jobQueueWorker: JobQueueWorker,
    private workerPool: WorkerPool,
  ) {
    super(dispatch);
  }

  public async destroy() {
    // We don't actually destroy a remote sdk. Just emit the destroy event to perform any cleanup.
    debug('Destroying banana core sdk...');
    await this.jobQueueWorker?.stop();
    await this.workerPool.destroy();
    this.emit(SdkEvent.DESTROYED);
    debug('Banana core sdk destroyed.');
  }
}
