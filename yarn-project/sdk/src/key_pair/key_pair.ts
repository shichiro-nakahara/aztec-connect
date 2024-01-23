import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { SchnorrSignature } from '@polyaztec/barretenberg/crypto';

export interface KeyPair {
  getPublicKey(): GrumpkinAddress;
  getPrivateKey(): Promise<Buffer>;
  signMessage(message: Buffer): Promise<SchnorrSignature>;
}
