#!/usr/bin/env node
import { Web3Provider } from '@ethersproject/providers';
import { Contract, BigNumber } from 'ethers';
import { EthAddress } from '@aztec/barretenberg/address';
import { EthereumProvider, EthereumRpc, TxHash } from '@aztec/barretenberg/blockchain';
import { Command } from 'commander';
import { MainnetAddresses, purchaseTokens } from '@aztec/blockchain';
import { setBlockchainTime, getCurrentBlockTime } from '@aztec/blockchain';
import { decodeErrorFromContractByTxHash, decodeSelector, retrieveContractSelectors } from '@aztec/blockchain';
import { ElementBridgeData } from './clients/element-bridge-data.js';
import { WalletProvider } from '@aztec/blockchain';
import { getTokenBalance, getWethBalance } from '@aztec/blockchain';
import { LogDescription } from 'ethers/lib/utils.js';
import { RollupProcessor } from '@aztec/blockchain';
import { deriveDeploymentKeys } from './key_derivation.js';
import {
  RollupProcessor as RollupProcessorJson,
  AztecFaucetJson,
  ElementBridge as ElementBridgeJson,
  DataProviderJson,
} from './abis/index.js';
import { createAztecSdk, SdkFlavour, AliasHash, BarretenbergWasm, Blake2s, getRollupProviderStatus } from '@aztec/sdk';
const { PRIVATE_KEY } = process.env;

export const abis: { [key: string]: any } = {
  Rollup: RollupProcessorJson,
  Element: ElementBridgeJson,
  AztecFaucet: AztecFaucetJson,
  DataProvider: DataProviderJson,
};

const getProvider = (url = 'http://localhost:8545') => {
  const provider = WalletProvider.fromHost(url);
  if (PRIVATE_KEY) {
    const address = provider.addAccount(Buffer.from(PRIVATE_KEY, 'hex'));
    console.log(`Added account ${address.toString()} from provided private key`);
    return provider;
  }
  return provider;
};

export async function retrieveEvents(
  contractAddress: EthAddress,
  contractName: string,
  provider: EthereumProvider,
  eventName: string,
  from: number,
  to?: number,
) {
  console.log(`retrieving ${eventName} events from contract ${contractAddress} over blocks ${from} to ${to}`);
  const contract = new Contract(contractAddress.toString(), abis[contractName].abi, new Web3Provider(provider));
  const filter = contract.filters[eventName]();
  const events = await contract.queryFilter(filter, from, to);
  console.log(`received ${events.length} events`);
  return events.map(event => contract.interface.parseLog(event));
}

export async function decodeError(
  contractAddress: EthAddress,
  contractName: string,
  txHash: TxHash,
  provider: EthereumProvider,
) {
  const web3 = new Web3Provider(provider);
  const contract = new Contract(contractAddress.toString(), abis[contractName].abi, web3.getSigner());
  return await decodeErrorFromContractByTxHash(contract, txHash, provider);
}

export function decodeContractSelector(
  contractAddress: string,
  contractName: string,
  selector: string,
  provider: EthereumProvider,
) {
  const web3 = new Web3Provider(provider);
  const contract = new Contract(contractAddress, abis[contractName].abi, web3.getSigner());
  return decodeSelector(contract, selector);
}

export function getContractSelectors(
  contractAddress: EthAddress,
  contractName: string,
  provider: EthereumProvider,
  type?: string,
) {
  const web3 = new Web3Provider(provider);
  const contract = new Contract(contractAddress.toString(), abis[contractName].abi, web3.getSigner());
  return retrieveContractSelectors(contract, type);
}

export const createElementBridgeData = (
  rollupAddress: EthAddress,
  elementBridgeAddress: EthAddress,
  provider: EthereumProvider,
  rollupProviderUrl: string,
) => {
  return ElementBridgeData.create(
    provider,
    elementBridgeAddress as any,
    EthAddress.fromString(MainnetAddresses.Contracts['BALANCER']) as any,
    rollupAddress as any,
    rollupProviderUrl,
  );
};

const formatTime = (unixTimeInSeconds: number) => {
  return new Date(unixTimeInSeconds * 1000).toISOString().slice(0, 19).replace('T', ' ');
};

