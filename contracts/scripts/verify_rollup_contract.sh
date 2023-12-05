#!/bin/bash

echo "";

read -p "Please enter implementation address: " IMPLEMENTATION_ADDRESS

echo "";

forge verify-contract \
  --etherscan-api-key $ETHERSCAN_API_KEY \
  --chain 137 --watch \
  --constructor-args $(cast abi-encode "constructor(uint256, uint256)" "2160" "2400") \
  $IMPLEMENTATION_ADDRESS \
  RollupProcessorLatest