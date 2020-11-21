import { randomBytes } from 'crypto';

export class TxHash {
  constructor(private buffer: Buffer) {
    if (buffer.length !== 32) {
      throw new Error('Invalid hash buffer.');
    }
  }

  public static fromString(hash: string) {
    return new TxHash(Buffer.from(hash.replace(/^0x/i, ''), 'hex'));
  }

  public static random() {
    return new TxHash(randomBytes(32));
  }

  equals(rhs: TxHash) {
    return this.toBuffer().equals(rhs.toBuffer());
  }

  toBuffer() {
    return this.buffer;
  }

  toString() {
    return `0x${this.toBuffer().toString('hex')}`;
  }
}
