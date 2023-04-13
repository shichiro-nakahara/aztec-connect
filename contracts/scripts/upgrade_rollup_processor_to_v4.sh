#!/bin/sh
set -eu

# Runs the upgradeV4 script to perform RollupProcessor upgrade
#
# Expected enviornment variables
# - PROXY - Address of the proxy/rollup contract
# - MOCK_VERIFIER - If true will deploy a mock verifier (otherwise 28x32)
# - ETHEREUM_HOST - Target chain rpc
# - PRIVATE_KEY - Deployer key

MOCK_VERIFIER=${MOCK_VERIFIER:=true}

# Execute deployment solidity script
forge script UpgradeV4 --ffi --private-key $PRIVATE_KEY --broadcast --rpc-url $ETHEREUM_HOST --sig "upgrade(address,bool)" \
  $PROXY \
  $MOCK_VERIFIER