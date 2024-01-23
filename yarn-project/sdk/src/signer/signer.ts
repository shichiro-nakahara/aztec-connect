import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { SchnorrSignature } from '@polyaztec/barretenberg/crypto';

export interface Signer {
  getPublicKey(): GrumpkinAddress;
  signMessage(message: Buffer): Promise<SchnorrSignature>;
}
