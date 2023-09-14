#!/bin/bash

set -e

SCRIPT_DIR=$(dirname "$0")

BZLREG="$PWD/$SCRIPT_DIR/../bazel-bin/bzlreg/bzlreg"
BZLMOD="$PWD/$SCRIPT_DIR/../bazel-bin/bzlmod/bzlmod"

TEST_REG_DIR="$PWD/$SCRIPT_DIR/reg"
TEST_MODULE_DIR="$PWD/$SCRIPT_DIR/module"

echo initializing test registry
$BZLREG init $TEST_REG_DIR

echo adding rules_cc to test registry
$BZLREG add-module https://github.com/bazelbuild/rules_cc/releases/download/0.0.8/rules_cc-0.0.8.tar.gz --strip-prefix=rules_cc-0.0.8 --registry=$TEST_REG_DIR

echo initializing test module
$BZLMOD init $TEST_MODULE_DIR

echo "common --registry=file://$TEST_REG_DIR" >> $TEST_MODULE_DIR/.bazelrc

cd $TEST_MODULE_DIR
$BZLMOD add rules_cc

echo done
