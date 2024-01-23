import { EthAddress } from '@polyaztec/barretenberg/address';
import { AssetValue } from '@polyaztec/barretenberg/asset';
import { EthereumProvider } from '@polyaztec/barretenberg/blockchain';
import { TxId } from '@polyaztec/barretenberg/tx_id';
import { ClientEthereumBlockchain } from '@polyaztec/blockchain';
import { CoreSdk } from '../core_sdk/index.js';
import { ProofOutput, proofOutputToProofTx } from '../proofs/index.js';
import { RecoveryPayload } from '../user/index.js';
import { createTxRefNo } from './create_tx_ref_no.js';
import { DepositHandler } from './deposit_handler.js';

export class RecoverAccountController extends DepositHandler {
  private proofOutput!: ProofOutput;
  private txIds: TxId[] = [];
  private requireDeposit: boolean;

  constructor(
    public readonly recoveryPayload: RecoveryPayload,
    public readonly deposit: AssetValue,
    public readonly fee: AssetValue,
    public readonly depositor = EthAddress.ZERO,
    protected readonly core: CoreSdk,
    blockchain: ClientEthereumBlockchain,
    provider: EthereumProvider,
  ) {
    const {
      recoveryData: { accountPublicKey },
    } = recoveryPayload;
    super(deposit, fee, depositor, accountPublicKey, true, core, blockchain, provider);
    this.requireDeposit = !!this.publicInput.value;
  }

  public async createProof(timeout?: number) {
    const txRefNo = this.requireDeposit ? createTxRefNo() : 0;

    if (this.requireDeposit) {
      await super.createProof(txRefNo, timeout);
    }

    const {
      trustedThirdPartyPublicKey,
      recoveryPublicKey,
      recoveryData: { accountPublicKey, signature },
    } = this.recoveryPayload;
    const proofInput = await this.core.createAccountProofInput(
      accountPublicKey,
      recoveryPublicKey,
      false,
      undefined,
      trustedThirdPartyPublicKey,
      undefined,
      undefined,
    );
    proofInput.signature = signature;
    this.proofOutput = await this.core.createAccountProof(proofInput, txRefNo, timeout);
  }

  public exportProofTxs() {
    return this.getProofOutputs().map(proofOutputToProofTx);
  }

  public async send() {
    const proofs = this.getProofOutputs();
    this.txIds = await this.core.sendProofs(proofs);
    return this.txIds[0];
  }

  public getTxIds() {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutput ? 'createProof()' : 'send()'} first.`);
    }

    return this.txIds;
  }

  public async awaitSettlement(timeout?: number) {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutput ? 'createProof()' : 'send()'} first.`);
    }

    await Promise.all(this.txIds.map(txId => this.core.awaitSettlement(txId, timeout)));
  }

  private getProofOutputs() {
    if (!this.proofOutput) {
      throw new Error('Call createProof() first.');
    }

    const proofOutputs = [this.proofOutput];
    if (this.requireDeposit) {
      proofOutputs.push(super.getProofOutput());
    }
    return proofOutputs;
  }
}
