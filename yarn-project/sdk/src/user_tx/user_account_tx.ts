import { AliasHash } from '@polyaztec/barretenberg/account_id';
import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { ProofId } from '@polyaztec/barretenberg/client_proofs';
import { TxId } from '@polyaztec/barretenberg/tx_id';

export class UserAccountTx {
  public readonly proofId = ProofId.ACCOUNT;

  constructor(
    public readonly txId: TxId,
    public readonly userId: GrumpkinAddress,
    public readonly aliasHash: AliasHash,
    public readonly newSpendingPublicKey1: Buffer | undefined,
    public readonly newSpendingPublicKey2: Buffer | undefined,
    public readonly migrated: boolean,
    public readonly fee: AssetValue,
    public readonly created: Date,
    public readonly settled?: Date,
  ) {}
}
