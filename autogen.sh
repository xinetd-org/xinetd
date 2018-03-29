#!/bin/bash

set -e

CURDIR=$(pwd)
SCRIPT_DIR=$(cd $(dirname $0); pwd)

cd $SCRIPT_DIR

autoreconf -v -i -f