export async function profileElement(
  rollupAddress: EthAddress,
  elementAddress: EthAddress,
  provider: EthereumProvider,
  rollupProviderUrl: string,
  from: number,
  to?: number,
) {
  const convertEvents = await retrieveEvents(elementAddress, 'Element', provider, 'LogConvert', from, to);
  const finaliseEvents = await retrieveEvents(elementAddress, 'Element', provider, 'LogFinalise', from, to);
  const poolEvents = await retrieveEvents(elementAddress, 'Element', provider, 'LogPoolAdded', from, to);
  const rollupBridgeEvents = await retrieveEvents(rollupAddress, 'Rollup', provider, 'DefiBridgeProcessed', from, to);
  const elementBridgeData = createElementBridgeData(rollupAddress, elementAddress, provider, rollupProviderUrl);

  const interactions: {
    [key: string]: {
      finalised: boolean;
      success?: boolean;
      inputValue?: bigint;
      presentValue?: bigint;
      finalValue?: bigint;
      convertGas?: number;
      finaliseGas?: number;
      message?: string;
    };
  } = {};
  const pools: {
    [key: string]: { wrappedPosition: string; expiry: string };
  } = {};
  const convertLogs = convertEvents.map((log: LogDescription) => {
    return {
      nonce: log.args.nonce,
      totalInputValue: log.args.totalInputValue.toBigInt(),
      gasUsed: log.args.gasUsed,
    };
  });
  const finaliseLogs = finaliseEvents.map((log: LogDescription) => {
    return {
      nonce: log.args.nonce,
      success: log.args.success,
      gasUsed: log.args.gasUsed,
      message: log.args.message,
    };
  });
  const poolLogs = poolEvents.map((log: LogDescription) => {
    return {
      poolAddress: log.args.poolAddress,
      wrappedPosition: log.args.wrappedPositionAddress,
      expiry: log.args.expiry,
    };
  });
  const rollupLogs = rollupBridgeEvents.map((log: LogDescription) => {
    return {
      nonce: log.args.nonce.toBigInt(),
      outputValue: log.args.totalOutputValueA.toBigInt(),
    };
  });
  for (const log of poolLogs) {
    pools[log.poolAddress] = {
      wrappedPosition: log.wrappedPosition,
      expiry: formatTime(log.expiry.toNumber()),
    };
  }
  for (const log of convertLogs) {
    interactions[log.nonce.toString()] = {
      finalised: false,
      success: undefined,
      inputValue: log.totalInputValue,
      convertGas: Number(log.gasUsed),
    };
  }

  for (const log of finaliseLogs) {
    if (!interactions[log.nonce.toString()]) {
      interactions[log.nonce.toString()] = {
        finalised: true,
        success: log.success,
        inputValue: undefined,
        finaliseGas: Number(log.gasUsed),
        message: log.message,
      };
      continue;
    }
    const interaction = interactions[log.nonce.toString()];
    interaction.success = log.success;
    interaction.finalised = true;
    interaction.finaliseGas = Number(log.gasUsed);
    interaction.message = log.message;
    const rollupLog = rollupLogs.find(x => x.nonce == log.nonce);
    if (rollupLog) {
      interactions[log.nonce.toString()].finalValue = rollupLog.outputValue;
    }
  }
  interface BridgeAssetValue {
    assetId: number;
    value: bigint;
  }
  const promises: { [key: string]: Promise<BridgeAssetValue[]> } = {};
  for (const log of convertLogs) {
    if (!interactions[log.nonce.toString()]?.finalised) {
      const inputValue = interactions[log.nonce.toString()].inputValue!;
      promises[log.nonce.toString()] = elementBridgeData.getInteractionPresentValue(
        log.nonce.toBigInt(),
        inputValue,
      ) as unknown as Promise<BridgeAssetValue[]>;
    }
  }
  console.log(`calculating interaction present values...`);
  await Promise.all(Object.values(promises));
  const nonces = Object.keys(promises);
  for (const nonce of nonces) {
    const presentValue = await promises[nonce];
    if (presentValue.length > 0) {
      interactions[nonce].presentValue = presentValue[0].value;
    }
  }
  const summary = {
    Pools: pools,
    Interactions: interactions,
    NumInteractions: convertLogs.length,
    NumFinalised: finaliseLogs.length,
  };
  console.log('Element summary ', summary);
}

