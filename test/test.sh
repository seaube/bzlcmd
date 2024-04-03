#!/bin/bash

set -e

SCRIPT_DIR=$(dirname "$0")

BAZEL_BIN=$(bazelisk info bazel-bin)

BZLREG="$BAZEL_BIN/bzlreg/bzlreg"
BZLMOD="$BAZEL_BIN/bzlmod/bzlmod"

TEST_REG_DIR="$PWD/$SCRIPT_DIR/reg"
TEST_MODULE_DIR="$PWD/$SCRIPT_DIR/module"

rm -rf $TEST_REG_DIR
rm -rf $TEST_MODULE_DIR

echo initializing test registry
$BZLREG init $TEST_REG_DIR
echo adding rules_cc to test registry
$BZLREG add-module https://github.com/bazelbuild/rules_cc/releases/download/0.0.8/rules_cc-0.0.8.tar.gz --strip-prefix=rules_cc-0.0.8 --registry=$TEST_REG_DIR

echo adding known problem-some archive
$BZLREG add-module https://github.com/ecsact-dev/ecsact_lang_cpp/releases/download/0.3.4/ecsact_lang_cpp-0.3.4.tar.gz --registry=$TEST_REG_DIR

echo initializing test module
$BZLMOD init $TEST_MODULE_DIR

echo "common --registry=file://$TEST_REG_DIR" >> $TEST_MODULE_DIR/.bazelrc

cd $TEST_MODULE_DIR
$BZLMOD add rules_cc

echo done
