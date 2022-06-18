import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { AssetValue } from '@aztec/barretenberg/asset';
import { EthereumProvider, TxHash } from '@aztec/barretenberg/blockchain';
import { retryUntil } from '@aztec/barretenberg/retry';
import { InterruptableSleep, sleep } from '@aztec/barretenberg/sleep';
import { Timer } from '@aztec/barretenberg/timer';
import { TxId } from '@aztec/barretenberg/tx_id';
import {
  ClientEthereumBlockchain,
  createPermitData,
  createPermitDataNonStandard,
  validateSignature,
  Web3Signer,
} from '@aztec/blockchain';
import { CoreSdkInterface } from '../core_sdk';
import { ProofOutput } from '../proofs';
import { createTxRefNo } from './create_tx_ref_no';
import { FeePayer } from './fee_payer';

export class DepositController {
  private readonly publicInput: AssetValue;
  private proofOutput?: ProofOutput;
  private feeProofOutput?: ProofOutput;
  private txIds: TxId[] = [];
  private pendingFundsStatus: {
    pendingDeposit: bigint;
    pendingFunds: bigint;
    requiredFunds: bigint;
    approveTxHash?: TxHash;
    approvedFromContractWallet?: boolean;
    txHash?: TxHash;
    depositedFromContractWallet?: boolean;
    approveProofTxHash?: TxHash;
    proofApprovedFromContractWallet?: boolean;
  } = { pendingDeposit: BigInt(0), pendingFunds: BigInt(0), requiredFunds: BigInt(0) };

  constructor(
    public readonly assetValue: AssetValue,
    public readonly fee: AssetValue,
    public readonly depositor: EthAddress,
    public readonly recipient: GrumpkinAddress,
    public readonly recipientAccountRequired: boolean,
    public readonly feePayer: FeePayer | undefined,
    private readonly core: CoreSdkInterface,
    private readonly blockchain: ClientEthereumBlockchain,
    private readonly provider: EthereumProvider,
  ) {
    const { assetId, value } = assetValue;
    if (!blockchain.getAsset(assetId)) {
      throw new Error('Unsupported asset');
    }
    if (!value && !fee.value) {
      throw new Error('Deposit value must be greater than 0.');
    }
    const requireFeePayingTx = fee.value && fee.assetId !== assetId;
    if (requireFeePayingTx && !feePayer) {
      throw new Error('Fee payer not provided.');
    }

    this.publicInput = { assetId, value: value + (fee.assetId === assetId ? fee.value : BigInt(0)) };
  }

  public async getPendingFunds() {
    const { pendingFunds } = await this.getPendingFundsStatus();
    return pendingFunds;
  }

  public async getRequiredFunds() {
    const { requiredFunds } = await this.getPendingFundsStatus();
    return requiredFunds;
  }

  public async getPublicAllowance() {
    const { assetId } = this.publicInput;
    const { rollupContractAddress } = await this.core.getLocalStatus();
    return this.blockchain.getAsset(assetId).allowance(this.depositor, rollupContractAddress);
  }

  public async hasPermitSupport() {
    const { assetId } = this.publicInput;
    return this.blockchain.hasPermitSupport(assetId);
  }

  public async approve() {
    const permitSupport = await this.hasPermitSupport();
    if (permitSupport) {
      throw new Error('Asset supports permit. No need to call approve().');
    }

    const pendingFundsStatus = await this.getPendingFundsStatus();
    const value = pendingFundsStatus.requiredFunds;
    if (!value) {
      throw new Error('User has deposited enough funds.');
    }

    const { assetId } = this.publicInput;
    const { rollupContractAddress } = await this.core.getLocalStatus();
    const approve = async () =>
      this.blockchain
        .getAsset(assetId)
        .approve(value, this.depositor, rollupContractAddress, { provider: this.provider });
    const isContract = await this.blockchain.isContract(this.depositor);
    let approveTxHash: TxHash | undefined;
    if (!isContract) {
      approveTxHash = await approve();
    } else {
      const checkOnchainData = async () => {
        const allowance = await this.getPublicAllowance();
        return allowance >= value;
      };
      approveTxHash = await this.sendTransactionAndCheckOnchainData(
        'approve allowance from contract wallet',
        approve,
        checkOnchainData,
      );
    }

    this.pendingFundsStatus = {
      ...pendingFundsStatus,
      approvedFromContractWallet: isContract && !approveTxHash,
      approveTxHash,
    };
    return approveTxHash;
  }

