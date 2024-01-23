import { CoreSdk } from './core_sdk/index.js';
export { SDK_VERSION } from './version.js';
export * from './aztec_sdk/index.js';
export * from './controllers/index.js';
export { SdkEvent, SdkStatus } from './core_sdk/index.js';
export * from './note/index.js';
export * from './signer/index.js';
export * from './user/index.js';
export * from './user_tx/index.js';
export * from '@polyaztec/barretenberg/account_id';
export * from '@polyaztec/barretenberg/address';
export * from '@polyaztec/barretenberg/asset';
export * from '@polyaztec/barretenberg/bigint_buffer';
export * from '@polyaztec/barretenberg/bridge_call_data';
export { ProofId } from '@polyaztec/barretenberg/client_proofs';
export * from '@polyaztec/barretenberg/crypto';
export * from '@polyaztec/barretenberg/rollup_provider';
export * from '@polyaztec/barretenberg/rollup_proof';
export * from '@polyaztec/barretenberg/fifo';
export * from '@polyaztec/barretenberg/tx_id';
export * from '@polyaztec/barretenberg/blockchain';
export * from '@polyaztec/barretenberg/timer';
export * from '@polyaztec/barretenberg/log';
export * from '@polyaztec/barretenberg/offchain_tx_data';
export { DecodedBlock } from '@polyaztec/barretenberg/block_source';

export {
  JsonRpcProvider,
  WalletProvider,
  EthersAdapter,
  Web3Adapter,
  Web3Provider,
  Web3Signer,
  toBaseUnits,
  fromBaseUnits,
  FeeDistributor,
  RollupProcessor,
  EthAsset,
} from '@polyaztec/blockchain';

// Exposing for medici. Remove once they have proper multisig api.
export * from './proofs/index.js';
export { BarretenbergWasm } from '@polyaztec/barretenberg/wasm';
export type CoreSdkInterface = {
  [K in keyof CoreSdk]: CoreSdk[K];
};
