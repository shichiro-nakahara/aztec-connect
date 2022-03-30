import {
  AccountId,
  AssetValue,
  AztecSdk,
  AztecSdkUser,
  createAztecSdk,
  CreateSdkOptions,
  EthAddress,
  EthereumProvider,
  EthereumSigner,
  GrumpkinAddress,
  SdkEvent,
  TxId,
  Web3Signer,
} from '@aztec/sdk';
import { EventEmitter } from 'events';
import { Database } from './database';
import { EthereumSdkUser } from './ethereum_sdk_user';

export * from './ethereum_sdk_user';

export async function createEthSdk(ethereumProvider: EthereumProvider, sdkOptions: CreateSdkOptions) {
  const aztecSdk = await createAztecSdk(ethereumProvider, sdkOptions);

  const db = new Database();
  await db.init();

  const ethSigner = new Web3Signer(ethereumProvider);

  return new EthereumSdk(aztecSdk, db, ethSigner);
}

export class EthereumSdk extends EventEmitter {
  constructor(private aztecSdk: AztecSdk, private db: Database, private ethSigner: EthereumSigner) {
    super();
  }

  public async init() {
    // Forward all aztecSdk events.
    for (const e in SdkEvent) {
      const event = (SdkEvent as any)[e];
      this.aztecSdk.on(event, (...args: any[]) => this.emit(event, ...args));
    }

    await this.aztecSdk.run();

    await this.aztecSdk.awaitSynchronised();
  }

  public async destroy() {
    await this.aztecSdk.destroy();
    await this.db.close();
    this.removeAllListeners();
  }

  public async isUserSynching(userId: AccountId) {
    return this.aztecSdk.isUserSynching(userId);
  }

  public async awaitUserSynchronised(accountId: AccountId) {
    return this.aztecSdk.awaitUserSynchronised(accountId);
  }

  public async awaitSynchronised() {
    return this.aztecSdk.awaitSynchronised();
  }

  public async awaitSettlement(txId: TxId, timeout?: number) {
    return this.aztecSdk.awaitSettlement(txId, timeout);
  }

  public getLocalStatus() {
    return this.aztecSdk.getLocalStatus();
  }

  public async getRemoteStatus() {
    return this.aztecSdk.getRemoteStatus();
  }

  public async getTxFees(assetId: number) {
    return this.aztecSdk.getTxFees(assetId);
  }

  public getUserPendingDeposit(assetId: number, account: EthAddress) {
    return this.aztecSdk.getUserPendingDeposit(assetId, account);
  }

  private async getPublicKeyFromAddress(address: EthAddress) {
    const account = await this.db.getAccount(address);
    return account ? account.accountPublicKey : undefined;
  }

  public async isAliasAvailable(alias: string) {
    return this.aztecSdk.isAliasAvailable(alias);
  }

  public async mint(assetId: number, value: bigint, ethAddress: EthAddress) {
    return this.aztecSdk.mint(assetId, value, ethAddress);
  }

  public async createDepositController(from: EthAddress, to: AccountId, value: AssetValue, fee: AssetValue) {
    const userData = await this.aztecSdk.getUserData(to);
    const aztecSigner = await this.aztecSdk.createSchnorrSigner(userData.privateKey);
    return this.aztecSdk.createDepositController(to, aztecSigner, value, fee, from);
  }

  public async getDepositFees(assetId: number) {
    return this.aztecSdk.getDepositFees(assetId);
  }

  public async createWithdrawController(from: AccountId, to: EthAddress, value: AssetValue, fee: AssetValue) {
    const userData = await this.aztecSdk.getUserData(from);
    const aztecSigner = await this.aztecSdk.createSchnorrSigner(userData.privateKey);
    return this.aztecSdk.createWithdrawController(from, aztecSigner, value, fee, to);
  }

  public async getWithdrawFees(assetId: number, recipient?: EthAddress) {
    return this.aztecSdk.getWithdrawFees(assetId, recipient);
  }

  public async createTransferController(from: AccountId, to: AccountId, value: AssetValue, fee: AssetValue) {
    const userData = await this.aztecSdk.getUserData(from);
    const aztecSigner = await this.aztecSdk.createSchnorrSigner(userData.privateKey);
    return this.aztecSdk.createTransferController(from, aztecSigner, value, fee, to);
  }

