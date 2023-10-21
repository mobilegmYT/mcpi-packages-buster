#!/bin/sh

set -e

# Run
#docker run --rm -v "$(pwd):/data" -w "/data" buildpack-deps:bullseye sh -c "./scripts/ci/run.sh"

docker run --rm -v "$(pwd):/data" -w '/data' noozsbc/mcpi-reborn-extended-build:busterv2 ./scripts/ci/run.sh
