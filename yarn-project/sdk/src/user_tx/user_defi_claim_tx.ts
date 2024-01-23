import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { BridgeCallData } from '@polyaztec/barretenberg/bridge_call_data';
import { ProofId } from '@polyaztec/barretenberg/client_proofs';
import { TxId } from '@polyaztec/barretenberg/tx_id';

export class UserDefiClaimTx {
  public readonly proofId = ProofId.DEFI_CLAIM;

  constructor(
    public readonly txId: TxId | undefined,
    public readonly defiTxId: TxId,
    public readonly userId: GrumpkinAddress,
    public readonly bridgeCallData: BridgeCallData,
    public readonly depositValue: AssetValue,
    public readonly success: boolean,
    public readonly outputValueA: AssetValue,
    public readonly outputValueB: AssetValue | undefined,
    public readonly created: Date,
    public readonly settled?: Date,
  ) {}
}
