import { Contract, ContractFactory, Signer } from 'ethers';
import { ERC20Mintable, ERC20Permit } from '../../abis.js';

const gasLimit = 5000000;

export async function deployErc20(
  rollup: Contract,
  permitHelper: Contract,
  signer: Signer,
  supportsPermit: boolean,
  symbol = 'TEST',
  decimals = 18,
) {
  if (supportsPermit) {
    console.error('Deploying ERC20 with permit support...');
    const erc20Factory = new ContractFactory(ERC20Permit.abi, ERC20Permit.bytecode, signer);
    const erc20 = await erc20Factory.deploy(symbol);
    console.error(`ERC20 contract address: ${erc20.address}`);
    if (decimals !== 18) {
      console.error(`Changing decimals to: ${decimals}...`);
      await erc20.setDecimals(decimals);
    }
    await rollup.setSupportedAsset(erc20.address, 0, { gasLimit });
    await permitHelper.preApprove(erc20.address, { gasLimit });
    return erc20;
  } else {
    console.error('Deploying ERC20...');
    const erc20Factory = new ContractFactory(ERC20Mintable.abi, ERC20Mintable.bytecode, signer);
    const erc20 = await erc20Factory.deploy(symbol);
    console.error(`ERC20 contract address: ${erc20.address}`);
    if (decimals !== 18) {
      console.error(`Changing decimals to: ${decimals}...`);
      await erc20.setDecimals(decimals, { gasLimit });
    }
    await rollup.setSupportedAsset(erc20.address, 0, { gasLimit });
    return erc20;
  }
}
