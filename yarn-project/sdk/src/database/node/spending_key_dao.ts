import { GrumpkinAddress } from '@polyaztec/barretenberg/address';
import { Column, Entity, Index, PrimaryColumn } from 'typeorm';
import { SpendingKey } from '../database.js';
import { grumpkinAddressTransformer } from './transformer.js';

@Entity({ name: 'spendingKey' })
@Index(['key', 'userId'], { unique: true })
export class SpendingKeyDao implements SpendingKey {
  constructor(init?: SpendingKey) {
    Object.assign(this, init);
  }

  @PrimaryColumn()
  public key!: Buffer;

  @PrimaryColumn('blob', { transformer: [grumpkinAddressTransformer] })
  public userId!: GrumpkinAddress;

  @Column()
  public treeIndex!: number;

  @Column()
  public hashPath!: Buffer;
}
