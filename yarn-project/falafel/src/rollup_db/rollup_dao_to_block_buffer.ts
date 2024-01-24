import { toBigIntBE } from '@polyaztec/barretenberg/bigint_buffer';
import { Block } from '@polyaztec/barretenberg/block_source';
import { RollupDao } from '../entity/rollup.js';
import { parseInteractionResult } from './parse_interaction_result.js';

export const rollupDaoToBlockBuffer = (dao: RollupDao) => {
  return new Block(
    dao.ethTxHash!,
    dao.mined!,
    dao.id,
    dao.rollupProof.rollupSize,
    dao.rollupProof.encodedProofData!,
    dao.rollupProof.txs.map(tx => tx.offchainTxData),
    parseInteractionResult(dao.interactionResult!),
    dao.gasUsed!,
    toBigIntBE(dao.gasPrice!),
    dao.subtreeRoot,
  ).toBuffer();
};
