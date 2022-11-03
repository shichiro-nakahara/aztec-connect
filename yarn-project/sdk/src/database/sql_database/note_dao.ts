import { GrumpkinAddress } from '@aztec/barretenberg/address';
import { TreeNote } from '@aztec/barretenberg/note_algorithms';
import { AfterInsert, AfterLoad, AfterUpdate, Column, Entity, Index, PrimaryColumn } from 'typeorm';
import { Note } from '../../note/index.js';
import { bigintTransformer, grumpkinAddressTransformer } from './transformer.js';

@Entity({ name: 'note' })
export class NoteDao {
  @PrimaryColumn()
  public commitment!: Buffer;

  @Index({ unique: true })
  @Column()
  public nullifier!: Buffer;

  @Column()
  public noteSecret!: Buffer;

  @Column('blob', { transformer: [grumpkinAddressTransformer] })
  public owner!: GrumpkinAddress;

  @Column()
  public accountRequired!: boolean;

  @Column()
  public creatorPubKey!: Buffer;

  @Column()
  public inputNullifier!: Buffer;

  @Column()
  public assetId!: number;

  @Column('text', { transformer: [bigintTransformer] })
  public value!: bigint;

  @Column()
  public allowChain!: boolean;

  @Index({ unique: false })
  @Column({ nullable: true })
  public index?: number;

  @Index({ unique: false })
  @Column()
  public nullified!: boolean;

  @Column({ nullable: true })
  public hashPath?: Buffer;

  @AfterLoad()
  @AfterInsert()
  @AfterUpdate()
  afterLoad() {
    if (!this.hashPath) {
      delete this.hashPath;
    }
    if (this.index === null) {
      delete this.index;
    }
  }
}

export const noteToNoteDao = ({
  treeNote: { noteSecret, ownerPubKey, accountRequired, creatorPubKey, inputNullifier, assetId },
  commitment,
  nullifier,
  value,
  allowChain,
  index,
  nullified,
  hashPath,
}: Note) => ({
  commitment,
  nullifier,
  noteSecret,
  owner: ownerPubKey,
  accountRequired,
  creatorPubKey,
  inputNullifier,
  assetId,
  value,
  allowChain,
  nullified,
  index,
  hashPath,
});

export const noteDaoToNote = ({
  commitment,
  nullifier,
  noteSecret,
  owner,
  accountRequired,
  creatorPubKey,
  inputNullifier,
  assetId,
  value,
  allowChain,
  index,
  nullified,
  hashPath,
}: NoteDao) =>
  new Note(
    new TreeNote(owner, value, assetId, accountRequired, noteSecret, creatorPubKey, inputNullifier),
    commitment,
    nullifier,
    allowChain,
    nullified,
    index,
    hashPath,
  );
