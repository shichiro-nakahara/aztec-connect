import { HardhatUserConfig } from 'hardhat/config';
import '@nomiclabs/hardhat-waffle';
import '@nomiclabs/hardhat-ethers';
import '@nomiclabs/hardhat-etherscan';

const config: HardhatUserConfig = {
  solidity: {
    version: '0.6.10',
    settings: {
      evmVersion: 'berlin',
      optimizer: { enabled: true, runs: 200 },
    },
  },
  networks: {
    ganache: {
      url: `http://${process.env.GANACHE_HOST || 'localhost'}:8545`,
    },
    // mainnet: {
    //   url: process.env.ETHEREUM_HOST,
    // },
    hardhat: {
      blockGasLimit: 15000000,
      gasPrice: 10,
      hardfork: 'berlin',
    },
  },
  paths: {
    artifacts: './src/artifacts',
  },
  etherscan: {
    apiKey: process.env.ETHERSCAN_API_KEY,
  },
};

export default config;
