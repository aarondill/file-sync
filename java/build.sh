#!/usr/bin/env bash
set -euC
shopt -s globstar
cd "$(dirname -- "$0")"
javac -d build ./src/**/*.java
