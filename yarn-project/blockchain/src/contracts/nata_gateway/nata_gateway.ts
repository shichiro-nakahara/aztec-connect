import { EthAddress } from '@polyaztec/barretenberg/address';
import { EthereumProvider } from '@polyaztec/barretenberg/blockchain';
import { Web3Provider } from '@ethersproject/providers';
import { Contract } from 'ethers';

export class NataGateway {
  public nataGateway: Contract;
  private provider: Web3Provider;

  constructor(
    protected address: EthAddress,
    ethereumProvider: EthereumProvider,
  ) {
    this.provider = new Web3Provider(ethereumProvider);
    this.nataGateway = new Contract(
      address.toString(), 
      [ 
        `function sgWithdraws(uint256) view returns (
          uint16 sgChainId, 
          uint256 srcPoolId, 
          uint256 dstPoolId, 
          uint256 assetId, 
          uint256 rpWithdrawAmount, 
          address destination, 
          bool complete, 
          string txHash, 
          uint256 sgSendAmount
        )`,
        `function acrossWithdraws(uint256) view returns (
          uint256 destinationChainId, 
          address originToken, 
          uint256 assetId, 
          uint256 rpWithdrawAmount, 
          address recipient, 
          bool complete, 
          string txHash, 
          uint256 acrossSendAmount
        )`,

      ], 
      this.provider
    );
  }

  async getSGWithdraw(id: number) {
    try {
      const withdraw = await this.nataGateway.sgWithdraws(id);
      if (withdraw.sgChainId == 0) return undefined;
      return withdraw;
    }
    catch (e: any) {
      console.warn(e);
      return undefined;
    }
  }

  async getAcrossWithdraw(id: number) {
    try {
      const withdraw = await this.nataGateway.acrossWithdraws(id);
      if (withdraw.destinationChainId == 0) return undefined;
      return withdraw;
    }
    catch (e: any) {
      console.warn(e);
      return undefined;
    }
  }
}