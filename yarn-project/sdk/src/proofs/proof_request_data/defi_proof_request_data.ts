import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { BridgeCallData } from '@polyaztec/barretenberg/bridge_call_data';
import { Note } from '../../note/index.js';
import { SpendingKeyAccount } from './spending_key_account.js';

export interface DefiProofRequestData {
  accountPublicKey: GrumpkinAddress;
  bridgeCallData: BridgeCallData;
  assetValue: AssetValue;
  fee: AssetValue;
  inputNotes: Note[];
  spendingKeyAccount: SpendingKeyAccount;
  dataRoot: Buffer;
  allowChain: boolean;
}
