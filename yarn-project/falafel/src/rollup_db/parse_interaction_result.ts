import { DefiInteractionEvent } from '@polyaztec/barretenberg/block_source';
import { Deserializer } from '@polyaztec/barretenberg/serialize';

export const parseInteractionResult = (buf: Buffer) => {
  if (!buf.length) {
    return [];
  }
  const des = new Deserializer(buf);
  return des.deserializeArray(DefiInteractionEvent.deserialize);
};
