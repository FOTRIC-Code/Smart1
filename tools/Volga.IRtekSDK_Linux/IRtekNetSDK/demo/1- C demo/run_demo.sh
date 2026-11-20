#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SDK_VARIANT="${SDK_VARIANT:-ubuntu22}"
BIN_PATH="${SCRIPT_DIR}/bin/${SDK_VARIANT}/IRtekNetSDKDemo/irtek_net_sdk_demo"
IRTEKNETSDK_PACKAGE_ROOT="${IRTEKNETSDK_PACKAGE_ROOT:-${PRODUCT_ROOT}/library/${SDK_VARIANT}}"
LIB_DIR="${IRTEKNETSDK_PACKAGE_ROOT}/library"
THERMAL_FILESDK_LIBRARY="${REPO_ROOT}/ThermalImage/library/${SDK_VARIANT}/library/libIRtekFileSDK.so"

pick_first() {
  local pattern path
  for pattern in "$@"; do
    for path in $pattern; do
      if [[ -f "${path}" ]]; then
        printf '%s
' "${path}"
        return 0
      fi
    done
  done
  return 1
}

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

if [[ ! -f "${LIB_DIR}/libIRtekNetSDK.so" ]]; then
  echo "Missing libIRtekNetSDK.so under ${LIB_DIR}" >&2
  echo "Populate IRtekNetSDK/library/${SDK_VARIANT}/library/ first or set IRTEKNETSDK_PACKAGE_ROOT." >&2
  exit 1
fi

export LD_LIBRARY_PATH="${LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

if [[ -f "${THERMAL_FILESDK_LIBRARY}" ]]; then
  export IRTEKFILESDK_LIBRARY="${THERMAL_FILESDK_LIBRARY}"
fi

if avformat="$(pick_first "${LIB_DIR}/libavformat.so" "${LIB_DIR}/libavformat.so.*")"; then
  export IRTEKNETSDK_AVFORMAT_LIB="${avformat}"
fi
if avcodec="$(pick_first "${LIB_DIR}/libavcodec.so" "${LIB_DIR}/libavcodec.so.*")"; then
  export IRTEKNETSDK_AVCODEC_LIB="${avcodec}"
fi
if avutil="$(pick_first "${LIB_DIR}/libavutil.so" "${LIB_DIR}/libavutil.so.*")"; then
  export IRTEKNETSDK_AVUTIL_LIB="${avutil}"
fi
if swscale="$(pick_first "${LIB_DIR}/libswscale.so" "${LIB_DIR}/libswscale.so.*")"; then
  export IRTEKNETSDK_SWSCALE_LIB="${swscale}"
fi

exec "${BIN_PATH}" "$@"
