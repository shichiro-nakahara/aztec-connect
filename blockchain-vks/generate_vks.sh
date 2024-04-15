#!/bin/bash
set -e

OUTPUT_DIR=../../blockchain-vks/keys

cd ../aztec-connect-cpp/

(cd barretenberg/cpp/srs_db && ./download_ignition.sh 10)

cd build

mkdir -p $OUTPUT_DIR

./bin/keygen 28 32 $OUTPUT_DIR 2> >(awk '$0="real28x32: "$0' 1>&2)