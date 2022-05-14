import { AccountAliasId, AccountId } from '@aztec/barretenberg/account_id';
import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { BridgeId } from '@aztec/barretenberg/bridge_id';
import { JoinSplitTx, ProofId } from '@aztec/barretenberg/client_proofs';
import { randomBytes } from '@aztec/barretenberg/crypto';
import { Grumpkin } from '@aztec/barretenberg/ecc';
import { HashPath } from '@aztec/barretenberg/merkle_tree';
import { ClaimNoteTxData, deriveNoteSecret, NoteAlgorithms, TreeNote } from '@aztec/barretenberg/note_algorithms';
import { WorldState, WorldStateConstants } from '@aztec/barretenberg/world_state';
import { Database } from '../database';
import { Note } from '../note';
import { UserData } from '../user';

export class JoinSplitTxFactory {
  constructor(
    private noteAlgos: NoteAlgorithms,
    private worldState: WorldState,
    private grumpkin: Grumpkin,
    private db: Database,
  ) {}

  async createTx(
    user: UserData,
    proofId: ProofId,
    assetId: number,
    inputNotes: Note[],
    signingPubKey: GrumpkinAddress,
    {
      publicValue = BigInt(0),
      publicOwner = EthAddress.ZERO,
      outputNoteValue1 = BigInt(0),
      outputNoteValue2 = BigInt(0),
      newNoteOwner = user.id,
      bridgeId = BridgeId.ZERO,
      defiDepositValue = BigInt(0),
      allowChain = 0,
    } = {},
  ) {
    if (inputNotes.reduce((count, n) => count + (n.allowChain ? 1 : 0), 0) > 1) {
      throw new Error('Cannot chain from more than one pending note.');
    }

    const { id: accountId, aliasHash, privateKey, publicKey, accountNonce } = user;
    if (accountNonce && !aliasHash) {
      throw new Error('Alias hash not found.');
    }

    const accountAliasId = aliasHash ? new AccountAliasId(aliasHash, accountNonce) : AccountAliasId.random();
    const { path: accountPath, index: accountIndex } = await this.getAccountPathAndIndex(accountId, signingPubKey);

    const numInputNotes = inputNotes.length;
    const notes = [...inputNotes];
    const inputTreeNotes = notes.map(n => n.treeNote);

    // Add gibberish notes to ensure we have two notes.
    for (let i = notes.length; i < 2; ++i) {
      const treeNote = TreeNote.createFromEphPriv(
        publicKey, // owner
        BigInt(0), // value
        assetId,
        accountNonce,
        randomBytes(32), // inputNullifier - this is a dummy input nullifier for the dummy note.
        this.createEphemeralPrivKey(),
        this.grumpkin,
      );
      inputTreeNotes.push(treeNote);
      notes.push(this.generateNewNote(treeNote, privateKey, { gibberish: true }));
    }

    const inputNoteIndices = notes.map(n => n.index || 0);
    // For each input note we need to
    // Determine if there is a hash path stored with the note
    // If there is then concatenate that path with the hash path returned from the data tree for that note's subtree
    // If there isn't then generate a 'zero' hash path of the full data tree depth
    const inputNotePaths = await Promise.all(
      notes.map(async (note, index) => {
        if (note.hashPath) {
          const immutableHashPath = HashPath.fromBuffer(note.hashPath);
          return await this.worldState.buildFullHashPath(inputNoteIndices[index], immutableHashPath);
        }
        return this.worldState.buildZeroHashPath(WorldStateConstants.DATA_TREE_DEPTH);
      }),
    );
    const inputNoteNullifiers = notes.map(n => n.nullifier);

    const newNotes = [
      this.createNote(assetId, outputNoteValue1, newNoteOwner, inputNoteNullifiers[0]),
      this.createNote(assetId, outputNoteValue2, accountId, inputNoteNullifiers[1]),
    ];
    const outputNotes = newNotes.map(n => n.note);

    const claimNote =
      proofId === ProofId.DEFI_DEPOSIT
        ? this.createClaimNote(bridgeId, defiDepositValue, accountId, inputNoteNullifiers[0])
        : { note: ClaimNoteTxData.EMPTY, ephPubKey: undefined };

    const propagatedInputIndex = 1 + inputNotes.findIndex(n => n.allowChain);
    const backwardLink = propagatedInputIndex ? inputNotes[propagatedInputIndex - 1].commitment : Buffer.alloc(32);

    const dataRoot = this.worldState.getRoot();

    // For now, we will use the account key as the signing key (no account note required).
    const tx = new JoinSplitTx(
      proofId,
      publicValue,
      publicOwner,
      assetId,
      numInputNotes,
      inputNoteIndices,
      dataRoot,
      inputNotePaths,
      inputTreeNotes,
      outputNotes,
      claimNote.note,
      privateKey,
      accountAliasId,
      accountIndex,
      accountPath,
      signingPubKey,
      backwardLink,
      allowChain,
    );

    const viewingKeys =
      proofId === ProofId.DEFI_DEPOSIT ? [newNotes[1].viewingKey] : [newNotes[0].viewingKey, newNotes[1].viewingKey];

    return { tx, viewingKeys, partialStateSecretEphPubKey: claimNote.ephPubKey };
  }

