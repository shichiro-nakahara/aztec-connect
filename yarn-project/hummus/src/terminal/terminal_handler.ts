import {
  AssetValue,
  AztecSdk,
  AztecSdkUser,
  BridgeCallData,
  createAztecSdk,
  CreateSdkOptions,
  DefiSettlementTime,
  EthAddress,
  EthereumProvider,
  EthereumRpc,
  MemoryFifo,
  ProofId,
  SdkEvent,
  TxType,
  UserPaymentTx,
  WalletProvider,
  Timer,
} from '@aztec/sdk';
import createDebug from 'debug';
import { Terminal } from './terminal.js';

declare global {
  interface Window {
    aztecSdk: any;
  }
}

const debug = createDebug('bb:terminal_handler');

enum TermControl {
  PROMPT,
  LOCK,
}

/**
 * The terminal handler is composed of two queues. A command queue and a print queue.
 * Commands are executed in sequence, and prints are printed in sequence.
 * The print queue also accepts terminal control codes for enabling and disabling the prompt.
 * When the terminal emits a command, it will have already locked the terminal (disabled the prompt)
 * and it is the job of the handler to reenable the prompt once the command is handled.
 */
export class TerminalHandler {
  private controlQueue = new MemoryFifo<() => Promise<void>>();
  private printQueue = new MemoryFifo<string | TermControl>();
  private preInitCmds = { help: this.help, init: this.init };
  private postInitCmds = {
    help: this.help,
    deposit: this.deposit,
    defi: this.defiDeposit,
    withdraw: this.withdraw,
    transfer: this.transfer,
    register: this.registerAlias,
    balance: this.balance,
    fees: this.fees,
    status: this.status,
    user: this.userInfo,
  };
  private assetId = 0;
  private sdk!: AztecSdk;
  private ethAddress!: EthAddress;
  private user!: AztecSdkUser;
  private accountPrivateKey!: Buffer;

  constructor(
    private terminal: Terminal,
    public provider: EthereumProvider,
    private options: CreateSdkOptions = {
      serverUrl: 'http://localhost:8081',
    },
  ) {}

  public start() {
    void this.controlQueue.process(fn => fn());
    void this.processPrint();
    this.printQueue.put('\x01\x01\x01\x01aztec zero knowledge terminal.\x01\n');

    this.printQueue.put("type command or 'help'\n");
    this.printQueue.put(TermControl.PROMPT);
    this.terminal.on('cmd', this.queueCommand);
  }

  private async handleCommand(cmd: string, args: string[], cmds: any) {
    if (!cmds[cmd]) {
      return;
    }
    await cmds[cmd].call(this, ...args);
  }

  /**
   * Called when a command is entered in the terminal.
   * The terminal is locked when this is called.
   * Run the command, and then restore the prompt.
   */
  private queueCommand = (cmdStr: string) => {
    this.controlQueue.put(async () => {
      try {
        const [cmd, ...args] = cmdStr.toLowerCase().split(/ +/g);
        if (!this.sdk) {
          await this.handleCommand(cmd, args, this.preInitCmds);
        } else {
          await this.handleCommand(cmd, args, this.postInitCmds);
        }
      } catch (err: any) {
        debug(err);
        this.printQueue.put(err.message + '\n');
      }
      this.printQueue.put(TermControl.PROMPT);
    });
  };

  /**
   * Called after the app has been initialized.
   */
  private registerHandlers() {
    this.sdk.on(SdkEvent.DESTROYED, this.handleSdkDestroyed);
  }

  private unregisterHandlers() {
    if (this.sdk) {
      this.sdk.off(SdkEvent.DESTROYED, this.handleSdkDestroyed);
    }
  }

  private handleSdkDestroyed = () => {
    this.controlQueue.put(() => {
      this.printQueue.put(TermControl.LOCK);
      this.printQueue.put('\rlogged out. reinitialize.\n');
      this.printQueue.put(TermControl.PROMPT);
      this.unregisterHandlers();
      return Promise.resolve();
    });
  };

  public stop() {
    this.terminal.stop();
    this.controlQueue.cancel();
    this.printQueue.cancel();
    this.unregisterHandlers();
  }

  private async processPrint() {
    const isTermControl = (tbd: any): tbd is TermControl => !isNaN(tbd);

    while (true) {
      const item = await this.printQueue.get();
      if (item === null) {
        break;
      }
      if (isTermControl(item)) {
        switch (item) {
          case TermControl.PROMPT:
            await this.terminal.prompt();
            break;
          case TermControl.LOCK:
            this.terminal.lock();
            break;
        }
      } else {
        if (this.terminal.isPrompting()) {
          await this.terminal.putString('\r' + item);
          this.printQueue.put(TermControl.PROMPT);
        } else {
          await this.terminal.putString(item);
        }
      }
    }
  }

  private help() {
    if (!this.sdk) {
      this.printQueue.put('init [server]\n');
    } else {
      this.printQueue.put(
        'deposit <amount>\n' +
          'defi <amount> <bridge call data> <input asset>\n' +
          '     <output asset> [aux data]\n' +
          'withdraw <amount>\n' +
          'transfer <to> <amount>\n' +
          'register <alias> [amount]\n' +
          'balance\n' +
          'fees\n' +
          'status [num] [from]\n' +
          'user\n',
      );
    }
  }

