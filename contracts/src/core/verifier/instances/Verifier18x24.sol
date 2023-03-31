// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Aztec
pragma solidity >=0.8.4;

import {VerificationKey18x24} from "../keys/VerificationKey18x24.sol";
import {BaseStandardVerifier} from "../BaseStandardVerifier.sol";

contract Verifier18x24 is BaseStandardVerifier {
    function getVerificationKeyHash() public pure override(BaseStandardVerifier) returns (bytes32) {
        return VerificationKey18x24.verificationKeyHash();
    }

    function loadVerificationKey(uint256 vk, uint256 _omegaInverseLoc) internal pure virtual override {
        VerificationKey18x24.loadVerificationKey(vk, _omegaInverseLoc);
    }
}