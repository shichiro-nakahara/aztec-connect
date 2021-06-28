import { toBigIntBE } from '../bigint_buffer';
import { AccountId } from '../account_id';
import { ViewingKey } from '../viewing_key';
import { BarretenbergWasm } from '../wasm';
import { BarretenbergWorker } from '../wasm/worker';
import { ClaimNoteTxData } from './claim_note_tx_data';
import { DefiInteractionNote } from './defi_interaction_note';
import { TreeClaimNote } from './tree_claim_note';
import { TreeNote } from './tree_note';

export class NoteAlgorithms {
  constructor(private wasm: BarretenbergWasm, private worker: BarretenbergWorker = wasm as any) {}

  public computeNoteNullifier(noteCommitment: Buffer, index: number, accountPrivateKey: Buffer, real = true) {
    this.wasm.transferToHeap(noteCommitment, 0);
    this.wasm.transferToHeap(accountPrivateKey, 64);
    this.wasm.call('notes__compute_value_note_nullifier', 0, 64, index, real, 0);
    return Buffer.from(this.wasm.sliceMemory(0, 32));
  }

  public computeNoteNullifierBigInt(noteCommitment: Buffer, index: number, accountPrivateKey: Buffer, real = true) {
    return toBigIntBE(this.computeNoteNullifier(noteCommitment, index, accountPrivateKey, real));
  }

  public commitNote(note: TreeNote) {
    const noteBuf = note.toBuffer();
    const mem = this.wasm.call('bbmalloc', noteBuf.length);
    this.wasm.transferToHeap(noteBuf, mem);
    this.wasm.call('notes__commit_value_note', mem, 0);
    this.wasm.call('bbfree', mem);
    return Buffer.from(this.wasm.sliceMemory(0, 64));
  }

  public computePartialState(note: ClaimNoteTxData, owner: AccountId) {
    const noteBuf = note.toBuffer();
    const mem = this.wasm.call('bbmalloc', noteBuf.length + 64);
    this.wasm.transferToHeap(noteBuf, mem);
    this.wasm.transferToHeap(owner.publicKey.toBuffer(), mem + noteBuf.length);
    this.wasm.call('notes__create_partial_value_note', mem, mem + noteBuf.length, owner.nonce, 0);
    this.wasm.call('bbfree', mem);
    return Buffer.from(this.wasm.sliceMemory(0, 64));
  }

  public commitClaimNote(note: TreeClaimNote) {
    const noteBuf = note.toBuffer();
    const mem = this.wasm.call('bbmalloc', noteBuf.length);
    this.wasm.transferToHeap(noteBuf, mem);
    this.wasm.call('notes__commit_claim_note', mem, 0);
    this.wasm.call('bbfree', mem);
    return Buffer.from(this.wasm.sliceMemory(0, 64));
  }

  public computeClaimNoteNullifier(noteCommitment: Buffer, index: number) {
    this.wasm.transferToHeap(noteCommitment, 0);
    this.wasm.call('notes__compute_claim_note_nullifier', 0, index, 0);
    return Buffer.from(this.wasm.sliceMemory(0, 32));
  }

  public commitDefiInteractionNote(note: DefiInteractionNote) {
    const noteBuf = note.toBuffer();
    const mem = this.wasm.call('bbmalloc', noteBuf.length);
    this.wasm.transferToHeap(noteBuf, mem);
    this.wasm.call('notes__commit_defi_interaction_note', mem, 0);
    this.wasm.call('bbfree', mem);
    return Buffer.from(this.wasm.sliceMemory(0, 64));
  }

  public async batchDecryptNotes(keysBuf: Buffer, privateKey: Buffer) {
    const decryptedNoteLength = 41;
    const numKeys = keysBuf.length / ViewingKey.SIZE;

    const mem = await this.worker.call('bbmalloc', keysBuf.length + privateKey.length);
    await this.worker.transferToHeap(keysBuf, mem);
    await this.worker.transferToHeap(privateKey, mem + keysBuf.length);

    await this.worker.call('notes__batch_decrypt_notes', mem, mem + keysBuf.length, numKeys, mem);
    const dataBuf: Buffer = Buffer.from(await this.worker.sliceMemory(mem, mem + numKeys * decryptedNoteLength));
    await this.worker.call('bbfree', mem);
    return dataBuf;
  }
}
