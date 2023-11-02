#!/bin/bash

forge verify-contract \
  --etherscan-api-key $ETHERSCAN_API_KEY \
  --chain 137 --watch \
  "0x0B36beCB3C1De85A8f12b4aB201C1dA8C1D405C6" \
  Verifier28x32