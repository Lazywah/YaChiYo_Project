#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
手機記事接收器 — iPhone 捷徑經 Tailscale 把文字 POST 進電腦，附加到 inbox.md。
Phone note receiver: the iPhone shortcut POSTs text over Tailscale and this script
appends it to inbox.md. It is the "PC as cloud" half of To-do Item 17.

安全設計 / Safety by design:
  只做「寫文字檔 / 讀摘要檔」兩件事，不碰 Hermes、不執行任何東西 →
  就算 token 外流，對方最多只能塞文字進 inbox，沒辦法叫電腦執行指令。
  只綁 Tailscale IP（拒絕 0.0.0.0），區網 / 公網都連不到；每個請求都要 Bearer token。

端點 / Endpoints (全部需要 Authorization: Bearer <token>):
  POST /note     本文 JSON {"text": "...", "time": "(選) 手機端時間"} 或純文字 → 附加到 inbox.md
  GET  /summary  回傳 summary.md（給手機塞進 system prompt 的記憶摘要；沒有就回空字串）
  GET  /health   連線測試

資料位置 / Data (不進 Git)，預設 %LOCALAPPDATA%\\YaChiYo\\phone\\ :
  inbox.md    手機送來的記事（Hermes 讀這個）
  summary.md  給手機讀的摘要（由 Hermes / 手動寫入）
  token.txt   第一次啟動自動產生；複製到 iPhone 捷徑裡

用法 / Usage:
    python tools/note_receiver.py                     # 自動抓 `tailscale ip -4`
    python tools/note_receiver.py --host 127.0.0.1    # 本機測試
  由 start_yachiyo.ps1 一起啟動、stop_yachiyo.ps1 一起關閉。
"""

import argparse
import hmac
import json
import os
import secrets
import shutil
import subprocess
import sys
import threading
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

# ---- 設定 / Config ------------------------------------------------------------------------
PORT = 39220                    # 與桌寵 39217 (事件) / 39218 (嘴型) 排在一起
MAX_BODY = 256 * 1024           # 手機補送整批暫存也夠用
DEFAULT_DATA_DIR = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "YaChiYo" / "phone"
TAILSCALE_EXE = r"C:\Program Files\Tailscale\tailscale.exe"

_append_lock = threading.Lock()


def log(msg):
    print(f"[{datetime.now():%H:%M:%S}] {msg}", flush=True)


def tailscale_ip():
    """回傳本機 Tailscale IPv4；Tailscale 沒開 / 沒裝回 None。"""
    exe = shutil.which("tailscale") or TAILSCALE_EXE
    try:
        out = subprocess.run([exe, "ip", "-4"], capture_output=True, text=True, timeout=10,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except (OSError, subprocess.TimeoutExpired):
        return None
    lines = out.stdout.strip().splitlines() if out.returncode == 0 else []
    return lines[0].strip() if lines else None


def load_token(data_dir):
    """讀 token.txt；不存在就產生一把新的（不印出 token 本身）。"""
    path = data_dir / "token.txt"
    if path.exists():
        token = path.read_text(encoding="utf-8").strip()
        if token:
            return token
    token = secrets.token_urlsafe(32)
    path.write_text(token + "\n", encoding="utf-8")
    log(f"已產生新 token / new token written: {path}")
    return token


def append_note(data_dir, text, when=None):
    # 手機補送時帶它自己的時間，否則用收到的時間。去掉換行以免弄亂 Markdown 標題。
    stamp = " ".join(str(when).split())[:64] if when else datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with _append_lock, open(data_dir / "inbox.md", "a", encoding="utf-8") as f:
        f.write(f"## {stamp}\n{text}\n\n")


class Handler(BaseHTTPRequestHandler):
    token = ""
    data_dir = DEFAULT_DATA_DIR
    server_version = "YaChiYoNote/1"

    def log_message(self, fmt, *args):
        log(f"{self.client_address[0]} {fmt % args}")

    def _reply(self, code, obj=None, text=None):
        if text is None:
            body, ctype = json.dumps(obj, ensure_ascii=False).encode("utf-8"), "application/json"
        else:
            body, ctype = text.encode("utf-8"), "text/plain"
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _authorized(self):
        auth = self.headers.get("Authorization", "")
        ok = auth.startswith("Bearer ") and hmac.compare_digest(
            auth[7:].strip().encode("utf-8"), self.token.encode("utf-8"))
        if not ok:
            self._reply(401, {"ok": False, "error": "unauthorized"})
        return ok

    def do_GET(self):
        if not self._authorized():
            return
        if self.path == "/health":
            self._reply(200, {"ok": True})
        elif self.path == "/summary":
            path = self.data_dir / "summary.md"
            self._reply(200, text=path.read_text(encoding="utf-8") if path.exists() else "")
        else:
            self._reply(404, {"ok": False, "error": "not found"})

    def do_POST(self):
        if not self._authorized():
            return
        if self.path != "/note":
            return self._reply(404, {"ok": False, "error": "not found"})
        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            length = 0
        if length <= 0:
            return self._reply(400, {"ok": False, "error": "empty"})
        if length > MAX_BODY:
            return self._reply(413, {"ok": False, "error": "too large"})

        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        text, when = raw, None
        if "json" in self.headers.get("Content-Type", ""):
            try:
                data = json.loads(raw)
            except json.JSONDecodeError:
                data = None
            if not isinstance(data, dict):
                return self._reply(400, {"ok": False, "error": "bad json"})
            text, when = str(data.get("text", "")), data.get("time")
        text = text.strip()
        if not text:
            return self._reply(400, {"ok": False, "error": "empty"})

        append_note(self.data_dir, text, when)
        log(f"收到記事 {len(text)} 字 / note received ({len(text)} chars)")   # 不記內容
        self._reply(200, {"ok": True})


def main():
    ap = argparse.ArgumentParser(description="手機記事接收器 (Tailscale only)")
    ap.add_argument("--host", help="綁定位址；預設自動抓 Tailscale IPv4")
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--data-dir", type=Path, default=DEFAULT_DATA_DIR)
    args = ap.parse_args()

    host = args.host or tailscale_ip()
    if not host:
        log("找不到 Tailscale IP（Tailscale 沒開？）— 不啟動。 / no Tailscale IP, not starting")
        return 1
    if host in ("0.0.0.0", "::"):
        log("拒絕綁全部介面（會開給整個網路）。 / refusing to bind all interfaces")
        return 1

    args.data_dir.mkdir(parents=True, exist_ok=True)
    Handler.data_dir = args.data_dir
    Handler.token = load_token(args.data_dir)
    try:
        server = ThreadingHTTPServer((host, args.port), Handler)
    except OSError as e:
        log(f"無法綁定 {host}:{args.port}: {e}")
        return 1
    log(f"聽 http://{host}:{args.port}  資料夾 {args.data_dir}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
