import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { toBigIntBE } from '@polyaztec/barretenberg/bigint_buffer';
import { JoinSplitProver, JoinSplitTx, ProofData } from '@polyaztec/barretenberg/client_proofs';
import { SchnorrSignature } from '@polyaztec/barretenberg/crypto';
import { createDebugLogger } from '@polyaztec/barretenberg/log';
import { NoteAlgorithms } from '@polyaztec/barretenberg/note_algorithms';
import { OffchainDefiDepositData } from '@polyaztec/barretenberg/offchain_tx_data';
import { TxId } from '@polyaztec/barretenberg/tx_id';
import { ViewingKey } from '@polyaztec/barretenberg/viewing_key';
import { CoreDefiTx } from '../../core_tx/index.js';

const debug = createDebugLogger('bb:defi_proof_creator');

export class DefiProofCreator {
  constructor(private prover: JoinSplitProver, private noteAlgos: NoteAlgorithms) {}

  public async createProof(
    tx: JoinSplitTx,
    viewingKey: ViewingKey,
    partialStateSecretEphPubKey: GrumpkinAddress,
    signature: SchnorrSignature,
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
    const { ownerPubKey: accountPublicKey, accountRequired } = outputNotes[1];
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
      partialStateSecretEphPubKey,
      depositValue,
      txFee,
      viewingKey,
      txRefNo,
    );

    return {
      tx: coreTx,
      proofData,
      offchainTxData,
    };
  }
}