  private async getAccountPathAndIndex(accountId: AccountId, signingPubKey: GrumpkinAddress) {
    if (accountId.accountNonce === 0) {
      return {
        path: this.worldState.buildZeroHashPath(WorldStateConstants.DATA_TREE_DEPTH),
        index: 0,
      };
    } else {
      const signingKey = await this.db.getUserSigningKey(accountId, signingPubKey);
      if (signingKey === undefined) {
        throw new Error('Unknown signing key.');
      }
      const immutableHashPath = HashPath.fromBuffer(signingKey.hashPath);
      const path = await this.worldState.buildFullHashPath(signingKey.treeIndex, immutableHashPath);
      return {
        path,
        index: signingKey.treeIndex,
      };
    }
  }

  generateNewNote(treeNote: TreeNote, privateKey: Buffer, { allowChain = false, gibberish = false } = {}) {
    const commitment = this.noteAlgos.valueNoteCommitment(treeNote);
    const nullifier = this.noteAlgos.valueNoteNullifier(commitment, privateKey, !gibberish);
    return new Note(treeNote, commitment, nullifier, allowChain, false);
  }

  private createNote(assetId: number, value: bigint, owner: AccountId, inputNullifier: Buffer, sender?: AccountId) {
    const { ephPrivKey } = this.createEphemeralKeyPair();
    const creatorPubKey: Buffer = sender ? sender.publicKey.x() : Buffer.alloc(32);
    const note = TreeNote.createFromEphPriv(
      owner.publicKey,
      value,
      assetId,
      owner.accountNonce,
      inputNullifier,
      ephPrivKey,
      this.grumpkin,
      creatorPubKey,
    );
    const viewingKey = note.createViewingKey(ephPrivKey, this.grumpkin);
    return { note, viewingKey };
  }

  private createClaimNote(bridgeId: BridgeId, value: bigint, owner: AccountId, inputNullifier: Buffer) {
    const { ephPrivKey, ephPubKey } = this.createEphemeralKeyPair();
    const noteSecret = deriveNoteSecret(owner.publicKey, ephPrivKey, this.grumpkin);
    const note = new ClaimNoteTxData(value, bridgeId, noteSecret, inputNullifier);
    // ephPubKey is returned for the defi deposit use case, where we'd like to avoid creating a viewing key for the
    // partial claim note's partialState, since all we want to transmit is the ephPubKey (which we can do via offchain tx data).
    return { note, ephPubKey };
  }

  private createEphemeralPrivKey() {
    return this.grumpkin.getRandomFr();
  }

  private createEphemeralKeyPair() {
    const ephPrivKey = this.grumpkin.getRandomFr();
    const ephPubKey = new GrumpkinAddress(this.grumpkin.mul(Grumpkin.one, ephPrivKey));
    return { ephPrivKey, ephPubKey };
  }
}
