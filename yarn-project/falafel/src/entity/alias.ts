import { Column, PrimaryColumn, Entity } from 'typeorm';
import { bufferColumn } from './buffer_column.js';

@Entity({ name: 'alias' })
export class AliasDao {
  public constructor(init?: AliasDao) {
    Object.assign(this, init);
  }

  @PrimaryColumn(...bufferColumn({ length: 32 }))
  public hash!: Buffer;

  @Column()
  public length!: number;
}
