import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { assetValueToJson } from '@aztec/barretenberg/asset';
import { BridgeId } from '@aztec/barretenberg/bridge_id';
import { depositTxToJson, rollupProviderStatusToJson } from '@aztec/barretenberg/rollup_provider';
import { TxId } from '@aztec/barretenberg/tx_id';
import { EventEmitter } from 'events';
import { coreUserTxToJson } from '../core_tx';
import { noteFromJson, NoteJson, noteToJson } from '../note';
import {
  accountProofInputFromJson,
  AccountProofInputJson,
  accountProofInputToJson,
  joinSplitProofInputFromJson,
  JoinSplitProofInputJson,
  joinSplitProofInputToJson,
  proofOutputFromJson,
  ProofOutputJson,
  proofOutputToJson,
} from '../proofs';
import { CoreSdkInterface } from './core_sdk_interface';
import { CoreSdkOptions } from './core_sdk_options';
import { SdkEvent, sdkStatusToJson } from './sdk_status';

/**
 * Implements the standard CoreSdkSerializedInterface (actually the interface is derived from this, but same thing).
 * Translates the CoreSdkSerializedInterface from serial types such as string, UInt8Array into normal types such
 * as bigint, Buffer etc.
 * It forwards the calls onto an implementation of CoreSdkInterface.
 */
export class CoreSdkServerStub {
  private eventDelegator = new EventEmitter();

  constructor(private core: CoreSdkInterface) {
    // Broadcast all core sdk events.
    for (const e in SdkEvent) {
      const event = (SdkEvent as any)[e];
      this.core.on(event, (...args: any[]) => {
        switch (event) {
          case SdkEvent.UPDATED_USER_STATE: {
            const [userId] = args;
            this.eventDelegator.emit(event, userId.toString());
            break;
          }
          default:
            this.eventDelegator.emit(event, ...args);
        }
      });
    }
  }

  public async init(options: CoreSdkOptions) {
    await this.core.init(options);
  }

  public on(event: SdkEvent, listener: (...args: any[]) => void) {
    this.eventDelegator.on(event, listener);
  }

  public async run() {
    await this.core.run();
  }

  public async destroy() {
    await this.core.destroy();
  }

  public async getLocalStatus() {
    const status = await this.core.getLocalStatus();
    return sdkStatusToJson(status);
  }

  public async getRemoteStatus() {
    const status = await this.core.getRemoteStatus();
    return rollupProviderStatusToJson(status);
  }

  public async isAccountRegistered(accountPublicKey: string, includePending: boolean) {
    return this.core.isAccountRegistered(GrumpkinAddress.fromString(accountPublicKey), includePending);
  }

  public async isAliasRegistered(alias: string, includePending: boolean) {
    return this.core.isAliasRegistered(alias, includePending);
  }

  public async isAliasRegisteredToAccount(
    accountPublicKey: string,
    alias: string,
    isAliasRegisteredToAccount: boolean,
  ) {
    return this.core.isAliasRegisteredToAccount(
      GrumpkinAddress.fromString(accountPublicKey),
      alias,
      isAliasRegisteredToAccount,
    );
  }

  public async getAccountPublicKey(alias: string) {
    const key = await this.core.getAccountPublicKey(alias);
    return key?.toString();
  }

  public async getTxFees(assetId: number) {
    const txFees = await this.core.getTxFees(assetId);
    return txFees.map(fees => fees.map(assetValueToJson));
  }

  public async getDefiFees(bridgeId: string) {
    const fees = await this.core.getDefiFees(BridgeId.fromString(bridgeId));
    return fees.map(assetValueToJson);
  }

  public async getPendingDepositTxs() {
    const txs = await this.core.getPendingDepositTxs();
    return txs.map(depositTxToJson);
  }

  public async createDepositProof(
    assetId: number,
    publicInput: string,
    privateOutput: string,
    depositor: string,
    recipient: string,
    recipientAccountRequired: boolean,
    txRefNo: number,
  ) {
    const proofOutput = await this.core.createDepositProof(
      assetId,
      BigInt(publicInput),
      BigInt(privateOutput),
      EthAddress.fromString(depositor),
      GrumpkinAddress.fromString(recipient),
      recipientAccountRequired,
      txRefNo,
    );
    return proofOutputToJson(proofOutput);
  }

