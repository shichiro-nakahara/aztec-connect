#!/bin/sh

# Expected enviornment variables
# - ETHEREUM_HOST - Target chain rpc
# - PRIVATE_KEY - Deployer key

# Execute deployment solidity script
forge script DeployGasOracle --ffi --private-key $PRIVATE_KEY --broadcast --rpc-url $ETHEREUM_HOST --sig "deploy()"