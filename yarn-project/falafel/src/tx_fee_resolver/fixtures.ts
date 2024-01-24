import { toBufferBE } from '@polyaztec/barretenberg/bigint_buffer';
import { TxType } from '@polyaztec/barretenberg/blockchain';
import { numToUInt32BE } from '@polyaztec/barretenberg/serialize';
import { randomBytes } from 'crypto';
import { TxDao } from '../entity/index.js';

const txTypeToProofId = (txType: TxType) => (txType < TxType.WITHDRAW_HIGH_GAS ? txType + 1 : txType);

export const mockTx = (txFeeAssetId: number, txType: TxType, txFee: bigint) =>
  ({
    txType,
    proofData: Buffer.concat([
      numToUInt32BE(txTypeToProofId(txType), 32), // proofId
      randomBytes(32), // note1
      randomBytes(32), // note2
      randomBytes(32), // nullifier1
      randomBytes(32), // nullifier2
      randomBytes(32), // publicValue
      randomBytes(32), // publicOwner
      randomBytes(32), // publicAssetId
      randomBytes(32), // merkle root
      toBufferBE(txFee, 32),
      numToUInt32BE(txFeeAssetId, 32),
      randomBytes(32), // bridge call data
      randomBytes(32), // defi deposit value
      randomBytes(32), // defi root
      randomBytes(32), // propagated input index
      randomBytes(32), // backward link
      randomBytes(32), // allow chain
    ]),
  } as any as TxDao);