  public async createPaymentProofInput(
    userId: string,
    assetId: number,
    publicInput: string,
    publicOutput: string,
    privateInput: string,
    recipientPrivateOutput: string,
    senderPrivateOutput: string,
    noteRecipient: string | undefined,
    recipientAccountRequired: boolean,
    publicOwner: string | undefined,
    spendingPublicKey: string,
    allowChain: number,
  ) {
    const proofInput = await this.core.createPaymentProofInput(
      GrumpkinAddress.fromString(userId),
      assetId,
      BigInt(publicInput),
      BigInt(publicOutput),
      BigInt(privateInput),
      BigInt(recipientPrivateOutput),
      BigInt(senderPrivateOutput),
      noteRecipient ? GrumpkinAddress.fromString(noteRecipient) : undefined,
      recipientAccountRequired,
      publicOwner ? EthAddress.fromString(publicOwner) : undefined,
      GrumpkinAddress.fromString(spendingPublicKey),
      allowChain,
    );
    return joinSplitProofInputToJson(proofInput);
  }

  public async createPaymentProof(input: JoinSplitProofInputJson, txRefNo: number) {
    const proofOutput = await this.core.createPaymentProof(joinSplitProofInputFromJson(input), txRefNo);
    return proofOutputToJson(proofOutput);
  }

  public async createAccountProofSigningData(
    accountPublicKey: string,
    alias: string,
    migrate: boolean,
    spendingPublicKey: string,
    newAccountPublicKey?: string,
    newSpendingPublicKey1?: string,
    newSpendingPublicKey2?: string,
  ) {
    const signingData = await this.core.createAccountProofSigningData(
      GrumpkinAddress.fromString(accountPublicKey),
      alias,
      migrate,
      GrumpkinAddress.fromString(spendingPublicKey),
      newAccountPublicKey ? GrumpkinAddress.fromString(newAccountPublicKey) : undefined,
      newSpendingPublicKey1 ? GrumpkinAddress.fromString(newSpendingPublicKey1) : undefined,
      newSpendingPublicKey2 ? GrumpkinAddress.fromString(newSpendingPublicKey2) : undefined,
    );
    return new Uint8Array(signingData);
  }

  public async createAccountProofInput(
    userId: string,
    alias: string,
    migrate: boolean,
    spendingPublicKey: string,
    newSpendingPublicKey1: string | undefined,
    newSpendingPublicKey2: string | undefined,
    newAccountPrivateKey: Uint8Array | undefined,
  ) {
    const proofInput = await this.core.createAccountProofInput(
      GrumpkinAddress.fromString(userId),
      alias,
      migrate,
      GrumpkinAddress.fromString(spendingPublicKey),
      newSpendingPublicKey1 ? GrumpkinAddress.fromString(newSpendingPublicKey1) : undefined,
      newSpendingPublicKey2 ? GrumpkinAddress.fromString(newSpendingPublicKey2) : undefined,
      newAccountPrivateKey ? Buffer.from(newAccountPrivateKey) : undefined,
    );
    return accountProofInputToJson(proofInput);
  }

  public async createAccountProof(proofInput: AccountProofInputJson, txRefNo: number) {
    const proofOutput = await this.core.createAccountProof(accountProofInputFromJson(proofInput), txRefNo);
    return proofOutputToJson(proofOutput);
  }

  public async createDefiProofInput(
    userId: string,
    bridgeId: string,
    depositValue: string,
    inputNotes: NoteJson[],
    spendingPublicKey: string,
  ) {
    const proofInput = await this.core.createDefiProofInput(
      GrumpkinAddress.fromString(userId),
      BridgeId.fromString(bridgeId),
      BigInt(depositValue),
      inputNotes.map(n => noteFromJson(n)),
      GrumpkinAddress.fromString(spendingPublicKey),
    );
    return joinSplitProofInputToJson(proofInput);
  }

  public async createDefiProof(input: JoinSplitProofInputJson, txRefNo: number) {
    const proofOutput = await this.core.createDefiProof(joinSplitProofInputFromJson(input), txRefNo);
    return proofOutputToJson(proofOutput);
  }

  public async sendProofs(proofs: ProofOutputJson[]) {
    const txIds = await this.core.sendProofs(proofs.map(proofOutputFromJson));
    return txIds.map(txId => txId.toString());
  }

  public async awaitSynchronised(timeout?: number) {
    await this.core.awaitSynchronised(timeout);
  }

  public async isUserSynching(userId: string) {
    return this.core.isUserSynching(GrumpkinAddress.fromString(userId));
  }

  public async awaitUserSynchronised(userId: string, timeout?: number) {
    await this.core.awaitUserSynchronised(GrumpkinAddress.fromString(userId), timeout);
  }

