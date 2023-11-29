// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {Test} from "forge-std/Test.sol";

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {ProxyAdmin} from "@openzeppelin/contracts/proxy/transparent/ProxyAdmin.sol";
import {TransparentUpgradeableProxy} from "@openzeppelin/contracts/proxy/transparent/TransparentUpgradeableProxy.sol";

import {RollupProcessorLatest} from "core/processors/RollupProcessorLatest.sol";

contract UpgradeLatestProd is Test {
    error StorageAltered(uint256 index, bytes32 expected, bytes32 actual);

    bytes32 internal constant MOCK_KEY_HASH = 0xe93606306cfda92d3e8937e91d4467ecb74c7092eb49e932be66a2f488ca7003;
    bytes32 internal constant PROD_KEY_HASH = 0x8c16a95cccbb8c49aaf2bf27970df31180f348dfd3bbda93acb7fa800840ce5d;

    function deploy(address _proxy, uint256 _oldVersion, uint256 _newVersion) public {
        RollupProcessorLatest proxy = RollupProcessorLatest(payable(_proxy));
        ProxyAdmin proxyAdmin = _getProxyAdmin(_proxy);

        require(proxy.getImplementationVersion() == _oldVersion, "Version before don't match");
        require(proxy.paused(), "Rollup is not paused");

        uint256 lowerBound = proxy.escapeBlockLowerBound();
        uint256 upperBound = proxy.escapeBlockUpperBound();

        vm.broadcast();
        RollupProcessorLatest fix = new RollupProcessorLatest(lowerBound, upperBound);

        vm.expectRevert("Initializable: contract is already initialized");
        fix.initialize();

        require(fix.getImplementationVersion() == _newVersion, "Fix Version not matching");

        emit log_named_address(
            "Old rollup ",
            proxyAdmin.getProxyImplementation(TransparentUpgradeableProxy(payable(_proxy)))
        );
        emit log_named_address("New rollup ", address(fix));
        emit log_named_address("Proxy admin", address(proxyAdmin));
    }

    function verify(address _proxy, address _newRollup, address _oldRollup, uint256 _newVersion) public {
        RollupProcessorLatest proxy = RollupProcessorLatest(payable(_proxy));
        ProxyAdmin proxyAdmin = _getProxyAdmin(_proxy);

        address implementation = proxyAdmin.getProxyImplementation(TransparentUpgradeableProxy(payable(_proxy)));

        require(implementation == _newRollup, "Proxy implementation does not match new rollup address");
        require(proxy.getImplementationVersion() == _newVersion, "Version after don't match");

        // Load storage values from old implementation. Skip initialising (_initialized, _initializing)
        bytes32[] memory values = new bytes32[](25);
        for (uint256 i = 1; i < 25; i++) {
            values[i] = vm.load(_oldRollup, bytes32(i));
        }

        // check that existing storage is unaltered or altered as planned
        for (uint256 i = 1; i < 17; i++) {
            bytes32 readSlot = vm.load(_newRollup, bytes32(i));
            if (values[i] != readSlot) {
                revert StorageAltered(i, values[i], readSlot);
            }
        }

        emit log("Upgrade successful");

        emit log_named_address("Proxy                      ", _proxy);
        emit log_named_uint("Implementation version     ", proxy.getImplementationVersion());
        emit log_named_bytes32("Rollup state hash          ", proxy.rollupStateHash());
        emit log_named_address("Proxy admin address        ", address(proxyAdmin));
        emit log_named_address("Owner of proxy admin       ", proxyAdmin.owner());
        emit log_named_address(
            "Implementation address     ",
            proxyAdmin.getProxyImplementation(TransparentUpgradeableProxy(payable(_proxy)))
        );
    }

    function _getProxyAdmin(address _proxy) internal view returns (ProxyAdmin) {
        address admin = address(
            uint160(
                uint256(vm.load(address(_proxy), 0xb53127684a568b3173ae13b9f8a6016e243e63b6e8ee1178d6a717850b5d6103))
            )
        );
        return ProxyAdmin(admin);
    }
}
