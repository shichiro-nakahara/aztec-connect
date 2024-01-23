import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { toBigIntBE } from '@polyaztec/barretenberg/bigint_buffer';
import { BridgeCallData } from '@polyaztec/barretenberg/bridge_call_data';
import { JoinSplitProver, ProofData, ProofId } from '@polyaztec/barretenberg/client_proofs';
import { Grumpkin } from '@polyaztec/barretenberg/ecc';
import { createDebugLogger } from '@polyaztec/barretenberg/log';
import { NoteAlgorithms } from '@polyaztec/barretenberg/note_algorithms';
import { OffchainDefiDepositData } from '@polyaztec/barretenberg/offchain_tx_data';
import { TxId } from '@polyaztec/barretenberg/tx_id';
import { WorldState } from '@polyaztec/barretenberg/world_state';
import { CoreDefiTx } from '../core_tx/index.js';
import { Database } from '../database/index.js';
import { Note, treeNoteToNote } from '../note/index.js';
import { UserData } from '../user/index.js';
import { JoinSplitTxFactory } from './join_split_tx_factory.js';
import { JoinSplitProofInput } from './proof_input.js';

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
    bridgeCallData: BridgeCallData,
    depositValue: bigint,
    inputNotes: Note[],
    spendingPublicKey: GrumpkinAddress,
  ) {
    const assetId = bridgeCallData.inputAssetIdA;
    const newNoteOwnerAccountRequired = !spendingPublicKey.equals(user.accountPublicKey);
    const proofInput = await this.txFactory.createTx(
      user,
      ProofId.DEFI_DEPOSIT,
      assetId,
      inputNotes,
      spendingPublicKey,
      {
        bridgeCallData,
        defiDepositValue: depositValue,
        newNoteOwner: user.accountPublicKey,
        newNoteOwnerAccountRequired,
      },
    );

    const signingData = await this.prover.computeSigningData(proofInput.tx);

    return { ...proofInput, signingData };
  }

  public async createProof(
    { accountPublicKey, accountPrivateKey }: UserData,
    { tx, signature, partialStateSecretEphPubKey, viewingKeys }: JoinSplitProofInput,
    txRefNo: number,
    timeout?: number,
  ) {
    debug('creating proof...');
    const start = new Date().getTime();
    tx.signature = signature!;
    const proof = await this.prover.createProof(tx, timeout);
    debug(`created proof: ${new Date().getTime() - start}ms`);
    debug(`proof size: ${proof.length}`);

    const proofData = new ProofData(proof);
    const txId = new TxId(proofData.txId);
    const {
      outputNotes,
      claimNote: { value: depositValue, bridgeCallData, partialStateSecret },
    } = tx;
    const txFee = toBigIntBE(proofData.txFee);
    const accountRequired = outputNotes[1].accountRequired;
    const partialState = this.noteAlgos.valueNotePartialCommitment(
      partialStateSecret,
      accountPublicKey,
      accountRequired,
    );
    const coreTx = new CoreDefiTx(
      txId,
      accountPublicKey,
      bridgeCallData,
      depositValue,
      txFee,
      txRefNo,
      new Date(),
      partialState,
      partialStateSecret,
    );
    const offchainTxData = new OffchainDefiDepositData(
      bridgeCallData,
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
        treeNoteToNote(outputNotes[0], accountPrivateKey, this.noteAlgos, {
          allowChain: proofData.allowChainFromNote1,
        }),
        treeNoteToNote(outputNotes[1], accountPrivateKey, this.noteAlgos, {
          allowChain: proofData.allowChainFromNote2,
        }),
      ],
    };
  }
}