  public async awaitSettlement(txId: string, timeout?: number) {
    await this.core.awaitSettlement(TxId.fromString(txId), timeout);
  }

  public async awaitDefiDepositCompletion(txId: string, timeout?: number) {
    await this.core.awaitDefiDepositCompletion(TxId.fromString(txId), timeout);
  }

  public async awaitDefiFinalisation(txId: string, timeout?: number) {
    await this.core.awaitDefiFinalisation(TxId.fromString(txId), timeout);
  }

  public async awaitDefiSettlement(txId: string, timeout?: number) {
    await this.core.awaitDefiSettlement(TxId.fromString(txId), timeout);
  }

  public async getDefiInteractionNonce(txId: string) {
    return this.core.getDefiInteractionNonce(TxId.fromString(txId));
  }

  public async userExists(userId: string) {
    return this.core.userExists(GrumpkinAddress.fromString(userId));
  }

  public async getUsers() {
    const accountPublicKeys = await this.core.getUsers();
    return accountPublicKeys.map(pk => pk.toString());
  }

  public async derivePublicKey(privateKey: Uint8Array) {
    const publicKey = await this.core.derivePublicKey(Buffer.from(privateKey));
    return publicKey.toString();
  }

  public async constructSignature(message: Uint8Array, privateKey: Uint8Array) {
    const signature = await this.core.constructSignature(Buffer.from(message), Buffer.from(privateKey));
    return signature.toString();
  }

  public async addUser(accountPrivateKey: Uint8Array, noSync?: boolean) {
    const accountPublicKey = await this.core.addUser(Buffer.from(accountPrivateKey), noSync);
    return accountPublicKey.toString();
  }

  public async removeUser(userId: string) {
    await this.core.removeUser(GrumpkinAddress.fromString(userId));
  }

  public async getUserSyncedToRollup(userId: string) {
    return this.core.getUserSyncedToRollup(GrumpkinAddress.fromString(userId));
  }

  public async getSpendingKeys(userId: string) {
    const keys = await this.core.getSpendingKeys(GrumpkinAddress.fromString(userId));
    return keys.map(k => new Uint8Array(k));
  }

  public async getBalances(userId: string, unsafe?: boolean) {
    const balances = await this.core.getBalances(GrumpkinAddress.fromString(userId), unsafe);
    return balances.map(assetValueToJson);
  }

  public async getBalance(userId: string, assetId: number, unsafe?: boolean) {
    const balance = await this.core.getBalance(GrumpkinAddress.fromString(userId), assetId, unsafe);
    return balance.toString();
  }

  public async getSpendableSum(userId: string, assetId: number, excludePendingNotes?: boolean, unsafe?: boolean) {
    const sum = await this.core.getSpendableSum(
      GrumpkinAddress.fromString(userId),
      assetId,
      excludePendingNotes,
      unsafe,
    );
    return sum.toString();
  }

  public async getSpendableSums(userId: string, excludePendingNotes?: boolean, unsafe?: boolean) {
    const sums = await this.core.getSpendableSums(GrumpkinAddress.fromString(userId), excludePendingNotes, unsafe);
    return sums.map(assetValueToJson);
  }

  public async getMaxSpendableValue(
    userId: string,
    assetId: number,
    numNotes?: number,
    excludePendingNotes?: boolean,
    unsafe?: boolean,
  ) {
    const value = await this.core.getMaxSpendableValue(
      GrumpkinAddress.fromString(userId),
      assetId,
      numNotes,
      excludePendingNotes,
      unsafe,
    );
    return value.toString();
  }

  public async pickNotes(
    userId: string,
    assetId: number,
    value: string,
    excludePendingNotes?: boolean,
    unsafe?: boolean,
  ) {
    return (
      await this.core.pickNotes(GrumpkinAddress.fromString(userId), assetId, BigInt(value), excludePendingNotes, unsafe)
    ).map(noteToJson);
  }

  public async pickNote(
    userId: string,
    assetId: number,
    value: string,
    excludePendingNotes?: boolean,
    unsafe?: boolean,
  ) {
    const note = await this.core.pickNote(
      GrumpkinAddress.fromString(userId),
      assetId,
      BigInt(value),
      excludePendingNotes,
      unsafe,
    );
    return note ? noteToJson(note) : undefined;
  }

  public async getUserTxs(userId: string) {
    const txs = await this.core.getUserTxs(GrumpkinAddress.fromString(userId));
    return txs.map(coreUserTxToJson);
  }
}