  public async awaitApprove(timeout?: number, interval?: number) {
    const { approvedFromContractWallet, approveTxHash, requiredFunds } = this.pendingFundsStatus;
    if (approvedFromContractWallet) {
      return true;
    }

    if (!approveTxHash) {
      throw new Error('Call approve() first.');
    }

    const checkOnchainData = async () => {
      const allowance = await this.getPublicAllowance();
      return allowance === requiredFunds;
    };
    await this.awaitTransaction('approve allowance', checkOnchainData, timeout, interval);
  }

  public async depositFundsToContract(permitDeadline?: bigint) {
    const pendingFundsStatus = await this.getPendingFundsStatus();
    const value = pendingFundsStatus.requiredFunds;
    if (!value) {
      throw new Error('User has deposited enough funds.');
    }
    if (this.pendingFundsStatus.approveTxHash && value < this.pendingFundsStatus.requiredFunds) {
      throw new Error(`Required allowance has increased since approve() was called.`);
    }

    const { assetId } = this.publicInput;
    const isContract = await this.blockchain.isContract(this.depositor);
    const depositFunds = async () => {
      const proofHash = isContract ? this.getProofHash() : undefined;
      const permitSupport = await this.hasPermitSupport();
      if (!permitSupport) {
        return this.blockchain.depositPendingFunds(assetId, value, proofHash, {
          signingAddress: this.depositor,
          provider: this.provider,
        });
      } else {
        const deadline = permitDeadline ?? BigInt(Math.floor(Date.now() / 1000) + 5 * 60); // Default deadline is 5 mins from now.
        const { signature } = await this.createPermitArgs(value, deadline);
        return this.blockchain.depositPendingFundsPermit(assetId, value, deadline, signature, proofHash, {
          signingAddress: this.depositor,
          provider: this.provider,
        });
      }
    };

    let txHash: TxHash | undefined;
    if (!isContract) {
      txHash = await depositFunds();
    } else {
      const { pendingDeposit, requiredFunds } = pendingFundsStatus;
      const expectedPendingDeposit = pendingDeposit + requiredFunds;
      const checkOnchainData = async () => {
        const value = await this.blockchain.getUserPendingDeposit(assetId, this.depositor);
        return value === expectedPendingDeposit;
      };
      txHash = await this.sendTransactionAndCheckOnchainData(
        'deposit funds from contract wallet',
        depositFunds,
        checkOnchainData,
      );
    }

    this.pendingFundsStatus = {
      ...pendingFundsStatus,
      depositedFromContractWallet: isContract && !txHash,
      txHash,
    };
    return txHash;
  }

  public async depositFundsToContractWithNonStandardPermit(permitDeadline?: bigint) {
    const pendingFundsStatus = await this.getPendingFundsStatus();
    const value = pendingFundsStatus.requiredFunds;
    if (!value) {
      throw new Error('User has deposited enough funds.');
    }

    const isContract = await this.blockchain.isContract(this.depositor);
    const proofHash = isContract ? this.getProofHash() : undefined;
    const deadline = permitDeadline ?? BigInt(Math.floor(Date.now() / 1000) + 5 * 60); // Default deadline is 5 mins from now.
    const { signature, nonce } = await this.createPermitArgsNonStandard(deadline);
    const { assetId } = this.publicInput;
    const depositFunds = async () =>
      this.blockchain.depositPendingFundsPermitNonStandard(assetId, value, nonce, deadline, signature, proofHash, {
        signingAddress: this.depositor,
        provider: this.provider,
      });
    let txHash: TxHash | undefined;
    if (!isContract) {
      txHash = await depositFunds();
    } else {
      const { pendingDeposit, requiredFunds } = pendingFundsStatus;
      const expectedPendingDeposit = pendingDeposit + requiredFunds;
      const checkOnchainData = async () => {
        const value = await this.blockchain.getUserPendingDeposit(assetId, this.depositor);
        return value === expectedPendingDeposit;
      };
      txHash = await this.sendTransactionAndCheckOnchainData(
        'deposit funds with non standard permit from contract wallet',
        depositFunds,
        checkOnchainData,
      );
    }

    this.pendingFundsStatus = {
      ...pendingFundsStatus,
      depositedFromContractWallet: isContract && !txHash,
      txHash,
    };
    return txHash;
  }

