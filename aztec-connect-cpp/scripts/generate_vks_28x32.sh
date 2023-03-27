#!/bin/bash
set -e

OUTPUT_DIR=../../contracts/src/core/verifier/keys

# cd .. && mkdir -p build && cd build && cmake .. && cmake --build . --parallel --target keygen

cd .. && cd build

./bin/keygen 28 32 $OUTPUT_DIR 2> >(awk '$0="real28x32: "$0' 1>&2)