  private async init(serverUrl: string) {
    this.unregisterHandlers();

    if (this.provider instanceof WalletProvider) {
      // We're probably running in a puppeteer test. Enable terminal logging.
      createDebug.enable('aztec:hummus:terminal');
      this.printQueue.put(`using injected wallet provider.\n`);
    }

    if ((this.provider as any).enable) {
      this.printQueue.put(`requesting account access...\n`);
      await (this.provider as any).enable();
    }

    this.sdk = await createAztecSdk(this.provider, serverUrl ? { ...this.options, serverUrl } : this.options);
    try {
      window.aztecSdk = this.sdk;
    } catch (_) {
      // Nom.
    }

    const ethereumRpc = new EthereumRpc(this.provider);
    [this.ethAddress] = await ethereumRpc.getAccounts();
    this.printQueue.put(`check provider to create account key...\n`);
    const { publicKey, privateKey } = await this.sdk.generateAccountKeyPair(this.ethAddress, this.provider);
    this.accountPrivateKey = privateKey;

    try {
      const {
        blockchainStatus: { dataSize, dataRoot, nullRoot },
      } = await this.sdk.getRemoteStatus();
      this.printQueue.put(`data size: ${dataSize}\n`);
      this.printQueue.put(`data root: ${dataRoot.slice(0, 8).toString('hex')}...\n`);
      this.printQueue.put(`null root: ${nullRoot.slice(0, 8).toString('hex')}...\n`);
    } catch (err) {
      this.printQueue.put('failed to get server status.\n');
    }

    const pubKey = await this.sdk.derivePublicKey(privateKey);
    if (!(await this.sdk.userExists(pubKey))) {
      this.user = await this.sdk.addUser(privateKey);
    } else {
      this.user = await this.sdk.getUser(pubKey);
    }

    await this.sdk.run();

    const timer = new Timer();
    this.printQueue.put(`syncing user: ${publicKey.toString().slice(0, 12)}...\n`);
    await this.sdk.awaitUserSynchronised(this.user.id);
    this.printQueue.put(`sync complete in ${timer.s()}s\n`);
    await this.balance();

    this.registerHandlers();
  }

  private async deposit(valueStr: string) {
    await this.assertRegistered();
    const value = this.sdk.toBaseUnits(this.assetId, valueStr);
    const [, fee] = await this.sdk.getDepositFees(this.assetId);
    const publicInput = value.value + fee.value;
    const controller = this.sdk.createDepositController(this.ethAddress, value, fee, this.user.id, true);
    const assetBalance = await this.sdk.getPublicBalance(this.ethAddress, this.assetId);
    const pendingBalance = await controller.getPendingFunds();
    if (assetBalance.value + pendingBalance < publicInput) {
      throw new Error('insufficient balance.');
    }
    if (publicInput > pendingBalance) {
      this.printQueue.put(`depositing funds to contract...\n`);
      await controller.depositFundsToContract();
      this.printQueue.put(`awaiting transaction confirmation...\n`);
      await controller.awaitDepositFundsToContract();
    }
    this.printQueue.put(`generating proof...\n`);
    await controller.createProof();
    this.printQueue.put(`signing proof...\n`);
    await controller.sign();
    await controller.send();
    this.printQueue.put(`deposit proof sent.\n`);
  }

  private async defiDeposit(
    valueStr: string,
    addressIdStr: string,
    inputAssetStr: string,
    outputAssetStr: string,
    auxData = BigInt(0),
  ) {
    await this.assertRegistered();
    const inputAsset = +inputAssetStr;
    const assetValue = this.sdk.toBaseUnits(inputAsset, valueStr);
    const bridgeCallData = new BridgeCallData(
      +addressIdStr,
      inputAsset,
      +outputAssetStr,
      undefined,
      undefined,
      auxData,
    );
    const fee = (
      await this.sdk.getDefiFees(bridgeCallData, { userId: this.user.id, userSpendingKeyRequired: true, assetValue })
    )[DefiSettlementTime.INSTANT];
    const spendingKey = await this.sdk.generateSpendingKeyPair(this.ethAddress);
    const userSigner = await this.sdk.createSchnorrSigner(spendingKey.privateKey);
    const controller = this.sdk.createDefiController(this.user.id, userSigner, bridgeCallData, assetValue, fee);
    this.printQueue.put(`generating proof...\n`);
    await controller.createProof();
    await controller.send();
    this.printQueue.put(`defi deposit proof sent.\n`);
  }

  private async withdraw(valueStr: string) {
    await this.assertRegistered();
    const value = this.sdk.toBaseUnits(this.assetId, valueStr);
    const [, fee] = await this.sdk.getWithdrawFees(this.assetId, { recipient: this.ethAddress });
    const spendingKey = await this.sdk.generateSpendingKeyPair(this.ethAddress);
    const userSigner = await this.sdk.createSchnorrSigner(spendingKey.privateKey);
    const controller = this.sdk.createWithdrawController(this.user.id, userSigner, value, fee, this.ethAddress);
    await controller.createProof();
    await controller.send();
    this.printQueue.put(`withdraw proof sent.\n`);
  }