  public async awaitDepositFundsToContract(timeout?: number, interval?: number) {
    const { depositedFromContractWallet, txHash, pendingDeposit, requiredFunds } = this.pendingFundsStatus;
    if (depositedFromContractWallet) {
      return true;
    }

    if (!txHash) {
      throw new Error('Call depositFundsToContract() first.');
    }

    const { assetId } = this.publicInput;
    const expectedPendingDeposit = pendingDeposit + requiredFunds;
    const checkOnchainData = async () => {
      const value = await this.blockchain.getUserPendingDeposit(assetId, this.depositor);
      return value === expectedPendingDeposit;
    };
    await this.awaitTransaction('deposit pending funds', checkOnchainData, timeout, interval);
  }

  public async createProof(txRefNo = 0) {
    const { assetId, value } = this.publicInput;
    const requireFeePayingTx = !!this.fee.value && this.fee.assetId !== assetId;
    const privateOutput = requireFeePayingTx ? value : value - this.fee.value;
    if (requireFeePayingTx && !txRefNo) {
      txRefNo = createTxRefNo();
    }

    this.proofOutput = await this.core.createDepositProof(
      assetId,
      value, // publicInput,
      privateOutput,
      this.depositor,
      this.recipient,
      this.recipientAccountRequired,
      txRefNo,
    );

    if (requireFeePayingTx) {
      const { userId, signer } = this.feePayer!;
      const spendingPublicKey = signer.getPublicKey();
      const accountRequired = !spendingPublicKey.equals(userId);
      const feeProofInput = await this.core.createPaymentProofInput(
        userId,
        this.fee.assetId,
        BigInt(0),
        BigInt(0),
        this.fee.value,
        BigInt(0),
        BigInt(0),
        userId,
        accountRequired,
        undefined,
        spendingPublicKey,
        2,
      );
      feeProofInput.signature = await signer.signMessage(feeProofInput.signingData);
      this.feeProofOutput = await this.core.createPaymentProof(feeProofInput, txRefNo);
    }
  }

