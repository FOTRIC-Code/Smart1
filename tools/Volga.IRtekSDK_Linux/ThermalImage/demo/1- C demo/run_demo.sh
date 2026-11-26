#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SDK_VARIANT="${SDK_VARIANT:-ubuntu22}"
BIN_PATH="${SCRIPT_DIR}/bin/${SDK_VARIANT}/ThermalFileReaderDemo/thermal_file_reader_demo"

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

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Missing demo binary: ${BIN_PATH}" >&2
  echo "Build it first with ./build_demo.sh" >&2
  exit 1
fi

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

exec "${BIN_PATH}" "$@"
