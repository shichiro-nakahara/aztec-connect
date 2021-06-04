import { BarretenbergWasm } from '../../wasm';
import { Grumpkin } from '../../ecc/grumpkin';
import { GrumpkinAddress } from '../../address';
import { NoteAlgorithms } from '.';
import { TreeNote } from '../tree_note';
import { TreeClaimNote } from '../tree_claim_note';
// import createDebug from 'debug';

// const debug = createDebug('bb:decrypt_test');

describe('compute_nullifier', () => {
  let grumpkin!: Grumpkin;
  let noteAlgos!: NoteAlgorithms;
  let pubKey: GrumpkinAddress;

  const privateKey = Buffer.from('0b9b3adee6b3d81b28a0886b2a8415c7da31291a5e96bb7a56639e177d301beb', 'hex');
  const noteSecret = Buffer.from('0000000011111111000000001111111100000000111111110000000011111111', 'hex');

  beforeAll(async () => {
    const barretenberg = await BarretenbergWasm.new();
    grumpkin = new Grumpkin(barretenberg);
    noteAlgos = new NoteAlgorithms(barretenberg);
    pubKey = new GrumpkinAddress(grumpkin.mul(Grumpkin.one, privateKey));
  });

  it('should compute correct nullifier', async () => {
    const inputNote1 = new TreeNote(pubKey, BigInt(100), 0, 0, noteSecret);
    const inputNote2 = new TreeNote(pubKey, BigInt(50), 0, 0, noteSecret);
    inputNote1.noteSecret = noteSecret;
    inputNote2.noteSecret = noteSecret;

    const inputNote1Enc = noteAlgos.encryptNote(inputNote1.toBuffer());
    const inputNote2Enc = noteAlgos.encryptNote(inputNote2.toBuffer());

    const nullifier1 = noteAlgos.computeNoteNullifier(inputNote1Enc, 1, privateKey);
    const nullifier2 = noteAlgos.computeNoteNullifier(inputNote2Enc, 0, privateKey);

    const expected1 = '192bcf51d4d8384907f17ab851a19299615c5bd9d048a4b5633bf884580a6ac2';
    const expected2 = '1e1e11ce93adb2af718da9b135956f23240fe9db91f646e00862635a0a488de4';

    expect(nullifier1.toString('hex')).toEqual(expected1);
    expect(nullifier2.toString('hex')).toEqual(expected2);
  });

  it('should encrypt claim note and compute its nullifier', async () => {
    const inputNote = new TreeClaimNote(BigInt(100), BigInt(234), noteSecret, 0);
    const inputNoteEnc = noteAlgos.encryptClaimNote(inputNote.toBuffer(), pubKey, 0);
    const nullifier = noteAlgos.computeClaimNoteNullifier(inputNoteEnc, 1);
    expect(nullifier).toEqual(Buffer.from('06c772265c0e55bd0b15ec6fe660926161954775a52926457a44e1eafe501e03', 'hex'));
  });
});
