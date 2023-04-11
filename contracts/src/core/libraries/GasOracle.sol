// SPDX-License-Identifier: Apache-2.0
pragma solidity >=0.8.4;

import {Ownable} from "@openzeppelin/contracts/access/Ownable.sol";

contract GasOracle is Ownable {
    // is IChainlinkOracle

    int256 public priorityFee;

    constructor(int256 _priorityFee) {
        priorityFee = _priorityFee;
    }

    function latestAnswer() external view returns (int256) {
        return int(block.basefee) + priorityFee;
    }

    function getAnswer(uint256) external view returns (int256) {
        return this.latestAnswer();
    }

    function latestRound() external view returns (uint80, int256, uint256, uint256, uint80) {
        return (
            uint80(1), // roundId
            int(block.basefee) + priorityFee, // answer
            block.timestamp - 1, // startedAt
            block.timestamp, // updatedAt
            uint80(1) // answeredInRound
        );
    }

    function getRoundData(uint256) external view returns (uint80, int256, uint256, uint256, uint80) {
        return this.latestRound();
    }

    function setPriorityFee(int256 _priorityFee) external onlyOwner {
        priorityFee = _priorityFee;
    }
}