export async function findAliasCollisions(
  alias: string,
  provider: EthereumProvider,
  rollupProviderUrl: string,
  memoryDb = false,
) {
  const wasm = await BarretenbergWasm.new('main');
  const blake2s = new Blake2s(wasm);
  const sdk = await createAztecSdk(provider, {
    serverUrl: rollupProviderUrl,
    pollInterval: 10000,
    memoryDb: memoryDb, // if false will save chain data to file
    debug: 'bb:core_sdk',
    flavour: SdkFlavour.PLAIN,
    minConfirmation: 1,
  });
  await sdk.run();
  await sdk.awaitSynchronised();
  const isLetter = (char: string) => {
    return char.toLowerCase() != char.toUpperCase();
  };
  /// Generate alternative version of alias with different casing
  const inner = async (alias: string, index: number, collisions: { [key: string]: string }) => {
    if (index == alias.length) {
      const isRegistered = await sdk.isAliasRegistered(alias, false);
      if (isRegistered) {
        collisions[alias] = AliasHash.fromAlias(alias, blake2s).toString();
      }
      return;
    }
    await inner(alias, index + 1, collisions);

    // Numbers cannot be upper and lower case, so no need to check those.
    if (isLetter(alias.charAt(index))) {
      const temp = alias.substring(0, index) + alias.charAt(index).toUpperCase() + alias.substring(index + 1);
      await inner(temp, index + 1, collisions);
    }
  };
  const collisions: { [key: string]: string } = {};
  await inner(alias.toLowerCase(), 0, collisions);
  await sdk.destroy();
  console.log(`Registered aliases similar to "${alias}":`, collisions);
}

export interface DataProviderData {
  assets: {
    [assetName: string]: string;
  };
  bridges: { [bridgeName: string]: number };
  assetList: string[];
}

export interface RegistrationsDataRaw {
  [deployTag: string]: DataProviderData;
}

export async function fetchDataProviderData(url: string, deployTag: string) {
  // Fetch the data provider contract address
  const provider = getProvider(url);
  const rollupProviderUrl = `https://api.aztec.network/${deployTag}/falafel`;
  const status = await getRollupProviderStatus(rollupProviderUrl);
  const dataProviderAddress = status.blockchainStatus.bridgeDataProvider.toString();
  const dataProvider = new Contract(dataProviderAddress, abis['DataProvider'].abi, new Web3Provider(provider));

  const assetsRaw = await dataProvider.getAssets();
  const bridgesRaw = await dataProvider.getBridges();

  const assets: { [key: string]: string } = {};
  const assetList: string[] = [];
  assetsRaw.forEach((asset: { assetAddress: string; assetId: number; label: string }) => {
    assets[asset.label] = asset.assetAddress;
    assetList.push(asset.label);
  });
  const bridges = {};
  bridgesRaw.forEach((bridge: { bridgeAddress: string; bridgeAddressId: BigNumber; label: string }) => {
    if (bridge.label !== '') bridges[bridge.label] = bridge.bridgeAddressId.toNumber();
  });

  return {
    assetList,
    assets,
    bridges,
  };
}

export async function fetchAllDataProviderData() {
  const deployTags = ['aztec-connect-dev', 'aztec-connect-testnet', 'aztec-connect-stage', 'aztec-connect-prod'];
  const rpcUrls = [
    'https://aztec-connect-dev-eth-host.aztec.network:8545/e265e055c977fee83d415d3edeb26953',
    'https://aztec-connect-testnet-eth-host.aztec.network:8545/20ceb3a1db59c9b71315d98530093f94',
    'https://aztec-connect-stage-eth-host.aztec.network:8545/496405d10ea8bade3b4f91ee51399ab1',
    'https://aztec-connect-prod-eth-host.aztec.network:8545',
  ];
  const data: RegistrationsDataRaw = {};
  for (let i = 0; i < deployTags.length; i++) {
    data[deployTags[i]] = await fetchDataProviderData(rpcUrls[i], deployTags[i]);
  }
  console.log(JSON.stringify(data, null, 2));
}

const program = new Command();

