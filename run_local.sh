#!/usr/bin/env bash
set -e

root_dir="$(cd "$(dirname "$0")" && pwd)"
export GOCACHE="${root_dir}/.gocache"
mkdir -p "${GOCACHE}"

cd "${root_dir}"
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-13
cmake --build build

cd web
go run .
