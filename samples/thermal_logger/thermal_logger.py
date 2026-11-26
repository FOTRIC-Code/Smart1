#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""thermal_logger —— 全时温度记录示例（纯标准库 + irtek_thermal 绑定）。

功能：按固定间隔采集热像温度（ROI 或全帧），落 CSV，并内置一个 HTTP 页面
实时展示温度曲线。演示「持续采集 -> 自动记录 -> 曲线呈现」的最小闭环。

部署（详见 README.md）：
    scp tools/developer-kit/python/irtek_thermal.py root@192.168.123.100:/usr/bin/
    bash tools/developer-kit/install_user_app.sh 192.168.123.100 \\
        samples/thermal_logger/thermal_logger.py

注意：本程序独占热像设备（libusb claim），部署为 app_bin 后 net_app 不再启动，
属预期行为；恢复出厂功能把 app_bin 改回 /usr/bin/net_app 即可。
"""

import argparse
import csv
import json
import logging
import os
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# 设备上：irtek_thermal.py 与本脚本同目录（如 /usr/bin/），直接可导入；
# 仓库内开发：回退到 tools/developer-kit/python/。
try:
    import irtek_thermal
except ImportError:
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", "..", "tools", "developer-kit", "python"))
    import irtek_thermal

LOG_PATH = "/tmp/thermal_logger.log"
HISTORY_LEN = 3600  # 1Hz 下 1 小时

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("thermal_logger")
try:
    log.addHandler(logging.FileHandler(LOG_PATH))
except OSError:
    pass


class LoggerState(object):
    def __init__(self, roi):
        self.roi = roi  # (x, y, w, h) 或 None
        self.history = deque(maxlen=HISTORY_LEN)  # dict 序列
        self.latest = None
        self.lock = threading.Lock()
        self.camera = None

    def record(self, item):
        with self.lock:
            self.history.append(item)
            self.latest = item


_HTML_PAGE = """<!DOCTYPE html>
<html lang="zh"><head><meta charset="utf-8">
<meta http-equiv="refresh" content="5">
<title>thermal_logger</title>
<style>
body{{font-family:sans-serif;margin:24px;background:#14181f;color:#dfe6ee}}
h1{{font-size:18px}} .cards span{{display:inline-block;margin-right:24px}}
svg{{background:#0c0f14;border:1px solid #2a3342;margin-top:12px}}
</style></head><body>
<h1>Thermal Logger <small>({roi})</small></h1>
<div class="cards">{cards}</div>
{svg}
<p style="color:#8a94a6">每 5s 自动刷新 ｜ <a style="color:#7db8ff" href="/api/history">/api/history</a>
｜ 最近 {n} 个采样点</p>
</body></html>"""


def _svg_curve(points, width=900, height=260):
    """points: [(t_epoch, max_c, min_c, avg_c_or_None), ...] -> 简易 SVG 曲线。"""
    if len(points) < 2:
        return "<p>数据积累中…</p>"
    series = [("max", 1, "#ff6b6b"), ("avg", 3, "#ffd166"), ("min", 2, "#4ecdc4")]
    vals = [p[i] for p in points for _, i, _ in series if p[i] is not None]
    if not vals:
        return "<p>无有效数据</p>"
    lo, hi = min(vals), max(vals)
    if hi - lo < 0.5:
        hi = lo + 0.5
    t0, t1 = points[0][0], points[-1][0] or points[0][0] + 1
    span = max(t1 - t0, 1e-6)

    def xy(i, v):
        x = (points[i][0] - t0) / span * (width - 60) + 40
        y = height - 20 - (v - lo) / (hi - lo) * (height - 40)
        return "%.1f,%.1f" % (x, y)

    parts = ['<svg viewBox="0 0 %d %d" width="%d" height="%d">' % (width, height, width, height)]
    parts.append('<text x="8" y="18" fill="#dfe6ee" font-size="12">%.1f°C</text>' % hi)
    parts.append('<text x="8" y="%d" fill="#dfe6ee" font-size="12">%.1f°C</text>'
                 % (height - 8, lo))
    for name, idx, color in series:
        pts = " ".join(xy(i, p[idx]) for i, p in enumerate(points) if p[idx] is not None)
        if pts:
            parts.append('<polyline fill="none" stroke="%s" stroke-width="1.5" points="%s"/>'
                         % (color, pts))
        parts.append('<text x="%d" y="%d" fill="%s" font-size="12">%s</text>'
                     % (width - 52, 16 + 16 * ["max", "avg", "min"].index(name), color, name))
    parts.append("</svg>")
    return "".join(parts)


class HttpHandler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, body, ctype="text/html; charset=utf-8", code=200):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        state = self.server.state
        if self.path.startswith("/api/history"):
            with state.lock:
                data = list(state.history)
            self._send(json.dumps(data, ensure_ascii=False), "application/json")
            return
        if self.path.startswith("/api/latest"):
            with state.lock:
                data = state.latest or {}
            self._send(json.dumps(data, ensure_ascii=False), "application/json")
            return

        with state.lock:
            latest = state.latest
            points = [(p["epoch_s"], p["max_c"], p["min_c"], p["avg_c"])
                      for p in state.history]
        if latest:
            cards = ("<span>最高 <b>%.1f°C</b></span><span>最低 <b>%.1f°C</b></span>"
                     "<span>平均 <b>%s°C</b></span><span>中心 <b>%.1f°C</b></span>"
                     % (latest["max_c"], latest["min_c"],
                        ("%.1f" % latest["avg_c"]) if latest["avg_c"] is not None else "—",
                        latest["center_c"]))
        else:
            cards = "<span>等待首帧…</span>"
        roi_text = ("ROI %dx%d@(%d,%d)" % (state.roi[2], state.roi[3],
                                           state.roi[0], state.roi[1])
                    if state.roi else "全帧")
        self._send(_HTML_PAGE.format(roi=roi_text, cards=cards,
                                     svg=_svg_curve(points), n=len(points)))


def run_http_server(state, port):
    srv = ThreadingHTTPServer(("0.0.0.0", port), HttpHandler)
    srv.state = state
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    log.info("HTTP 曲线页: http://<设备IP>:%d/", port)
    return srv


def parse_roi(text):
    parts = [int(p) for p in text.split(",")]
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("ROI 格式: x,y,w,h")
    return tuple(parts)


def main():
    ap = argparse.ArgumentParser(description="全时温度记录示例")
    ap.add_argument("--interval", type=float, default=1.0, help="采样间隔秒（默认 1.0）")
    ap.add_argument("--roi", type=parse_roi, default=None,
                    help="采样区域 x,y,w,h（缺省全帧；全帧不计算平均温）")
    ap.add_argument("--csv", default=None, help="CSV 输出路径")
    ap.add_argument("--http", type=int, default=8080, help="HTTP 端口（默认 8080，0 关闭）")
    ap.add_argument("--count", type=int, default=0, help="采样 N 次后退出（默认 0=持续）")
    ap.add_argument("--device-index", type=int, default=0)
    args = ap.parse_args()

    csv_path = args.csv
    if not csv_path:
        csv_path = ("/userdata/parkia/thermal_log.csv"
                    if os.path.isdir("/userdata/parkia") else "./thermal_log.csv")

    state = LoggerState(args.roi)
    if args.http:
        run_http_server(state, args.http)

    cam = irtek_thermal.ThermalCamera(device_index=args.device_index)
    state.camera = cam
    cam.open()
    log.info("采集开始 interval=%.2fs roi=%s csv=%s lib=%s",
             args.interval, args.roi or "全帧", csv_path, cam.library_info())

    with open(csv_path, "a", newline="") as fp:
        writer = csv.writer(fp)
        if fp.tell() == 0:
            writer.writerow(["time_iso", "epoch_s", "frame_no", "pts_us", "roi",
                             "max_c", "min_c", "avg_c", "center_c", "lut_crc"])
        n = 0
        try:
            while True:
                t0 = time.monotonic()
                try:
                    frame = cam.get_frame(timeout=max(2.0, args.interval * 3))
                except irtek_thermal.FrameTimeoutError:
                    log.warning("取帧超时，重试中… stats=%s", cam.stats())
                    continue

                center_c = frame.center_temp_c()
                if state.roi:
                    s = frame.roi_stats(*state.roi)
                    max_c, min_c, avg_c = s["max_c"], s["min_c"], s["avg_c"]
                    roi_text = "%d,%d,%d,%d" % state.roi
                else:
                    max_c, min_c, avg_c = frame.max_temp_c(), frame.min_temp_c(), None
                    roi_text = "FULL"

                item = {
                    "time": time.strftime("%Y-%m-%dT%H:%M:%S"),
                    "epoch_s": round(time.time(), 3),
                    "frame_no": frame.frame_no,
                    "max_c": round(max_c, 2), "min_c": round(min_c, 2),
                    "avg_c": round(avg_c, 2) if avg_c is not None else None,
                    "center_c": round(center_c, 2),
                }
                state.record(item)
                writer.writerow([item["time"], item["epoch_s"], frame.frame_no,
                                 frame.pts_us, roi_text, item["max_c"], item["min_c"],
                                 item["avg_c"], item["center_c"], "%08x" % frame.lut_crc])
                fp.flush()
                n += 1
                log.info("#%d max=%.1f min=%.1f avg=%s center=%.1f °C",
                         n, max_c, min_c,
                         ("%.1f" % avg_c) if avg_c is not None else "—", center_c)
                if args.count and n >= args.count:
                    break
                time.sleep(max(0.0, args.interval - (time.monotonic() - t0)))
        except KeyboardInterrupt:
            log.info("收到中断，退出")
    cam.close()
    log.info("结束，共 %d 个采样点 -> %s", n, csv_path)


if __name__ == "__main__":
    main()
