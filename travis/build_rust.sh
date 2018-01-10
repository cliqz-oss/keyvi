#!/usr/bin/env bash
set -ev

cd rskeyvi
cargo build --verbose
cargo test --verbose
