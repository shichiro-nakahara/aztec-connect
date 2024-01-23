import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { BridgeCallData } from '@polyaztec/barretenberg/bridge_call_data';
import { ProofId } from '@polyaztec/barretenberg/client_proofs';
import { TxId } from '@polyaztec/barretenberg/tx_id';

export enum UserDefiInteractionResultState {
  PENDING = 'PENDING',
  AWAITING_FINALISATION = 'AWAITING_FINALISATION',
  AWAITING_SETTLEMENT = 'AWAITING_SETTLEMENT',
  SETTLED = 'SETTLED',
}

export interface UserDefiInteractionResult {
  state: UserDefiInteractionResultState;
  isAsync?: boolean;
  interactionNonce?: number;
  success?: boolean;
  outputValueA?: AssetValue;
  outputValueB?: AssetValue;
  claimSettled?: Date;
  finalised?: Date;
}

export class UserDefiTx {
  public readonly proofId = ProofId.DEFI_DEPOSIT;

  constructor(
    public readonly txId: TxId,
    public readonly userId: GrumpkinAddress,
    public readonly bridgeCallData: BridgeCallData,
    public readonly depositValue: AssetValue,
    public readonly fee: AssetValue,
    public readonly created: Date,
    public readonly settled: Date | undefined,
    public readonly interactionResult: UserDefiInteractionResult,
  ) {}
}
