import { EthAddress } from '../address';
import { BlockSource } from '../block_source';

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
}

export type TxHash = Buffer;

export interface RollupProvider extends BlockSource {
  sendProof(proof: Proof): Promise<TxHash>;
  status(): Promise<RollupProviderStatus>;
}
