#!/bin/bash
set -e
cd ../barretenberg && mkdir -p build && cd build && cmake .. && make -j$(nproc) keygen
./bin/keygen 28 32 ../../blockchain/contracts/verifier/keys
cd ../../blockchain
yarn compile