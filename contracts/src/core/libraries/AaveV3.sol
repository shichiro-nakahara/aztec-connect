// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {IPool} from "@aave/core-v3/contracts/interfaces/IPool.sol";
import {IWETH} from "@aave/core-v3/contracts/misc/interfaces/IWETH.sol";

library AaveV3 {
    address internal constant POLYGON_POOL_PROXY = 0x0b913A76beFF3887d35073b8e5530755D60F78C7; // Mumbai
    address internal constant POLYGON_WMATIC = 0xf237dE5664D3c2D2545684E76fef02A3A58A364c; // Mumbai

    IPool internal constant lendingPool = IPool(POLYGON_POOL_PROXY);

    function depositToLP(address _asset, uint256 _amount) public {
        IERC20(_asset).approve(address(lendingPool), _amount);
        lendingPool.supply(_asset, _amount, address(this), 0);
    }

    function depositNativeToLP(uint256 _amount) public {
        IWETH(POLYGON_WMATIC).deposit{value: _amount}();
        IERC20(POLYGON_WMATIC).approve(address(lendingPool), _amount);
        lendingPool.supply(POLYGON_WMATIC, _amount, address(this), 0);
    }

    function withdrawFromLP(address _asset, uint256 _amount) public {
        lendingPool.withdraw(_asset, _amount, address(this));
    }

    // Ensure receive() is implemented in the contract or this function will throw an exception
    function withdrawNativeFromLP(uint256 _amount) public {
        lendingPool.withdraw(POLYGON_WMATIC, _amount, address(this));
        IWETH(POLYGON_WMATIC).withdraw(_amount);
    }
}