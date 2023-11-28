import { EthAddress } from '@aztec/barretenberg/address';
import { EthereumProvider } from '@aztec/barretenberg/blockchain';
import { Web3Provider } from '@ethersproject/providers';
import { Contract, BigNumber } from 'ethers';

export class StargateComposer {
  public stargateComposer: Contract;
  private provider: Web3Provider;

  constructor(
    protected address: EthAddress,
    ethereumProvider: EthereumProvider,
  ) {
    this.provider = new Web3Provider(ethereumProvider);
    this.stargateComposer = new Contract(
      address.toString(), 
      [ 
        'function quoteLayerZeroFee(' +
          'uint16 _chainId, ' +
          'uint8 _functionType, ' +
          'bytes _toAddress, ' +
          'bytes _transferAndCallPayload, ' +
          'tuple(uint256 dstGasForCall, uint256 dstNativeAmount, bytes dstNativeAddr) _lzTxParams' +
        ') view returns (uint256, uint256)'
      ], 
      this.provider
    );
  }

  async quoteLayerZeroFee(sgChainId: number, to: EthAddress) {
    const result = await this.stargateComposer.quoteLayerZeroFee(
      sgChainId,
      1,
      to.toString(),
      '0x',
      {
        dstGasForCall: 0,
        dstNativeAmount: 0,
        dstNativeAddr: '0x'
      }
    );
    return result.map((r: BigNumber) => r.toBigInt());
  }
}