  public async getTransferFees(assetId: number) {
    return this.aztecSdk.getTransferFees(assetId);
  }

  public async createRegisterController(
    accountId: AccountId,
    alias: string,
    signingPublicKey: GrumpkinAddress,
    recoveryPublicKey: GrumpkinAddress | undefined,
    assetId: number,
    fee: bigint,
    depositor: EthAddress,
  ) {
    const userData = await this.aztecSdk.getUserData(accountId);
    const aztecSigner = await this.aztecSdk.createSchnorrSigner(userData.privateKey);

    return this.aztecSdk.createRegisterController(
      accountId,
      aztecSigner,
      alias,
      signingPublicKey,
      recoveryPublicKey,
      { assetId: 0, value: BigInt(0) },
      {
        assetId,
        value: fee,
      },
      depositor,
    );
  }

  public async getRegisterFees(value: AssetValue) {
    return this.aztecSdk.getRegisterFees(value);
  }

  public getUserData(accountId: AccountId) {
    return this.aztecSdk.getUserData(accountId)!;
  }

  public async getAccountId(alias: string) {
    return this.aztecSdk.getAccountId(alias);
  }

  private async deriveGrumpkinPrivateKey(address: EthAddress) {
    return (await this.ethSigner.signMessage(Buffer.from('Link Aztec account.'), address)).slice(0, 32);
  }

  public async addUser(ethAddress: EthAddress, nonce?: number) {
    const privateKey = await this.deriveGrumpkinPrivateKey(ethAddress);
    let user: AztecSdkUser;
    try {
      user = await this.aztecSdk.addUser(privateKey, nonce);
    } catch (e) {
      const pubKey = await this.aztecSdk.derivePublicKey(privateKey);
      const accountNonce = nonce ?? (await this.aztecSdk.getLatestAccountNonce(pubKey));
      user = await this.aztecSdk.getUser(new AccountId(pubKey, accountNonce));
    }
    await this.db.setAccount({ ethAddress, accountPublicKey: (await user.getUserData()).publicKey });
    return new EthereumSdkUser(ethAddress, user.id, this);
  }

  public async removeUser(address: EthAddress, accountId: AccountId) {
    await this.db.deleteAccount(address);
    return this.aztecSdk.removeUser(accountId);
  }

  public async getUser(address: EthAddress) {
    const pubKey = await this.getPublicKeyFromAddress(address);
    if (!pubKey) {
      return;
    }
    const nonce = await this.aztecSdk.getLatestAccountNonce(pubKey);
    const accountId = new AccountId(pubKey, nonce);
    return new EthereumSdkUser(address, accountId, this);
  }

  public getBalance(assetId: number, accountId: AccountId) {
    return this.aztecSdk.getBalance(assetId, accountId);
  }

  public async getMaxSpendableValue(assetId: number, accountId: AccountId) {
    return this.aztecSdk.getMaxSpendableValue(assetId, accountId);
  }

  public async getPublicBalance(assetId: number, ethAddress: EthAddress) {
    return this.aztecSdk.getPublicBalance(assetId, ethAddress);
  }

  public fromBaseUnits(assetId: number, value: bigint, precision?: number) {
    return this.aztecSdk.fromBaseUnits({ assetId, value }, false, precision);
  }

  public toBaseUnits(assetId: number, value: string) {
    return this.aztecSdk.toBaseUnits(assetId, value);
  }

  public getAssetInfo(assetId: number) {
    return this.aztecSdk.getAssetInfo(assetId);
  }

  public async getPaymentTxs(accountId: AccountId) {
    return this.aztecSdk.getPaymentTxs(accountId);
  }

  public async getAccountTxs(accountId: AccountId) {
    return this.aztecSdk.getAccountTxs(accountId);
  }

  public async getDefiTxs(accountId: AccountId) {
    return this.aztecSdk.getDefiTxs(accountId);
  }

  public async getNotes(accountId: AccountId) {
    return this.aztecSdk.getNotes(accountId);
  }
}
