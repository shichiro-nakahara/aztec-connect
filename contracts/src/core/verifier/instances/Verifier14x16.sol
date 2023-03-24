// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Aztec
pragma solidity >=0.8.4;

import {VerificationKey14x16} from "../keys/VerificationKey14x16.sol";
import {BaseStandardVerifier} from "../BaseStandardVerifier.sol";

contract Verifier14x16 is BaseStandardVerifier {
    function getVerificationKeyHash() public pure override(BaseStandardVerifier) returns (bytes32) {
        return VerificationKey14x16.verificationKeyHash();
    }

    function loadVerificationKey(uint256 vk, uint256 _omegaInverseLoc) internal pure virtual override {
        VerificationKey14x16.loadVerificationKey(vk, _omegaInverseLoc);
    }
}