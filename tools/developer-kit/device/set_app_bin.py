# set_app_bin.py -- device-side payload that updates main_app's app_bin config.
#
# Single canonical copy of the remote logic shared by the deploy scripts:
#   - update_app_bin.sh            (Linux/macOS)
#   - windows/update_app_bin.ps1   (Windows native)
#   - windows/install_user_app.ps1 (Windows native)
# They pipe this file over ssh into the device's `python3 -`; sys.argv[1] is
# the target app_bin path. Do NOT keep per-script copies of this logic.
#
# Constraints:
#   - stdlib only (the device has no pip);
#   - keep this file pure ASCII with LF line endings -- Windows PowerShell 5.1
#     encodes piped native-stdin as ASCII, and .gitattributes enforces LF;
#   - rewrite /etc/cfg/main_app/config.json atomically (same-dir tmp + replace).
import json
import os
import sys
import tempfile

target = sys.argv[1]
cfg_dir = "/etc/cfg/main_app"
cfg_path = os.path.join(cfg_dir, "config.json")

os.makedirs(cfg_dir, exist_ok=True)

data = {}
if os.path.exists(cfg_path):
    with open(cfg_path, "r", encoding="utf-8") as fp:
        try:
            data = json.load(fp)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"config decode failed: {exc}")

data["app_bin"] = target

fd, tmp_path = tempfile.mkstemp(prefix="config.", suffix=".json", dir=cfg_dir)
os.close(fd)
with open(tmp_path, "w", encoding="utf-8") as fp:
    json.dump(data, fp, indent=2, ensure_ascii=True)
    fp.write("\n")
os.replace(tmp_path, cfg_path)
