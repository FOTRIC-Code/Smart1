#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SDK_VARIANT="${SDK_VARIANT:-ubuntu22}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build/${SDK_VARIANT}}"
BIN_DIR="${BIN_DIR:-${SCRIPT_DIR}/bin/${SDK_VARIANT}/ThermalFileReaderDemo}"
JOBS="${JOBS:-$(nproc)}"

case "${SDK_VARIANT}" in
  rk3506|ubuntu20|ubuntu22) ;;
  *)
    echo "Unsupported SDK_VARIANT: ${SDK_VARIANT}" >&2
    echo "Expected one of: rk3506, ubuntu20, ubuntu22" >&2
    exit 1
    ;;
esac

resolve_package_root() {
  if [[ -n "${THERMALIMAGE_PACKAGE_ROOT:-}" ]]; then
    printf '%s\n' "${THERMALIMAGE_PACKAGE_ROOT}"
    return 0
  fi

  local preferred_root="${PRODUCT_ROOT}/library/${SDK_VARIANT}"
  if [[ -d "${preferred_root}" ]]; then
    printf '%s\n' "${preferred_root}"
    return 0
  fi

  if [[ "${SDK_VARIANT}" == "ubuntu22" ]]; then
    local ubuntu20_root="${PRODUCT_ROOT}/library/ubuntu20"
    if [[ -d "${ubuntu20_root}" ]]; then
      printf '%s\n' "${ubuntu20_root}"
      return 0
    fi
  fi

  printf '%s\n' "${preferred_root}"
}

THERMALIMAGE_PACKAGE_ROOT="$(resolve_package_root)"
if [[ "${THERMALIMAGE_PACKAGE_ROOT}" != "${PRODUCT_ROOT}/library/${SDK_VARIANT}" ]]; then
  printf 'SDK_VARIANT=%s package root not found, falling back to: %s\n' \
    "${SDK_VARIANT}" "${THERMALIMAGE_PACKAGE_ROOT}" >&2
fi

LIB_DIR="${THERMALIMAGE_PACKAGE_ROOT}/library"

if [[ ! -f "${LIB_DIR}/libIRtekFileSDK.so" ]]; then
  echo "Missing libIRtekFileSDK.so under ${LIB_DIR}" >&2
  echo "Populate ThermalImage/library/<variant>/library/ first or set THERMALIMAGE_PACKAGE_ROOT." >&2
  exit 1
fi

export LD_LIBRARY_PATH="${LIB_DIR}${THERMALIMAGE_EXTRA_LIBRARY_PATHS:+:${THERMALIMAGE_EXTRA_LIBRARY_PATHS}}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
MISSING_DEPS="$(ldd "${LIB_DIR}/libIRtekFileSDK.so" | awk '/=> not found/ {print}')"
if [[ -n "${MISSING_DEPS}" ]]; then
  echo "Unresolved runtime dependencies for ${LIB_DIR}/libIRtekFileSDK.so:" >&2
  printf '%s\n' "${MISSING_DEPS}" >&2
  echo "Install the matching .so files or set THERMALIMAGE_EXTRA_LIBRARY_PATHS to directories that contain them." >&2
  exit 1
fi

cmake_args=(
  -S "${SCRIPT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE=Release
  -DIRTEK_LINUX_VARIANT="${SDK_VARIANT}"
  -DTHERMALIMAGE_PACKAGE_ROOT="${THERMALIMAGE_PACKAGE_ROOT}"
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
cp "${BUILD_DIR}/thermal_file_reader_demo" "${BIN_DIR}/"
printf 'Demo binary copied to: %s
' "${BIN_DIR}/thermal_file_reader_demo"
printf 'Using package root: %s
' "${THERMALIMAGE_PACKAGE_ROOT}"
