// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {Test} from "forge-std/Test.sol";

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {ProxyAdmin} from "@openzeppelin/contracts/proxy/transparent/ProxyAdmin.sol";
import {TransparentUpgradeableProxy} from "@openzeppelin/contracts/proxy/transparent/TransparentUpgradeableProxy.sol";

import {PermitHelper} from "periphery/PermitHelper.sol";
import {IRollupProcessor} from "rollup-encoder/interfaces/IRollupProcessor.sol";
import {RollupProcessorV4} from "core/processors/RollupProcessorV4.sol";

import {MockVerifier} from "core/verifier/instances/MockVerifier.sol";
import {Verifier28x32} from "core/verifier/instances/Verifier28x32.sol";
import {IVerifier} from "core/interfaces/IVerifier.sol";

contract UpgradeV4 is Test {
    error StorageAltered(uint256 index, bytes32 expected, bytes32 actual);

    uint256 internal constant VERSION_BEFORE = 3;
    uint256 internal constant VERSION_AFTER = 4;

    bytes32 internal constant MOCK_KEY_HASH = 0xe93606306cfda92d3e8937e91d4467ecb74c7092eb49e932be66a2f488ca7003;
    bytes32 internal constant PROD_KEY_HASH = 0x8c16a95cccbb8c49aaf2bf27970df31180f348dfd3bbda93acb7fa800840ce5d;

    RollupProcessorV4 internal ROLLUP;
    ProxyAdmin internal PROXY_ADMIN;

    function upgrade(address _rollup, bool _mockVerifier) public {
        ROLLUP = RollupProcessorV4(payable(_rollup));
        PROXY_ADMIN = _getProxyAdmin();

        if (_mockVerifier) {
            emit log("Upgrading rollup processor to V4 with mock verifier");
        }
        else {
            emit log("Upgrading rollup processor to V4 with 28x32 verifier");
        }

        // Load storage values from implementation. Skip initialising (_initialized, _initializing)
        bytes32[] memory values = new bytes32[](25);
        for (uint256 i = 1; i < 25; i++) {
            values[i] = vm.load(address(ROLLUP), bytes32(i));
        }
        address bridgeProxy = ROLLUP.defiBridgeProxy();
        uint256 gasLimit = ROLLUP.bridgeGasLimits(1);
        bytes32 prevDefiInteractionsHash = ROLLUP.prevDefiInteractionsHash();
        // Simulate a deposit of ether setting the proof approval
        ROLLUP.depositPendingFunds{value: 1 ether}(0, 1 ether, address(this), bytes32("dead"));

        // Deploy V4
        address implementationV4 = _prepareRollup();

        // Upgrade to new implementation
        bytes memory upgradeCalldata = abi.encodeWithSignature(
            "upgradeAndCall(address,address,bytes)",
            TransparentUpgradeableProxy(payable(address(ROLLUP))),
            implementationV4,
            abi.encodeWithSignature("initialize()")
        );
        vm.broadcast();
        (bool success,) = address(PROXY_ADMIN).call(upgradeCalldata);
        require(success, "Upgrade call failed");

        // Deploy new verifier and check hash
        bytes32 verifierKeyHash = _mockVerifier ? MOCK_KEY_HASH : PROD_KEY_HASH;

        address verifierAddress;
        if (_mockVerifier) {
            // Deploy mock verifier
            vm.broadcast();
            MockVerifier mockVerifier = new MockVerifier();

            verifierAddress = address(mockVerifier);
        }
        else {
            // Deploy 28x32 verifier
            vm.broadcast();
            Verifier28x32 prodVerifier = new Verifier28x32();

            verifierAddress = address(prodVerifier);
        }

        vm.broadcast();
        ROLLUP.setVerifier(verifierAddress);

        // Update roles
        bytes32 lister = ROLLUP.LISTER_ROLE();
        bytes32 resume = ROLLUP.RESUME_ROLE();
        vm.startBroadcast();
        ROLLUP.grantRole(lister, tx.origin);
        ROLLUP.grantRole(resume, tx.origin);
        vm.stopBroadcast();

        // Checks
        require(IVerifier(ROLLUP.verifier()).getVerificationKeyHash() == verifierKeyHash, "Invalid key hash");
        require(ROLLUP.getImplementationVersion() == VERSION_AFTER, "Version after don't match");
        require(
            PROXY_ADMIN.getProxyImplementation(TransparentUpgradeableProxy(payable(address(ROLLUP))))
                == implementationV4,
            "Implementation address not matching"
        );
        require(ROLLUP.hasRole(lister, tx.origin), "Not lister");
        require(ROLLUP.hasRole(resume, tx.origin), "Not resume");

        // check that existing storage is unaltered or altered as planned
        for (uint256 i = 1; i < 17; i++) {
            if (i == 2) {
                // The rollup state must have changed to set the `capped` flag and new verifier
                bytes32 expected = bytes32(uint256(values[i]) | uint256(1 << 240));
                expected = bytes32((uint256(expected >> 160) << 160) | uint160(verifierAddress));

                bytes32 readSlot = vm.load(address(ROLLUP), bytes32(i));
                if (expected != readSlot) {
                    revert StorageAltered(i, expected, readSlot);
                }
            } else {
                bytes32 readSlot = vm.load(address(ROLLUP), bytes32(i));
                if (values[i] != readSlot) {
                    revert StorageAltered(i, values[i], readSlot);
                }
            }
        }
        require(ROLLUP.depositProofApprovals(address(this), bytes32("dead")), "Approval altered");
        require(ROLLUP.userPendingDeposits(0, address(this)) == 1 ether, "Pending amount altered");
        require(ROLLUP.defiBridgeProxy() == bridgeProxy, "Invalid bridgeProxy");
        require(ROLLUP.bridgeGasLimits(1) == gasLimit, "Invalid bridge gas limit");
        require(ROLLUP.prevDefiInteractionsHash() == prevDefiInteractionsHash, "Invalid prevDefiInteractionsHash");

        emit log("Upgrade to V4 successful");

        read();
    }

    function read() public {
        emit log_named_address("ROLLUP                     ", address(ROLLUP));
        emit log_named_uint("Implementation version     ", ROLLUP.getImplementationVersion());
        emit log_named_bytes32("Rollup state hash          ", ROLLUP.rollupStateHash());
        emit log_named_uint("Number of bridges          ", ROLLUP.getSupportedBridgesLength());
        emit log_named_address("Owner of proxy admin       ", PROXY_ADMIN.owner());
        emit log_named_address(
            "Implementation address     ",
            PROXY_ADMIN.getProxyImplementation(TransparentUpgradeableProxy(payable(address(ROLLUP))))
            );
    }

    function _prepareRollup() internal returns (address) {
        require(ROLLUP.getImplementationVersion() == VERSION_BEFORE, "Version before don't match");

        uint256 lowerBound = ROLLUP.escapeBlockLowerBound();
        uint256 upperBound = ROLLUP.escapeBlockUpperBound();

        vm.broadcast();
        RollupProcessorV4 fix = new RollupProcessorV4(lowerBound, upperBound);

        vm.expectRevert("Initializable: contract is already initialized");
        fix.initialize();

        require(fix.getImplementationVersion() == VERSION_AFTER, "Fix Version not matching");

        return address(fix);
    }

    function _getProxyAdmin() internal view returns (ProxyAdmin) {
        address admin = address(
            uint160(
                uint256(vm.load(address(ROLLUP), 0xb53127684a568b3173ae13b9f8a6016e243e63b6e8ee1178d6a717850b5d6103))
            )
        );
        return ProxyAdmin(admin);
    }
}