  public getProofHash() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }

    return this.proofOutput.tx.txId.toBuffer();
  }

  public async isProofApproved() {
    const proofHash = this.getProofHash();
    return !!(await this.blockchain.getUserProofApprovalStatus(this.depositor, proofHash));
  }

  public async approveProof() {
    const proofHash = this.getProofHash();
    const isContract = await this.blockchain.isContract(this.depositor);
    const approveProof = async () =>
      this.blockchain.approveProof(proofHash, {
        signingAddress: this.depositor,
        provider: this.provider,
      });
    let approveProofTxHash: TxHash | undefined;
    if (!isContract) {
      approveProofTxHash = await approveProof();
    } else {
      const checkOnchainData = async () => this.isProofApproved();
      approveProofTxHash = await this.sendTransactionAndCheckOnchainData(
        'approve proof from contract wallet',
        approveProof,
        checkOnchainData,
      );
    }

    this.pendingFundsStatus = {
      ...this.pendingFundsStatus,
      proofApprovedFromContractWallet: isContract && !approveProofTxHash,
      approveProofTxHash,
    };
    return approveProofTxHash;
  }

  public async awaitApproveProof(timeout?: number, interval?: number) {
    const { proofApprovedFromContractWallet, approveProofTxHash } = this.pendingFundsStatus;
    if (proofApprovedFromContractWallet) {
      return true;
    }

    if (!approveProofTxHash) {
      throw new Error('Call approveProof() first.');
    }

    const checkOnchainData = async () => this.isProofApproved();
    await this.awaitTransaction('approve proof', checkOnchainData, timeout, interval);
  }

  public getSigningData() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }
    return this.proofOutput.tx.txId.toDepositSigningData();
  }

  public async sign() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }

    const ethSigner = new Web3Signer(this.provider);
    const signingData = this.getSigningData();
    this.proofOutput.signature = await ethSigner.signMessage(signingData, this.depositor);
  }

  public isSignatureValid() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() and sign() first.');
    }
    if (!this.proofOutput.signature) {
      throw new Error('Call sign() first.');
    }

    const signingData = this.getSigningData();
    return validateSignature(this.depositor, this.proofOutput.signature, signingData);
  }

  public getProofs() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }

    return this.feeProofOutput ? [this.proofOutput, this.feeProofOutput] : [this.proofOutput];
  }

  public async send() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }
    if (!this.proofOutput.signature && !(await this.isProofApproved())) {
      throw new Error('Call sign() or approveProof() first.');
    }

    this.txIds = await this.core.sendProofs(this.getProofs());
    return this.txIds[0];
  }

  public async awaitSettlement(timeout?: number) {
    if (!this.txIds.length) {
      throw new Error('Call send() first.');
    }
    await Promise.all(this.txIds.map(txId => this.core.awaitSettlement(txId, timeout)));
  }

  private async getPendingFundsStatus() {
    const { assetId } = this.publicInput;
    const pendingDeposit = await this.blockchain.getUserPendingDeposit(assetId, this.depositor);
    const txs = await this.core.getPendingDepositTxs();
    const unsettledDeposit = txs
      .filter(tx => tx.assetId === assetId && tx.publicOwner.equals(this.depositor))
      .reduce((sum, tx) => sum + BigInt(tx.value), BigInt(0));
    const pendingFunds = pendingDeposit - unsettledDeposit;
    const { value } = this.publicInput;
    const requiredFunds = pendingFunds < value ? value - pendingFunds : BigInt(0);
    return { ...this.pendingFundsStatus, pendingDeposit, pendingFunds, requiredFunds };
  }

  private async createPermitArgs(value: bigint, deadline: bigint) {
    const { assetId } = this.publicInput;
    const asset = this.blockchain.getAsset(assetId);
    const nonce = await asset.getUserNonce(this.depositor);
    const { rollupContractAddress, chainId } = await this.core.getLocalStatus();
    const permitData = createPermitData(
      asset.getStaticInfo().name,
      this.depositor,
      rollupContractAddress,
      value,
      nonce,
      deadline,
      asset.getStaticInfo().address,
      this.getContractChainId(chainId),
    );
    const ethSigner = new Web3Signer(this.provider);
    const signature = await ethSigner.signTypedData(permitData, this.depositor);
    return { signature };
  }

  private async createPermitArgsNonStandard(deadline: bigint) {
    const { assetId } = this.publicInput;
    const asset = this.blockchain.getAsset(assetId);
    const nonce = await asset.getUserNonce(this.depositor);
    const { rollupContractAddress, chainId } = await this.core.getLocalStatus();
    const permitData = createPermitDataNonStandard(
      asset.getStaticInfo().name,
      this.depositor,
      rollupContractAddress,
      nonce,
      deadline,
      asset.getStaticInfo().address,
      this.getContractChainId(chainId),
    );
    const ethSigner = new Web3Signer(this.provider);
    const signature = await ethSigner.signTypedData(permitData, this.depositor);
    return { signature, nonce };
  }

  private getContractChainId(chainId: number) {
    // Any references to the chainId in the contracts on mainnet-fork (like the DOMAIN_SEPARATOR for permit data)
    // will have to be 1.
    switch (chainId) {
      case 0xa57ec:
      case 0xe2e:
        return 1;
      default:
        return chainId;
    }
  }

  private async sendTransactionAndCheckOnchainData(
    name: string,
    sendTx: () => Promise<TxHash>,
    checkOnchainData: () => Promise<boolean>,
    timeout?: number,
    interval = 1,
  ) {
    const interruptableSleep = new InterruptableSleep();
    let txHash: TxHash | undefined;
    let txError: Error | undefined;
    (async () => {
      try {
        txHash = await sendTx();
      } catch (e: any) {
        txError = e;
      }
      interruptableSleep.interrupt();
    })();

    const timer = new Timer();
    while (!txHash && !txError) {
      // We want confidence the tx will be accepted, so simulate waiting for confirmations.
      if (await checkOnchainData()) {
        const secondsTillConfirmed = (this.blockchain.minConfirmations - 1) * 15;
        await sleep(secondsTillConfirmed * 1000);
        break;
      }

      await interruptableSleep.sleep(interval * 1000);

      if (timeout && timer.s() > timeout) {
        throw new Error(`Timeout awaiting chain state condition: ${name}`);
      }
    }

    if (txError) {
      throw txError;
    }

    return txHash;
  }

  private async awaitTransaction(
    name: string,
    confirmedFromOnchainData: () => Promise<boolean>,
    timeout?: number,
    interval = 1,
  ) {
    await retryUntil(confirmedFromOnchainData, `chain state condition: ${name}`, timeout, interval);

    // We want confidence the tx will be accepted, so simulate waiting for confirmations.
    const secondsTillConfirmed = (this.blockchain.minConfirmations - 1) * 15;
    await sleep(secondsTillConfirmed * 1000);
  }
}
