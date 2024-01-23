import { GrumpkinAddress } from '@polyaztec/barretenberg/address';

export * from './recovery_payload.js';

export interface UserData {
  accountPublicKey: GrumpkinAddress;
  accountPrivateKey: Buffer;
  syncedToRollup: number;
}
