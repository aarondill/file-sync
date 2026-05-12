#!/usr/bin/env bash
set -euC
cd "$(dirname -- "$0")"
javac -d build ./src/**.java
