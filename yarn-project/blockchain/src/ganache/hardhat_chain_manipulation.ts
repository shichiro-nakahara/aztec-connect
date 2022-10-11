import { EthAddress } from '@aztec/barretenberg/address';
import { BigNumber, BigNumberish } from 'ethers';
import hre from 'hardhat';
import { JsonRpcProvider } from '@ethersproject/providers';
import { blocksToAdvance, getCurrentBlockNumber } from './manipulate_blocks.js';
import { EthersAdapter } from '../provider/index.js';

export const evmSnapshot = async () => await hre.network.provider.send('evm_snapshot', []);

export const evmRevert = async (id: string) => await hre.network.provider.send('evm_revert', [id]);

export const setEthBalance = async (address: EthAddress, balance: bigint) => {
  // Hardhat don't like leading zeros in hexes, e.g., 0x01 so we use a regex to remove them.
  let cleanedString = `0x${BigNumber.from(balance).toHexString().substring(2).replace(/^0+/, '')}`;
  if (balance == 0n) {
    cleanedString = '0x0';
  }
  await hre.network.provider.send('hardhat_setBalance', [address.toString(), cleanedString]);
};

export const setAutoMine = async (autoMine: boolean) => {
  await hre.network.provider.send('evm_setAutomine', [autoMine]);
  if (autoMine) {
    await hre.network.provider.send('evm_mine', []);
  }
};

export const multipleTxOneBlock = async (transactions: () => Promise<void>): Promise<void> => {
  await setAutoMine(false);
  await transactions();
  await setAutoMine(true);
};

export const setNextTime = async (timestamp: BigNumberish) =>
  await hre.network.provider.send('evm_setNextBlockTimestamp', [BigNumber.from(timestamp).toNumber()]);

export const setTime = async (timestamp: BigNumberish) => {
  await hre.network.provider.send('evm_setNextBlockTimestamp', [BigNumber.from(timestamp).toNumber()]);
  await hre.network.provider.send('evm_mine', []);
};

export async function advanceBlocksHardhat(blocks: number, provider: JsonRpcProvider) {
  // Hardhat don't like leading zeros in hexes, e.g., 0x01 so we use a regex to remove them.
  let diffClean = `0x${BigNumber.from(blocks).toHexString().substring(2).replace(/^0+/, '')}`;
  if (blocks == 0) {
    diffClean = '0x0';
  }
  await hre.network.provider.send('hardhat_mine', [diffClean]);
  return await getCurrentBlockNumber(new EthersAdapter(provider));
}

export async function blocksToAdvanceHardhat(target: number, accuracy: number, provider: JsonRpcProvider) {
  return await blocksToAdvance(target, accuracy, new EthersAdapter(provider));
}