async function main() {
  program.command('fetchAllDataProviderData').action(async () => {
    await fetchAllDataProviderData();
  });

  program
    .command('findAliasCollisions')
    .argument('<alias>', 'alias to check')
    .argument('[url]', 'your ethereum rpc url', 'http://localhost:8545')
    .argument('[rollupProviderUrl]', 'rollup provider url', 'https://api.aztec.network/aztec-connect-prod/falafel')
    .argument('[memoryDb]', 'use memory db', false)
    .action(async (alias: string, url: string, rollupProviderUrl: string, memoryDb: boolean) => {
      await findAliasCollisions(alias, getProvider(url), rollupProviderUrl, memoryDb);
    });

  program
    .command('deriveKeys')
    .description('derive keys')
    .argument('<mnemonic>', 'repository mnemonic')
    .action((mnemonic: string) => {
      console.log(deriveDeploymentKeys(mnemonic));
    });

  program
    .command('pkFromStore')
    .description('print a private key from an encrypted keystore file')
    .argument('<file>', 'path to the keystore file')
    .argument('[password]', 'password if keystore file is encrypted')
    .action((file: string, password?: string) => {
      const provider = getProvider();
      provider.addAccountFromKeystore(file, password);
      console.log(`${provider.getPrivateKey(0).toString('hex')}`);
    });

  program
    .command('pkFromMnemonic')
    .description('print a private key derived from a mnemonic')
    .argument('<mnemonic>', 'mnemonic')
    .argument('[derivation path]', 'derivation path', "m/44'/60'/0'/0/0'")
    .action((mnemonic: string, path: string) => {
      const provider = getProvider();
      provider.addAccountFromMnemonicAndPath(mnemonic, path);
      console.log(`${provider.getPrivateKey(0).toString('hex')}`);
    });

  program
    .command('setTime')
    .description('advance the blockchain time')
    .argument('<time>', 'the time you wish to set for the next block, unix timestamp format')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (time: any, url: any) => {
      const provider = getProvider(url);
      const date = new Date(parseInt(time));
      await setBlockchainTime(date.getTime(), provider);
      console.log(`New block time ${await getCurrentBlockTime(provider)}`);
    });

  program
    .command('decodeError')
    .description('attempt to decode the error for a reverted transaction')
    .argument('<contractAddress>', 'the address of the deployed contract, as a hex string')
    .argument('<contractName>', 'the name of the contract, valid values: Rollup, Element')
    .argument('<txHash>', 'the tx hash that you wish to decode, as a hex string')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (contractAddress, contractName, txHash, url) => {
      const provider = getProvider(url);
      const error = await decodeError(EthAddress.fromString(contractAddress), contractName, txHash, provider);
      if (!error) {
        console.log(`Failed to retrieve error for tx ${txHash}`);
        return;
      }
      console.log(`Retrieved error for tx ${txHash}`, error);
    });

  program
    .command('decodeSelector')
    .description('attempt to decode the selector for a reverted transaction')
    .argument('<contractAddress>', 'the address of the deployed contract, as a hex string')
    .argument('<contractName>', 'the name of the contract, valid values: Rollup, Element')
    .argument('<selector>', 'the 4 byte selector that you wish to decode, as a hex string 0x...')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action((contractAddress, contractName, selector, url) => {
      const provider = getProvider(url);
      if (selector.length == 10) {
        selector = selector.slice(2);
      }
      const error = decodeContractSelector(contractAddress, contractName, selector, provider);
      if (!error) {
        console.log(`Failed to retrieve error code for selector ${selector}`);
        return;
      }
      console.log(`Retrieved error code for selector ${selector}`, error);
    });

  program
    .command('finaliseDefi')
    .description('finalise an asynchronous defi interaction')
    .argument('<rollupAddress>', 'the address of the deployed rollup contract, as a hex string')
    .argument('<nonce>', 'the nonce you wish to finalise, as a number')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (rollupAddress, nonce, url) => {
      const provider = getProvider(url);
      const rollupProcessor = new RollupProcessor(rollupAddress, provider);
      try {
        await rollupProcessor.processAsyncDefiInteraction(parseInt(nonce), { gasLimit: 2000000 });
      } catch (err: any) {
        const result = decodeSelector(rollupProcessor.contract, err.data.result.slice(2));
        console.log('Failed to process async defi interaction, error: ', result);
      }
    });

  program
    .command('extractEvents')
    .description('extract events emitted from a contract')
    .argument('<contractAddress>', 'the address of the deployed contract, as a hex string')
    .argument('<contractName>', 'the name of the contract, valid values: Rollup, Element')
    .argument('<eventName>', 'the name of the emitted event')
    .argument('<from>', 'the block number to search from')
    .argument('[to]', 'the block number to search to, defaults to the latest block')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (contractAddress, contractName, eventName, from, to, url) => {
      const provider = getProvider(url);
      const logs = await retrieveEvents(
        EthAddress.fromString(contractAddress),
        contractName,
        provider,
        eventName,
        parseInt(from),
        to ? parseInt(to) : undefined,
      );
      console.log(
        'Received event args ',
        logs.map(l => l.args),
      );
    });

  program
    .command('purchaseTokens')
    .description('purchase tokens for an account')
    .argument('<token>', 'any token address or any key from the tokens listed in the mainnet command')
    .argument('<tokenQuantity>', 'the quantity of token you want to attempt to purchase')
    .argument(
      '[spender]',
      'the address of the account to purchase the token defaults to 1st default account 0xf39...',
      undefined,
    )
    .argument('[recipient]', 'the address of the account to receive the tokens defaults to the spender', undefined)
    .argument('[maxAmountToSpend]', 'optional limit of the amount to spend', BigInt(10n ** 21n).toString())
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (token, tokenQuantity, spender, recipient, maxAmountToSpend, url) => {
      const ourProvider = getProvider(url);
      const ethereumRpc = new EthereumRpc(ourProvider);
      const accounts = await ethereumRpc.getAccounts();
      spender = spender ? EthAddress.fromString(spender) : accounts[0];
      recipient = recipient ? EthAddress.fromString(recipient) : spender;

      let requestedToken = token;
      if (!EthAddress.isAddress(token)) {
        requestedToken = MainnetAddresses.Tokens[token];
        if (!requestedToken) {
          console.log(`Unknown token ${token}`);
          return;
        }
      }
      const amountPurchased = await purchaseTokens(
        EthAddress.fromString(requestedToken),
        BigInt(tokenQuantity),
        BigInt(maxAmountToSpend),
        ourProvider,
        spender,
        recipient,
      );
      if (amountPurchased === undefined) {
        console.log(`Failed to purchase ${token}`);
        return;
      }
      console.log(`Successfully purchased ${amountPurchased} of ${token}`);
    });

  program
    .command('getBalance')
    .description('display token/ETH balance for an account')
    .argument('<token>', 'any token address or any key from the tokens listed in the mainnet command')
    .argument(
      '[account]',
      'the address of the account to purchase the token defaults to 1st default account 0xf39...',
      undefined,
    )
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (token, account, url) => {
      const ourProvider = getProvider(url);
      const accounts = await new EthereumRpc(ourProvider).getAccounts();
      account = account ? EthAddress.fromString(account) : accounts[0];
      if (token == 'ETH') {
        const balance = await ourProvider.request({ method: 'eth_getBalance', params: [account.toString()] });
        console.log(`ETH balance of account ${account.toString()}: ${BigInt(balance)}`);
        return;
      }

      let requestedToken = token;
      if (!EthAddress.isAddress(token)) {
        requestedToken = MainnetAddresses.Tokens[token];
        if (!requestedToken) {
          console.log(`Unknown token ${token}`);
          return;
        }
      }

      const ethTokenAddress = EthAddress.fromString(requestedToken);
      const wethAddress = EthAddress.fromString(MainnetAddresses.Tokens['WETH']);
      if (ethTokenAddress.equals(wethAddress)) {
        const balance = await getWethBalance(account, ourProvider);
        console.log(`WETH balance of account ${account}: ${balance}`);
        return;
      }
      const balance = await getTokenBalance(ethTokenAddress, account, ourProvider);
      console.log(`Token ${ethTokenAddress.toString()} balance of account ${account}: ${balance}`);
    });

  program
    .command('selectors')
    .description('display useful information about a contract selectors')
    .argument('<contractAddress>', 'the address of the deployed contract, as a hex string')
    .argument('<contractName>', 'the name of the contract, valid values: Rollup, Element')
    .argument('[type]', 'optional filter for the type of selectors, e.g. error, event')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action((contractAddress, contractName, type, url) => {
      const ourProvider = getProvider(url);
      const selectorMap = getContractSelectors(EthAddress.fromString(contractAddress), contractName, ourProvider, type);
      console.log(selectorMap);
    });

  program
    .command('profileElement')
    .description('provides details of element defi interactions')
    .argument('<rollupAddress>', 'the address of the deployed rollup contract, as a hex string')
    .argument('<elementAddress>', 'the address of the deployed element bridge contract, as a hex string')
    .argument('<rollupProviderUrl>', "Falafel's endpoint in the required environemnt")
    .argument('<from>', 'the block number to search from')
    .argument('[to]', 'the block number to search to, defaults to the latest block')
    .argument('[url]', 'your ganache url', 'http://localhost:8545')
    .action(async (rollupAddress, elementAddress, rollupProviderUrl, from, to, url) => {
      const provider = getProvider(url);
      await profileElement(
        EthAddress.fromString(rollupAddress),
        EthAddress.fromString(elementAddress),
        provider,
        rollupProviderUrl,
        parseInt(from),
        to ? parseInt(to) : undefined,
      );
    });

  program
    .command('mainnet')
    .description('display useful addresses for mainnet')
    .action(() => {
      console.log(MainnetAddresses);
    });

  await program.parseAsync(process.argv);
}

main().catch(err => {
  console.log(`Error thrown: ${err}`);
  process.exit(1);
});