  private async transfer(alias: string, valueStr: string) {
    await this.assertRegistered();
    const to = await this.sdk.getAccountPublicKey(alias);
    if (!to) {
      throw new Error(`unknown user: ${alias}`);
    }
    const value = this.sdk.toBaseUnits(this.assetId, valueStr);
    const [, fee] = await this.sdk.getTransferFees(this.assetId);
    const spendingKey = await this.sdk.generateSpendingKeyPair(this.ethAddress);
    const userSigner = await this.sdk.createSchnorrSigner(spendingKey.privateKey);
    const controller = this.sdk.createTransferController(this.user.id, userSigner, value, fee, to, true);
    await controller.createProof();
    await controller.send();
    this.printQueue.put(`transfer proof sent.\n`);
  }

  private async assertRegistered() {
    if (!(await this.isRegistered())) {
      throw new Error('register an alias first.');
    }
  }

  private async isRegistered() {
    return await this.sdk.isAccountRegistered(this.user.id);
  }

  private async registerAlias(alias: string, valueStr = '0') {
    if (await this.isRegistered()) {
      throw new Error('account already has an alias.');
    }
    if (await this.sdk.isAliasRegistered(alias)) {
      throw new Error('alias already registered.');
    }
    const deposit = this.sdk.toBaseUnits(0, valueStr);
    const [, fee] = await this.sdk.getRegisterFees(deposit.assetId);
    const spendingKey = await this.sdk.generateSpendingKeyPair(this.ethAddress);
    const controller = this.sdk.createRegisterController(
      this.user.id,
      alias,
      this.accountPrivateKey,
      spendingKey.publicKey,
      undefined,
      deposit,
      fee,
      this.ethAddress,
    );
    const requiredFunds = await controller.getRequiredFunds();
    if (requiredFunds > 0n) {
      this.printQueue.put(`depositing funds to contract...\n`);
      await controller.depositFundsToContract();
      this.printQueue.put(`awaiting transaction confirmation...\n`);
      await controller.awaitDepositFundsToContract();
    }
    this.printQueue.put(`generating proof...\n`);
    await controller.createProof();
    this.printQueue.put(`signing proof...\n`);
    await controller.sign();
    await controller.send();
    this.printQueue.put(`registration proof sent.\nawaiting settlement...\n`);
    await controller.awaitSettlement();
    this.printQueue.put(`done.\n`);
  }

  private async balance(assetIdStr = '') {
    const assetId = assetIdStr ? +assetIdStr : this.assetId;
    this.printQueue.put(
      `public: ${this.sdk.fromBaseUnits(await this.sdk.getPublicBalance(this.ethAddress, assetId), true, 6)}\n`,
    );
    this.printQueue.put(
      `private: ${this.sdk.fromBaseUnits(await this.sdk.getBalance(this.user.id, assetId), true, 6)}\n`,
    );
    const fundsPendingDeposit = await this.sdk.getUserPendingDeposit(assetId, this.ethAddress);
    if (fundsPendingDeposit > 0) {
      this.printQueue.put(
        `pending deposit: ${this.sdk.fromBaseUnits({
          assetId,
          value: fundsPendingDeposit,
        })}\n`,
      );
    }
  }

  private async fees() {
    const { symbol } = this.sdk.getAssetInfo(this.assetId);
    const txTypes = [
      TxType.ACCOUNT,
      TxType.DEPOSIT,
      TxType.TRANSFER,
      TxType.WITHDRAW_TO_WALLET,
      TxType.WITHDRAW_HIGH_GAS,
    ];
    const txFees = await this.sdk.getTxFees(this.assetId);
    txTypes.forEach(txType => {
      this.printQueue.put(`${TxType[txType]}: ${this.sdk.fromBaseUnits(txFees[txType][0])} ${symbol}\n`);
    });
  }

  private async status(num = `1`, from = '0') {
    const txs = await this.user.getPaymentTxs();
    const f = Math.max(0, +from);
    const n = Math.min(Math.max(+num, 0), 5);
    const printTx = (tx: UserPaymentTx, action: string, value: AssetValue) => {
      const asset = this.sdk.getAssetInfo(tx.value.assetId);
      this.printQueue.put(
        `${tx.txId.toString().slice(2, 10)}: ${action} ${this.sdk.fromBaseUnits(value)} ${asset.symbol} ${
          tx.settled ? 'settled' : 'pending'
        }\n`,
      );
    };
    for (const tx of txs.slice(f, f + n)) {
      if (tx.proofId === ProofId.SEND && !tx.isSender) {
        printTx(tx, 'RECEIVE', tx.value);
        return;
      }
      printTx(tx, ProofId[tx.proofId], tx.value);
    }
  }

  private async userInfo() {
    this.printQueue.put(`user: ${this.user.id.toString().slice(0, 12)}...\n`);
    this.printQueue.put(`synchronised: ${!(await this.user.isSynching())}\n`);
  }
}
