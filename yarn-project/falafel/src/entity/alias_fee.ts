import { Column, PrimaryColumn, Entity } from 'typeorm';
import { bufferColumn } from './buffer_column.js';
import { bigintTransformer } from './transformer.js';

@Entity({ name: 'alias_fee' })
export class AliasFeeDao {
  public constructor(init?: AliasFeeDao) {
    Object.assign(this, init);
  }

  @PrimaryColumn(...bufferColumn({ length: 32 }))
  public aliasHash!: Buffer;

  @Column()
  public assetId!: number;

  @Column('text', { transformer: [bigintTransformer], default: '0' })
  public fee = BigInt(0);
}
