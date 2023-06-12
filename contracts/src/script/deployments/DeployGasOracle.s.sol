// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import "forge-std/Test.sol";
import "../../core/libraries/GasOracle.sol";

contract DeployGasOracle is Test {
    function deploy() public {
        vm.broadcast();
        GasOracle gasOracle = new GasOracle(150); // 1.5x multiplier

        emit log_named_address("GAS_ORACLE ", address(gasOracle));
    }
}