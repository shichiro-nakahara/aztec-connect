#!/bin/bash
# Assumes we have valid binaries at expected location.
# Builds a fixture (rollup proof) to input specification.
set -e

TXS=${1:-1}
INNER_SIZE=${2:-1}
OUTER_SIZE=${3:-1}
MOCK_PROOF=${4:-false}

[ "$MOCK_PROOF" = "true" ] && PREFIX="mock_"

CWD=$PWD
cd ../../../barretenberg/cpp/build

# Ensure bidirectional pipe exists to feed request/response between tx_factory and rollup_cli.
rm -rf pipe && mkfifo pipe

./bin/tx_factory \
    $TXS $INNER_SIZE $OUTER_SIZE false $MOCK_PROOF \
    $CWD/${PREFIX}rollup_proof_data_${INNER_SIZE}x${OUTER_SIZE}.dat < pipe 2> >(awk '$0="tx_factory: "$0' 1>&2) |
    ./bin/rollup_cli ../srs_db/ignition $INNER_SIZE $OUTER_SIZE $MOCK_PROOF false false > pipe 2> >(awk '$0="rollup_cli: "$0' 1>&2)