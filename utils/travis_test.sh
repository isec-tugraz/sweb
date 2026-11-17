#!/usr/bin/env bash
# Kept under its historical name for existing callers. Run from a build directory.
set -euo pipefail

mode=all
case "${1:-}" in
  --build-only) mode=build; shift ;;
  --boot-only) mode=boot; shift ;;
esac
if [[ $# != 1 ]]; then
  echo "Usage: $0 [--build-only|--boot-only] {x86_64|x86_32|x86_32_pae|arm_rpi2|armv8_rpi3}" >&2
  exit 2
fi
architecture=$1
extra_flags=()
case "$architecture" in
  x86_64) arch=x86/64 ;;
  x86_32) arch=x86/32 ;;
  x86_32_pae) arch=x86/32/pae ;;
  arm_rpi2) arch=arm/rpi2 ;;
  armv8_rpi3) arch=armv8/rpi3; extra_flags=(-DVIRTUALIZED_QEMU=1) ;;
  *) echo "Unknown architecture: $architecture" >&2; exit 2 ;;
esac
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $(pwd -P) == "$source_dir" ]]; then
  echo "Use a separate build directory for each architecture." >&2
  exit 2
fi
# Reusing CMake's cache across architectures can retain the wrong compiler.
if [[ -f CMakeCache.txt ]] && ! grep -Eq "^ARCH:[^=]+=$arch$" CMakeCache.txt; then
  echo "CMake cache does not match $arch; use a fresh build directory." >&2
  exit 2
fi

mkdir -p ci-logs
if [[ $mode != boot ]]; then
  rm -f ci-logs/result.json
  echo "Building architecture $architecture"
  cmake -S "$source_dir" -B . -DARCH="$arch" "${extra_flags[@]}" 2>&1 | tee ci-logs/configure.log
  cmake --build . --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}" 2>&1 | tee ci-logs/build.log
  echo "PASS: $architecture build"
fi
if [[ $mode != build ]]; then
  exec python3 "$source_dir/utils/qemu_boot.py" "$architecture" --log-dir ci-logs
fi
