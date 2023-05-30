// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {Test} from "forge-std/Test.sol";
import {PermitHelper} from "periphery/PermitHelper.sol";
import {AggregateDeployment} from "bridge-deployments/AggregateDeployment.s.sol";
import {ERC20Permit} from "../../test/mocks/ERC20Permit.sol";
import {AztecFeeDistributor} from "periphery/AztecFeeDistributor.sol";
import {GasOracle} from "core/libraries/GasOracle.sol";
import {RollupProcessorV3} from "core/processors/RollupProcessorV3.sol";

// Mocks
import {DummyDefiBridge} from "../../test/mocks/DummyDefiBridge.sol";
import {SyncBridge} from "../../test/mocks/SyncBridge.sol";
import {AsyncBridge} from "../../test/mocks/AsyncBridge.sol";
import {IDefiBridge} from "core/interfaces/IDefiBridge.sol";
import {AztecFaucet} from "periphery/AztecFaucet.sol";
import {MockChainlinkOracle} from "../../test/mocks/MockChainlinkOracle.sol";
import {MockBridgeDataProvider} from "../../test/mocks/MockBridgeDataProvider.sol";

contract ChainSpecificSetupV3 is Test {
    // Mainnet fork key addresses
    address internal constant DAI = 0x6B175474E89094C44Da98b954EedeAC495271d0F;
    address internal constant UNISWAP_V2_ROUTER = 0x7a250d5630B4cF539739dF2C5dAcb4c659F2488D;

    // Polygon key addresses
    address internal constant POLYGON_DAI = 0xF14f9596430931E177469715c591513308244e8F; // Mumbai

    // Mainnet addresses for criticial components
    address internal constant MAINNET_GAS_PRICE_FEED = 0x169E633A2D1E6c10dD91238Ba11c4A708dfEF37C;
    address internal constant MAINNET_DAI_PRICE_FEED = 0x773616E4d11A78F511299002da57A0a94577F1f4;

    // Polygon addresses for criticial components
    address internal constant POLYGON_DAI_PRICE_FEED = 0xFC539A559e170f848323e19dfD66007520510085; // DAI:ETH price on Polygon POS (Mainnet)

    /// @notice Addresses that are returned when setting up a testnet
    struct BridgePeripheryAddresses {
        address dataProvider;
        address gasPriceFeed;
        address daiPriceFeed;
        address ethPriceFeed;
        address dai;
        address eth;
        address faucet;
        address feeDistributor;
    }

    /**
     * @notice Deploys bridges for E2E or full setup based on chain-id
     * @param _proxy The address of the rollup proxy
     * @param _permitHelper The address of the permit helper
     * @param _faucetOperator The address of the faucet operator
     * @param _safe The address of the Multisig Safe
     * @return BridgePeripheryAddress contains dataProvider, priceFeeds, faucet and fee distributor addresses
     */
    function setupAssetsAndBridges(address _proxy, address _permitHelper, address _faucetOperator, address _safe)
        public
        returns (BridgePeripheryAddresses memory)
    {
        uint256 chainId = block.chainid;

        //  polygon mainnet   mumbai              zkEVM mainnet      zkEVM testnet
        if (chainId == 137 || chainId == 80001 || chainId == 1101 || chainId == 1442) {
            return setupAssetAndBridgesPolygon(_proxy, _permitHelper, _safe, _faucetOperator);
        }
        else {
            return setupAssetsAndBridgesTestsV3(_proxy, _permitHelper, _faucetOperator);
        }
    }

    /**
     * @notice Deploys bridges for full setup with the aggregate deployment from bridges repo
     * @param _proxy The address of the rollup proxy
     * @param _permitHelper The address of the permit helper
     * @param _safe The address of the Multisig Safe
     * @param _faucetOperator The address of the faucet operator
     * @return BridgePeripheryAddresses contains dataProvider, priceFeeds, faucet and fee distributor addresses
     */
    function setupAssetAndBridgesPolygon(address _proxy, address _permitHelper, address _safe, address _faucetOperator)
        public
        returns (BridgePeripheryAddresses memory)
    {
        emit log_string("Setting up assets and bridges for Polygon");

        // Deploy faucet
        address faucet = deployFaucet(_faucetOperator);

        // Use custom gas oracle with fixed priority fee
        vm.broadcast();
        GasOracle gasOracle = new GasOracle(5 gwei);

        return BridgePeripheryAddresses({
            dataProvider: address(0),
            gasPriceFeed: address(gasOracle),
            daiPriceFeed: address(0),
            ethPriceFeed: address(0),
            dai: address(0),
            eth: address(0),
            faucet: faucet,
            feeDistributor: address(0)
        });
    }

    /**
     * @notice Deploys bridges for E2E tests
     * @param _proxy The address of the rollup proxy
     * @param _permitHelper The address of the permit helper
     * @param _faucetOperator The address of the faucet operator
     * @return BridgePeripheryAddresses contains dataProvider, priceFeeds, faucet and fee distributor addresses
     */
    function setupAssetsAndBridgesTestsV3(address _proxy, address _permitHelper, address _faucetOperator)
        public
        returns (BridgePeripheryAddresses memory)
    {
        // Deploy two mock erc20s
        ERC20Permit dai = deployERC20(_proxy, _permitHelper, "DAI", 18);
        ERC20Permit eth = deployERC20(_proxy, _permitHelper, "WETH", 18);

        // Deploy Price Feeds
        vm.broadcast();
        MockChainlinkOracle gasPriceFeed = new MockChainlinkOracle(5 gwei);

        vm.broadcast();
        MockChainlinkOracle daiPriceFeed = new MockChainlinkOracle(1.25 ether); // 1.25 MATIC = 1 DAI

        vm.broadcast();
        MockChainlinkOracle ethPriceFeed = new MockChainlinkOracle(2000 ether); // 2000 MATIC = 1 WETH

        // Deploy faucet
        address faucet = deployFaucet(_faucetOperator);

        // return all of the addresses that have just been deployed
        return BridgePeripheryAddresses({
            dataProvider: address(0),
            gasPriceFeed: address(gasPriceFeed),
            daiPriceFeed: address(daiPriceFeed),
            ethPriceFeed: address(ethPriceFeed),
            dai: address(dai),
            eth: address(eth),
            faucet: faucet,
            feeDistributor: address(0) // Not required in end to end tests
        });
    }

    /**
     * @notice Deploy a new faucet and set the faucet operator
     * @param _faucetOperator The address of the new operator - nb: we dont need to worry about it being 0
     * @return address of new Faucet
     */
    function deployFaucet(address _faucetOperator) internal returns (address) {
        vm.broadcast();
        AztecFaucet faucet = new AztecFaucet();

        vm.broadcast();
        faucet.updateSuperOperator(_faucetOperator, true);

        return address(faucet);
    }

    /**
     * @notice Deploy a mock ERC20 for use in the e2e test, adds the ERC20 to the rollup then
     *         pre approve
     * @param _proxy Rollup address
     * @param _permitHelper Permit Helper address
     * @param _symbol Mock token symbol
     * @param _decimals Token decimals
     * @return mockToken ERC20Permit
     *
     */
    function deployERC20(address _proxy, address _permitHelper, string memory _symbol, uint8 _decimals)
        internal
        returns (ERC20Permit mockToken)
    {
        uint256 dummyTokenMockGasLimit = 55000;

        vm.broadcast();
        mockToken = new ERC20Permit(_symbol);

        if (_decimals != 18) {
            vm.broadcast();
            mockToken.setDecimals(_decimals);
        }
        vm.broadcast();
        RollupProcessorV3(payable(_proxy)).setSupportedAsset(address(mockToken), dummyTokenMockGasLimit);
        vm.broadcast();
        PermitHelper(_permitHelper).preApprove(address(mockToken));
    }
}
