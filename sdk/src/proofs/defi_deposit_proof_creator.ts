import { GrumpkinAddress } from '@aztec/barretenberg/address';
import { BridgeId } from '@aztec/barretenberg/bridge_id';
import { JoinSplitProver, ProofData, ProofId } from '@aztec/barretenberg/client_proofs';
import { createDebugLogger } from '@aztec/barretenberg/log';
import { Grumpkin } from '@aztec/barretenberg/ecc';
import { NoteAlgorithms } from '@aztec/barretenberg/note_algorithms';
import { OffchainDefiDepositData } from '@aztec/barretenberg/offchain_tx_data';
import { TxId } from '@aztec/barretenberg/tx_id';
import { WorldState } from '@aztec/barretenberg/world_state';
import { CoreDefiTx } from '../core_tx';
import { Database } from '../database';
import { Note } from '../note';
import { UserData } from '../user';
import { JoinSplitTxFactory } from './join_split_tx_factory';
import { JoinSplitProofInput } from './proof_input';

const debug = createDebugLogger('bb:defi_deposit_proof_creator');

export class DefiDepositProofCreator {
  private txFactory: JoinSplitTxFactory;

  constructor(
    private prover: JoinSplitProver,
    private noteAlgos: NoteAlgorithms,
    worldState: WorldState,
    grumpkin: Grumpkin,
    db: Database,
  ) {
    this.txFactory = new JoinSplitTxFactory(noteAlgos, worldState, grumpkin, db);
  }

  public async createProofInput(
    user: UserData,
    bridgeId: BridgeId,
    depositValue: bigint,
    inputNotes: Note[],
    spendingPublicKey: GrumpkinAddress,
  ) {
    if (spendingPublicKey.equals(user.accountPublicKey)) {
      throw new Error('Cannot spend notes for defi deposit using account key.');
    }

    const assetId = bridgeId.inputAssetIdA;
    const proofInput = await this.txFactory.createTx(
      user,
      ProofId.DEFI_DEPOSIT,
      assetId,
      inputNotes,
      spendingPublicKey,
      {
        bridgeId,
        defiDepositValue: depositValue,
        newNoteOwner: user.accountPublicKey,
        newNoteOwnerAccountRequired: true,
      },
    );

    const signingData = await this.prover.computeSigningData(proofInput.tx);

    return { ...proofInput, signingData };
  }

  public async createProof(
    { accountPublicKey, accountPrivateKey }: UserData,
    { tx, signature, partialStateSecretEphPubKey, viewingKeys }: JoinSplitProofInput,
    txRefNo: number,
  ) {
    debug('creating proof...');
    const start = new Date().getTime();
    const proof = await this.prover.createProof(tx, signature!);
    debug(`created proof: ${new Date().getTime() - start}ms`);
    debug(`proof size: ${proof.length}`);

    const proofData = new ProofData(proof);
    const txId = new TxId(proofData.txId);
    const {
      outputNotes,
      claimNote: { value: depositValue, bridgeId, partialStateSecret },
      inputNotes,
    } = tx;
    const privateInput =
      bridgeId.numInputAssets > 1 ? inputNotes[0].value : inputNotes.reduce((sum, n) => sum + n.value, BigInt(0));
    const txFee = privateInput - depositValue;
    const coreTx = new CoreDefiTx(
      txId,
      accountPublicKey,
      bridgeId,
      depositValue,
      txFee,
      partialStateSecret,
      txRefNo,
      new Date(),
    );
    const partialState = this.noteAlgos.valueNotePartialCommitment(
      partialStateSecret,
      accountPublicKey,
      true, // accountRequired
    );
    const offchainTxData = new OffchainDefiDepositData(
      bridgeId,
      partialState,
      partialStateSecretEphPubKey!,
      depositValue,
      txFee,
      viewingKeys[0], // contains [value, asset_id, accountRequired, creatorPubKey] of the change note (returned to the sender)
      txRefNo,
    );

    return {
      tx: coreTx,
      proofData,
      offchainTxData,
      outputNotes: [
        this.txFactory.generateNewNote(outputNotes[0], accountPrivateKey, {
          allowChain: proofData.allowChainFromNote1,
        }),
        this.txFactory.generateNewNote(outputNotes[1], accountPrivateKey, {
          allowChain: proofData.allowChainFromNote2,
        }),
      ],
    };
  }
}
