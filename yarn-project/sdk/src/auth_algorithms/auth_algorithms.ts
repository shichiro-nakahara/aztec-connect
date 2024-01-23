import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { DecryptedNote } from '@polyaztec/barretenberg/note_algorithms';
import { JoinSplitTxInput } from '../proofs/proof_input/index.js';

export interface AuthAlgorithms {
  computeValueNoteNullifier(commitment: Buffer, gibberish?: boolean): Promise<Buffer>;
  deriveNoteSecret(ecdhPubKey: GrumpkinAddress): Promise<Buffer>;
  decryptViewingKeys(viewingKeysBuf: Buffer): Promise<(DecryptedNote | undefined)[]>;
  createJoinSplitProofSigningData(tx: JoinSplitTxInput): Promise<Buffer>;
}
