import { randomBytes } from 'crypto';
import { AccountAliasId } from '../../account_id';
import { GrumpkinAddress } from '../../address';
import { HashPath } from '../../merkle_tree';
import { AccountTx } from './account_tx';

describe('account tx', () => {
  it('should convert to and from buffer', () => {
    const tx = new AccountTx(
      randomBytes(32),
      GrumpkinAddress.randomAddress(),
      GrumpkinAddress.randomAddress(),
      GrumpkinAddress.randomAddress(),
      GrumpkinAddress.randomAddress(),
      AccountAliasId.random(),
      true,
      123,
      new HashPath(
        Array(4)
          .fill(0)
          .map(() => [randomBytes(32), randomBytes(32)]),
      ),
      GrumpkinAddress.randomAddress(),
    );
    const buf = tx.toBuffer();
    expect(AccountTx.fromBuffer(buf)).toEqual(tx);
  });
});
