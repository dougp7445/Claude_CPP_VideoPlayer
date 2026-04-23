#!/bin/bash
set -e

cd build
ctest -C Release --output-on-failure
