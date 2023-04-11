// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {IPool} from "@aave/core-v3/contracts/interfaces/IPool.sol";
import {IWETH} from "@aave/core-v3/contracts/misc/interfaces/IWETH.sol";
import {DataTypes} from "@aave/core-v3/contracts/protocol/libraries/types/DataTypes.sol";

library AaveV3 {
    address internal constant POLYGON_POOL_PROXY = 0x0b913A76beFF3887d35073b8e5530755D60F78C7; // Mumbai
    address internal constant POLYGON_W_NATIVE = 0xf237dE5664D3c2D2545684E76fef02A3A58A364c; // Mumbai

    IPool internal constant lendingPool = IPool(POLYGON_POOL_PROXY);

    function depositToLP(address _asset, uint256 _amount) internal {
        IERC20(_asset).approve(address(lendingPool), _amount);
        lendingPool.supply(_asset, _amount, address(this), 0);
    }

    function depositNativeToLP(uint256 _amount) internal {
        IWETH(POLYGON_W_NATIVE).deposit{value: _amount}();
        IERC20(POLYGON_W_NATIVE).approve(address(lendingPool), _amount);
        lendingPool.supply(POLYGON_W_NATIVE, _amount, address(this), 0);
    }

    function withdrawFromLP(address _asset, uint256 _amount) internal {
        lendingPool.withdraw(_asset, _amount, address(this));
    }

    // Ensure receive() is implemented in the contract or this function will throw an exception
    function withdrawNativeFromLP(uint256 _amount) internal {
        lendingPool.withdraw(POLYGON_W_NATIVE, _amount, address(this));
        IWETH(POLYGON_W_NATIVE).withdraw(_amount);
    }

    function withdrawable(address _asset) internal view returns (uint256) {
        DataTypes.ReserveData memory reserveData = lendingPool.getReserveData(_asset);
        
        require (reserveData.aTokenAddress != address(0));

        return IERC20(reserveData.aTokenAddress).balanceOf(address(this));
    }
}