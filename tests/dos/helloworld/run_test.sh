#!/bin/bash
cd "$(dirname "$0")"
../../test_runner.sh "../../../target/x86_64-linux/bin/lxa" "../../../src/rom/lxa.rom" "helloworld" "expected.out" "actual.out"
