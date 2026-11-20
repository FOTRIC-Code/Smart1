#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SDK_VARIANT="${SDK_VARIANT:-ubuntu22}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build/${SDK_VARIANT}}"
BIN_DIR="${BIN_DIR:-${SCRIPT_DIR}/bin/${SDK_VARIANT}/IRtekNetSDKDemo}"
JOBS="${JOBS:-$(nproc)}"
IRTEKNETSDK_PACKAGE_ROOT="${IRTEKNETSDK_PACKAGE_ROOT:-${PRODUCT_ROOT}/library/${SDK_VARIANT}}"

case "${SDK_VARIANT}" in
  rk3506|ubuntu22) ;;
  *)
    echo "Unsupported SDK_VARIANT: ${SDK_VARIANT}" >&2
    echo "Expected one of: rk3506, ubuntu22" >&2
    exit 1
    ;;
esac

cmake_args=(
  -S "${SCRIPT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE=Release
  -DIRTEK_LINUX_VARIANT="${SDK_VARIANT}"
  -DIRTEKNETSDK_PACKAGE_ROOT="${IRTEKNETSDK_PACKAGE_ROOT}"
)

if [[ "${SDK_VARIANT}" == "rk3506" ]]; then
  : "${CMAKE_TOOLCHAIN_FILE:?Set CMAKE_TOOLCHAIN_FILE to your rk3506 toolchain file}"
  : "${TOOLCHAIN_PREFIX:?Set TOOLCHAIN_PREFIX to your rk3506 cross compiler prefix}"
  : "${SYSROOT:?Set SYSROOT to your rk3506 target sysroot}"
  cmake_args+=(
    -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}"
    -DTOOLCHAIN_PREFIX="${TOOLCHAIN_PREFIX}"
    -DSYSROOT="${SYSROOT}"
  )
elif [[ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]]; then
  cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}")
  if [[ -n "${SYSROOT:-}" ]]; then
    cmake_args+=(-DSYSROOT="${SYSROOT}")
  fi
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

mkdir -p "${BIN_DIR}"
cp "${BUILD_DIR}/irtek_net_sdk_demo" "${BIN_DIR}/"
printf 'Demo binary copied to: %s
' "${BIN_DIR}/irtek_net_sdk_demo"
printf 'Using package root: %s
' "${IRTEKNETSDK_PACKAGE_ROOT}"
