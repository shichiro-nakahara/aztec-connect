import { AliasHash } from '@polyaztec/barretenberg/account_id';
import { EthAddress, GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { Note } from '../../note/index.js';
import { SpendingKeyAccount } from './spending_key_account.js';

export interface AccountProofRequestData {
  accountPublicKey: GrumpkinAddress;
  alias: string;
  aliasHash: AliasHash;
  newAccountPublicKey: GrumpkinAddress;
  newSpendingPublicKey1: GrumpkinAddress;
  newSpendingPublicKey2: GrumpkinAddress;
  deposit: AssetValue;
  fee: AssetValue;
  depositor: EthAddress;
  inputNotes: Note[];
  spendingKeyAccount: SpendingKeyAccount;
  dataRoot: Buffer;
  allowChain: boolean;
}
