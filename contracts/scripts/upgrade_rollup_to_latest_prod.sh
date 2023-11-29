#!/bin/sh
set -eu

# Runs the upgradeV6 script to perform RollupProcessor upgrade
#
# Expected enviornment variables
# - PROXY - Address of the proxy/rollup contract
# - ETHEREUM_HOST - Target chain rpc
# - PRIVATE_KEY - Deployer key

echo "";

read -p "Please enter the current rollup's implementation version: " OLD_VERSION

echo ""

read -p "Please enter the new rollup's implementation version: " NEW_VERSION

echo ""

read -n 1 -s -r -p "Please pause the rollup. Press any key to continue."

echo -e "\n\nDeploying RollupProcessorLatest...\n"

# Execute deployment solidity script
forge script UpgradeLatestProd --ffi -vvvv --private-key $PRIVATE_KEY --broadcast --rpc-url $ETHEREUM_HOST \
  --sig "deploy(address, uint256, uint256)" \
  $PROXY \
  $OLD_VERSION \
  $NEW_VERSION

read -p "Please enter the address of the old rollup implementation: " OLD_ROLLUP_ADDRESS

echo ""

read -p "Please enter the address of the new rollup implementation: " NEW_ROLLUP_ADDRESS

echo -e "\nPlease call upgradeAndCall() on proxy admin contract. Pass '0x8129fc1c' in the 'data' field.\n"

read -n 1 -s -r -p "Press any key to continue."

echo -e "\n\nVerifying upgrade...\n"

forge script UpgradeLatestProd --ffi -vvvv --private-key $PRIVATE_KEY --broadcast --rpc-url $ETHEREUM_HOST \
  --sig "verify(address, address, address, uint256)" \
  $PROXY \
  $NEW_ROLLUP_ADDRESS \
  $OLD_ROLLUP_ADDRESS \
  $NEW_VERSION