#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

usage() {
  cat <<'EOF'
Usage:
  update_app_bin.sh [-u user] [-P port] <host> <app_bin_path>

Examples:
  update_app_bin.sh 192.168.123.100 /usr/bin/my_app
  update_app_bin.sh -u root -P 22 cube.local /usr/bin/net_app
EOF
}

SSH_USER="root"
SSH_PORT="22"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -u|--user)
      SSH_USER="${2:-}"
      shift 2
      ;;
    -P|--port)
      SSH_PORT="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      die "unknown argument: $1"
      ;;
    *)
      break
      ;;
  esac
done

[[ $# -eq 2 ]] || {
  usage >&2
  exit 2
}

HOST="$1"
TARGET_APP_BIN="$2"
TARGET_LOGIN="$(remote_login "${HOST}" "${SSH_USER}")"

need_cmd ssh

log "updating app_bin on ${TARGET_LOGIN}:${SSH_PORT} -> ${TARGET_APP_BIN}"

# 远端逻辑唯一副本：device/set_app_bin.py（.sh/.ps1 三脚本共用，勿在此另存副本）
ssh -p "${SSH_PORT}" "${TARGET_LOGIN}" "python3 - \"${TARGET_APP_BIN}\"" \
  < "${SCRIPT_DIR}/device/set_app_bin.py"

log "app_bin updated"
