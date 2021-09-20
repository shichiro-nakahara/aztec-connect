import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { AssetId } from '@aztec/barretenberg/asset';
import { BridgeId } from '@aztec/barretenberg/bridge_id';
import { JoinSplitTx } from '@aztec/barretenberg/client_proofs';
import { Grumpkin } from '@aztec/barretenberg/ecc';
import { ClaimNoteTxData, TreeNote } from '@aztec/barretenberg/note_algorithms';
import { WorldState } from '@aztec/barretenberg/world_state';
import { Database } from '../../database';
import { AccountAliasId, AccountId } from '../../user';
import { UserState } from '../../user_state';

export class JoinSplitTxFactory {
  constructor(private worldState: WorldState, private grumpkin: Grumpkin, private db: Database) {}

  public async createJoinSplitTx(
    userState: UserState,
    publicInput: bigint,
    publicOutput: bigint,
    privateInput: bigint,
    recipientPrivateOutput: bigint,
    senderPrivateOutput: bigint,
    defiDepositValue: bigint,
    assetId: AssetId,
    signingPubKey: GrumpkinAddress,
    newNoteOwner?: AccountId,
    publicOwner?: EthAddress,
    bridgeId?: BridgeId,
  ) {
    if (publicInput && publicOutput) {
      throw new Error('Public values cannot be both greater than zero.');
    }

    if (publicOutput + recipientPrivateOutput + senderPrivateOutput > publicInput + privateInput) {
      throw new Error('Total output cannot be larger than total input.');
    }

    if (publicInput + publicOutput && !publicOwner) {
      throw new Error('Public owner undefined.');
    }

    if (recipientPrivateOutput && !newNoteOwner) {
      throw new Error('Note recipient undefined.');
    }

    const isDefiBridge = defiDepositValue > BigInt(0);

    const { id, aliasHash, publicKey, nonce } = userState.getUser();
    const accountIndex = nonce !== 0 ? await this.db.getUserSigningKeyIndex(id, signingPubKey) : 0;
    if (accountIndex === undefined) {
      throw new Error('Unknown signing key.');
    }

    const accountAliasId = aliasHash ? new AccountAliasId(aliasHash, nonce) : AccountAliasId.random();
    const accountPath = await this.worldState.getHashPath(accountIndex);

    const notes = privateInput ? await userState.pickNotes(assetId, privateInput) : [];
    if (!notes) {
      throw new Error(`Failed to find no more than 2 notes that sum to ${privateInput}.`);
    }

    const numInputNotes = notes.length;
    const totalNoteInputValue = notes.reduce((sum, note) => sum + note.value, BigInt(0));
    const inputNoteIndices = notes.map(n => n.index);
    const inputNotes = notes.map(n => new TreeNote(n.owner.publicKey, n.value, n.assetId, n.owner.nonce, n.secret, n.creatorPubKey));
    const maxNoteIndex = Math.max(...inputNoteIndices, 0);

    // Add gibberish notes to ensure we have two notes.
    for (let i = notes.length; i < 2; ++i) {
      inputNoteIndices.push(maxNoteIndex + i); // notes can't have the same index
      inputNotes.push(
        TreeNote.createFromEphPriv(publicKey, BigInt(0), assetId, nonce, this.createEphemeralPrivKey(), this.grumpkin),
      );
    }

    const inputNotePaths = await Promise.all(inputNoteIndices.map(async idx => this.worldState.getHashPath(idx)));

    const changeValue = totalNoteInputValue > privateInput ? totalNoteInputValue - privateInput : BigInt(0);
    const outputNotes = [
      this.createNote(assetId, recipientPrivateOutput, newNoteOwner || id),
      this.createNote(assetId, changeValue + senderPrivateOutput, id),
    ];
    const claimNote = isDefiBridge
      ? new ClaimNoteTxData(defiDepositValue, bridgeId!, outputNotes[1].note.noteSecret)
      : ClaimNoteTxData.EMPTY;

    const dataRoot = this.worldState.getRoot();

    // For now, we will use the account key as the signing key (no account note required).
    const { privateKey } = userState.getUser();

    const tx = new JoinSplitTx(
      publicInput,
      publicOutput,
      publicOwner || EthAddress.ZERO,
      assetId,
      numInputNotes,
      inputNoteIndices,
      dataRoot,
      inputNotePaths,
      inputNotes,
      outputNotes.map(n => n.note),
      claimNote,
      privateKey,
      accountAliasId,
      accountIndex,
      accountPath,
      signingPubKey,
    );

    const viewingKeys = isDefiBridge
      ? [outputNotes[1].viewingKey]
      : [outputNotes[0].viewingKey, outputNotes[1].viewingKey];

    return { tx, viewingKeys };
  }

  private createNote(assetId: AssetId, value: bigint, owner: AccountId, sender?: AccountId) {
    const ephKey = this.createEphemeralPrivKey();
    const creatorPubKey : Buffer = sender ? sender.publicKey.x() : Buffer.alloc(32);
    const note = TreeNote.createFromEphPriv(owner.publicKey, value, assetId, owner.nonce, ephKey, this.grumpkin, TreeNote.LATEST_VERSION, creatorPubKey);
    const viewingKey = note.getViewingKey(ephKey, this.grumpkin);
    return { note, viewingKey };
  }

  private createEphemeralPrivKey() {
    return this.grumpkin.getRandomFr();
  }
}
