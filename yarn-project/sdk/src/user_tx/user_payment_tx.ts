import { EthAddress, GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { ProofId } from '@polyaztec/barretenberg/client_proofs';
import { TxId } from '@polyaztec/barretenberg/tx_id';

export class UserPaymentTx {
  constructor(
    public readonly txId: TxId,
    public readonly userId: GrumpkinAddress,
    public readonly proofId: ProofId.DEPOSIT | ProofId.WITHDRAW | ProofId.SEND,
    public readonly value: AssetValue,
    public readonly fee: AssetValue,
    public readonly publicOwner: EthAddress | undefined,
    public readonly isSender: boolean,
    public readonly created: Date,
    public readonly settled?: Date,
  ) {}
}
