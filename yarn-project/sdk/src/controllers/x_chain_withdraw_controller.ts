import { EthAddress, GrumpkinAddress } from '@aztec/barretenberg/address';
import { AssetValue } from '@aztec/barretenberg/asset';
import { TxId } from '@aztec/barretenberg/tx_id';
import { CoreSdk } from '../core_sdk/index.js';
import { ProofOutput, proofOutputToProofTx } from '../proofs/index.js';
import { Signer } from '../signer/index.js';
import { createTxRefNo } from './create_tx_ref_no.js';
import { EthereumProvider } from '@aztec/barretenberg/blockchain';
import { ethers } from 'ethers';

const NG_GATEKEEPER = 'http://localhost:3999';
const NG_ADDRESS = '0x5F091Af1aBdF685eF91722f8912DF2423FFCBC1E';

export class XChainWithdrawController {
  private readonly requireFeePayingTx: boolean;
  private proofOutputs: ProofOutput[] = [];
  private feeProofOutputs: ProofOutput[] = [];
  private txIds: TxId[] = [];
  private withdrawId: number | undefined = undefined;
  private withdrawAmount: bigint | undefined = undefined;

  constructor(
    public readonly userId: GrumpkinAddress,
    private readonly userSigner: Signer,
    public readonly assetValue: AssetValue,
    public readonly fee: AssetValue,
    public readonly recipient: EthAddress,
    public readonly sgChainId: number,
    public readonly srcPoolId: number,
    public readonly dstPoolId: number,
    private readonly core: CoreSdk,
    private readonly provider: EthereumProvider
  ) {
    if (!assetValue.value) {
      throw new Error('Value must be greater than 0.');
    }

    this.requireFeePayingTx = !!fee.value && fee.assetId !== assetValue.assetId;
  }

  public async initWithdraw() {
    // const result = await(
    //   await fetch(
    //     `${NG_GATEKEEPER}/x-chain-withdraw`,
    //     {
    //       method: 'POST',
    //       headers: {
    //         'Content-Type': 'application/json'
    //       },
    //       body: JSON.stringify({
    //         sgChainId: this.sgChainId,
    //         srcPoolId: this.srcPoolId,
    //         dstPoolId: this.dstPoolId,
    //         assetId: this.assetValue.assetId,
    //         amount: this.assetValue.value.toString(),
    //         destination: this.recipient.toString()
    //       })
    //     }
    //   )
    // ).json();

    const result = {
      "id": 8,
      "txHash": "0x4adda0d80b9c60146fdd8f3aaf65641ac0e37157fdee7e5ab2588fedafc33ca0",
      "withdrawAmount": 5000000000000000008n
    };

    this.withdrawId = result.id;
    this.withdrawAmount = result.withdrawAmount;

    await new Promise((resolve, reject) => {
      const checkTxSettled = async () => {
        const receipt = await this.provider.request({ method: 'eth_getTransactionReceipt', params: [ result.txHash ] });
        if (!receipt) {
          setTimeout(checkTxSettled, 1000);
          return;
        }
        if (receipt.status != 1) {
          return reject('createWithdraw transaction reverted');
        }
        resolve(true);
      }
      checkTxSettled();
    });

    const signer = await (<any>this.provider).provider?.getSigner();
    if (!signer) throw new Error(`Unable to get JsonRpcSigner from EthereumProvider`);
    const nataGateway = new ethers.Contract(
      NG_ADDRESS,
      [
        `function withdraws(uint256) view returns (
          uint16 sgChainId, 
          uint256 srcPoolId, 
          uint256 dstPoolId, 
          uint256 assetId, 
          uint256 amount, 
          address destination, 
          bool complete
        )`
      ],
      signer
    );

    const withdraw = await nataGateway.withdraws(this.withdrawId);
    if (parseInt(withdraw[0]) != this.sgChainId || 
      parseInt(withdraw[1]) != this.srcPoolId || 
      parseInt(withdraw[2]) != this.dstPoolId || 
      parseInt(withdraw[3]) != this.assetValue.assetId || 
      withdraw[4] != this.assetValue.value ||
      withdraw[5] != this.recipient.toString()
    ) {
      // throw new Error('Invalid withdraw id');
    }

    return result;
  }

  public async createProof(timeout?: number) {
    if (!this.withdrawId || !this.withdrawAmount) {
      throw new Error('Call initWithdraw() first.');
    }

    const { assetId } = this.assetValue;
    const privateInput = this.withdrawAmount + (!this.requireFeePayingTx ? this.fee.value : BigInt(0));
    const spendingPublicKey = this.userSigner.getPublicKey();
    const spendingKeyRequired = !spendingPublicKey.equals(this.userId);

    const proofInputs = await this.core.createPaymentProofInputs(
      this.userId,
      assetId,
      BigInt(0),
      this.withdrawAmount,
      privateInput,
      BigInt(0),
      BigInt(0),
      this.userId,
      spendingKeyRequired,
      this.recipient,
      spendingPublicKey,
      2,
    );

    const txRefNo = this.requireFeePayingTx || proofInputs.length > 1 ? createTxRefNo() : 0;

    if (this.requireFeePayingTx) {
      const feeProofInputs = await this.core.createPaymentProofInputs(
        this.userId,
        this.fee.assetId,
        BigInt(0),
        BigInt(0),
        this.fee.value,
        BigInt(0),
        BigInt(0),
        this.userId,
        spendingKeyRequired,
        undefined,
        spendingPublicKey,
        2,
      );
      this.feeProofOutputs = [];
      for (const proofInput of feeProofInputs) {
        proofInput.signature = await this.userSigner.signMessage(proofInput.signingData);
        this.feeProofOutputs.push(await this.core.createPaymentProof(proofInput, txRefNo, timeout));
      }
    }

    {
      const proofOutputs: ProofOutput[] = [];
      for (const proofInput of proofInputs) {
        proofInput.signature = await this.userSigner.signMessage(proofInput.signingData);
        proofOutputs.push(await this.core.createPaymentProof(proofInput, txRefNo, timeout));
      }
      this.proofOutputs = proofOutputs;
    }
  }

  public exportProofTxs() {
    if (!this.proofOutputs.length) {
      throw new Error('Call createProof() first.');
    }

    return [...this.proofOutputs, ...this.feeProofOutputs].map(proofOutputToProofTx);
  }

  public async send() {
    if (!this.proofOutputs.length) {
      throw new Error('Call createProof() first.');
    }

    this.txIds = await this.core.sendProofs([...this.proofOutputs, ...this.feeProofOutputs]);
    return this.txIds[this.proofOutputs.length - 1];
  }

  public getTxIds() {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutputs.length ? 'createProof()' : 'send()'} first.`);
    }

    return this.txIds;
  }

  public async awaitSettlement(timeout?: number) {
    if (!this.txIds.length) {
      throw new Error(`Call ${!this.proofOutputs.length ? 'createProof()' : 'send()'} first.`);
    }

    await Promise.all(this.txIds.map(txId => this.core.awaitSettlement(txId, timeout)));
  }
}
