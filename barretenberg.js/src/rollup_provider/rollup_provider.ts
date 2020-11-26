import { EthAddress } from '../address';
import { BlockSource } from '../block_source';
import { TxHash } from './tx_hash';

export interface Proof {
  proofData: Buffer;
  viewingKeys: Buffer[];
  depositSignature?: Buffer;
}

export interface RollupProviderStatus {
  serviceName: string;
  chainId: number;
  networkOrHost: string;
  rollupContractAddress: EthAddress;
  tokenContractAddresses: EthAddress[];
  nextRollupId: number;
  dataSize: number;
  dataRoot: Buffer;
  nullRoot: Buffer;
  rootRoot: Buffer;
  escapeOpen: boolean;
  numEscapeBlocksRemaining: number;
}

export interface RollupProvider extends BlockSource {
  sendProof(proof: Proof, signingAddress?: EthAddress): Promise<TxHash>;
  getStatus(): Promise<RollupProviderStatus>;
  getPendingNoteNullifiers: () => Promise<Buffer[]>;
}
