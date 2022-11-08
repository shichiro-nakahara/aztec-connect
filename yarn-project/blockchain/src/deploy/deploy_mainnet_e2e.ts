import { EthAddress } from '@aztec/barretenberg/address';
import { TreeInitData } from '@aztec/barretenberg/environment';
import { Signer } from 'ethers';
import {
  deployDefiBridgeProxy,
  deployElementBridge,
  deployFeeDistributor,
  deployLidoBridge,
  deployCurveBridge,
  deployRollupProcessor,
  deployVerifier,
  elementTokenAddresses,
  deployAztecFaucet,
} from './deployers/index.js';

const gasLimit = 5000000;
const escapeBlockLower = 2160;
const escapeBlockUpper = 2400;

const DAI_ADDRESS = '0x6b175474e89094c44da98b954eedeac495271d0f';
const LIDO_WSTETH_ADDRESS = '0x7f39C581F595B53c5cb19bD0b3f8dA6c935E2Ca0';
const UNISWAP_ROUTER_ADDRESS = '0x7a250d5630B4cF539739dF2C5dAcb4c659F2488D';
const DAI_PRICE_FEED_ADDRESS = '0x773616E4d11A78F511299002da57A0a94577F1f4';
const FAST_GAS_PRICE_FEED_ADDRESS = '0x169e633a2d1e6c10dd91238ba11c4a708dfef37c';

export async function deployMainnetE2e(
  signer: Signer,
  { dataTreeSize, roots }: TreeInitData,
  vk: string,
  faucetOperator?: EthAddress,
  rollupProvider?: EthAddress,
) {
  const verifier = await deployVerifier(signer, vk);
  const defiProxy = await deployDefiBridgeProxy(signer);
  const { rollup, permitHelper } = await deployRollupProcessor(
    signer,
    verifier,
    defiProxy,
    await signer.getAddress(),
    escapeBlockLower,
    escapeBlockUpper,
    roots.dataRoot,
    roots.nullRoot,
    roots.rootsRoot,
    dataTreeSize,
    true,
    true,
  );
  const feeDistributor = await deployFeeDistributor(signer, rollup, UNISWAP_ROUTER_ADDRESS);

  await rollup.setRollupProvider(rollupProvider ? rollupProvider.toString() : await signer.getAddress(), true, {
    gasLimit,
  });
  await rollup.grantRole(await rollup.LISTER_ROLE(), await signer.getAddress(), { gasLimit });

  await rollup.setSupportedAsset(DAI_ADDRESS, 0, { gasLimit });
  await permitHelper.preApprove(DAI_ADDRESS, { gasLimit });
  await rollup.setSupportedAsset(LIDO_WSTETH_ADDRESS, 0, { gasLimit });
  await permitHelper.preApprove(LIDO_WSTETH_ADDRESS, { gasLimit });
  await rollup.setSupportedAsset(elementTokenAddresses['lusd3crv-f'], 0, { gasLimit });
  await permitHelper.preApprove(elementTokenAddresses['lusd3crv-f'], { gasLimit });
  await rollup.setSupportedAsset(elementTokenAddresses['mim-3lp3crv-f'], 0, { gasLimit });
  await permitHelper.preApprove(elementTokenAddresses['mim-3lp3crv-f'], { gasLimit });

  const expiryCutOff = new Date('01 Sept 2022 00:00:00 GMT');
  await deployElementBridge(signer, rollup, ['dai'], expiryCutOff);
  await deployLidoBridge(signer, rollup);
  await deployCurveBridge(signer, rollup);

  const priceFeeds = [FAST_GAS_PRICE_FEED_ADDRESS, DAI_PRICE_FEED_ADDRESS];

  const faucet = await deployAztecFaucet(signer, faucetOperator);

  return { rollup, priceFeeds, feeDistributor, permitHelper, faucet };
}
