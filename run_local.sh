#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")" && pwd)"
export GOCACHE="${root_dir}/.gocache"
mkdir -p "${GOCACHE}"

cd "${root_dir}"
build_dir="${root_dir}/build"
cmake -S . -B "${build_dir}" \
  -DCMAKE_CXX_COMPILER="${CXX:-g++-13}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
cmake --build "${build_dir}"

cd web
go run .
