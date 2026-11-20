#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SDK_VARIANT="${SDK_VARIANT:-ubuntu22}"
BIN_PATH="${SCRIPT_DIR}/bin/${SDK_VARIANT}/IRtekUVCSDKDemo/irtek_uvc_sdk_demo"
IRTEKUVCSDK_PACKAGE_ROOT="${IRTEKUVCSDK_PACKAGE_ROOT:-${PRODUCT_ROOT}/library/${SDK_VARIANT}}"
UVC_LIB_DIR="${IRTEKUVCSDK_PACKAGE_ROOT}/library"
LOCAL_FILESDK_LIBRARY="${UVC_LIB_DIR}/libIRtekFileSDK.so"
SHARED_FILESDK_LIBRARY="${REPO_ROOT}/ThermalImage/library/${SDK_VARIANT}/library/libIRtekFileSDK.so"

case "${SDK_VARIANT}" in
  rk3506|ubuntu22) ;;
  *)
    echo "Unsupported SDK_VARIANT: ${SDK_VARIANT}" >&2
    echo "Expected one of: rk3506, ubuntu22" >&2
    exit 1
    ;;
esac

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Missing demo binary: ${BIN_PATH}" >&2
  echo "Build it first with ./build_demo.sh" >&2
  exit 1
fi

if [[ ! -f "${UVC_LIB_DIR}/libIRtekUVCSDK.so" ]]; then
  echo "Missing libIRtekUVCSDK.so under ${UVC_LIB_DIR}" >&2
  echo "Populate IRtekUVCSDK/library/${SDK_VARIANT}/library/ first or set IRTEKUVCSDK_PACKAGE_ROOT." >&2
  exit 1
fi

export LD_LIBRARY_PATH="${UVC_LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
if [[ -f "${LOCAL_FILESDK_LIBRARY}" ]]; then
  export IRTEKFILESDK_LIBRARY="${LOCAL_FILESDK_LIBRARY}"
elif [[ -f "${SHARED_FILESDK_LIBRARY}" ]]; then
  export IRTEKFILESDK_LIBRARY="${SHARED_FILESDK_LIBRARY}"
fi

exec "${BIN_PATH}" "$@"
