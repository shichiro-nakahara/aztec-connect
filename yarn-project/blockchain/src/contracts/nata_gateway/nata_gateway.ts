import { EthAddress } from '@aztec/barretenberg/address';
import { EthereumProvider } from '@aztec/barretenberg/blockchain';
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
      this.provider
    );
  }

  async getWithdraw(id: number) {
    try {
      const withdraw = await this.nataGateway.withdraws(id);
      if (withdraw.sgChainId == 0) return undefined;
      return withdraw;
    }
    catch (e: any) {
      console.warn(e);
      return undefined;
    }
  }
}