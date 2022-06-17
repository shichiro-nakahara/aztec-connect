import { ServerRollupProvider } from '@aztec/barretenberg/rollup_provider';
import { BarretenbergWasm } from '@aztec/barretenberg/wasm';
import { CoreSdk, CoreSdkServerStub } from '../../core_sdk';
import { JobQueue, JobQueueFftFactory, JobQueueNoteDecryptor, JobQueuePedersen, JobQueuePippenger } from '../job_queue';
import { getDb, getLevelDb } from '../vanilla_core_sdk';
import { ChocolateCoreSdkOptions } from './chocolate_core_sdk_options';

/**
 * Construct a chocolate version of the sdk.
 * This creates a CoreSdk for running in some remote context, e.g. a shared worker.
 * It is wrapped in a network type adapter.
 * It is not interfaced with directly, but rather via a banana sdk, over some transport layer.
 */
export async function createChocolateCoreSdk(jobQueue: JobQueue, options: ChocolateCoreSdkOptions) {
  const wasm = await BarretenbergWasm.new();
  const noteDecryptor = new JobQueueNoteDecryptor(jobQueue);
  const pedersen = new JobQueuePedersen(wasm, jobQueue);
  const pippenger = new JobQueuePippenger(jobQueue);
  const fftFactory = new JobQueueFftFactory(jobQueue);
  const { pollInterval, serverUrl } = options;

  const leveldb = getLevelDb();
  const db = await getDb();
  await db.init();

  const host = new URL(serverUrl);
  const rollupProvider = new ServerRollupProvider(host, pollInterval);

  const coreSdk = new CoreSdk(leveldb, db, rollupProvider, wasm, noteDecryptor, pedersen, pippenger, fftFactory);
  await coreSdk.init(options);
  return new CoreSdkServerStub(coreSdk);
}
