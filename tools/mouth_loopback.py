#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Path B 擷取端 — 讀 Hermes 的音量,驅動桌寵 V2 真嘴型 (不改 Hermes)。
Path B capture — reads Hermes's output volume and drives the pet's V2 real mouth (no Hermes changes).

兩種擷取模式 / Two capture backends:
  ● per-process (預設,建議) — 只讀「Hermes 這個行程」的 WASAPI audio session 峰值 (見 session_meter.py)。
    別的 App 放音樂/通知音都不會動到嘴巴。純同步 COM,無額外套件。
  ● --whole-system (退路) — 舊行為:WASAPI loopback 錄整台喇叭輸出 (需 `pip install soundcard numpy`)。
    任何系統聲音都會驅動嘴型;僅在 per-process 抓不到目標時當備援。

原理 / How it works (兩模式共用後段):
  取得 0~1 音量 → (可選)包絡平滑 → 乘 GAIN。因為完全不改 Hermes,Hermes 不主動送事件,故本腳本一手包辦:
    1. 音量升過上緣 → 對事件埠(39217) 送 speaking (開嘴型閘門);持續靜音 → 送 idle (關閘門)。
    2. 說話期間 → 把 0~1 音量以 UDP(1 byte) 連續送到嘴型埠(39218)。
  桌寵側 (MouthStream + Live2DWidget::setMouthLevel) 已就緒,不需再改。

用法 / Usage:
    1) 桌寵以 Live2D + 語音跑起來: YaChiYo_Project.exe --live2d --voice
    2) python tools/mouth_loopback.py                 # 預設盯行程名含 "hermes"
       python tools/mouth_loopback.py --pid 12345      # 指定 Hermes PID(含子孫)
       python tools/mouth_loopback.py --whole-system    # 舊的整機 loopback 退路
    3) 讓 Hermes 唸一段話 → 桌寵嘴巴跟音量起伏;停下嘴巴閉起。
  抓不到目標時先跑 `python tools/session_meter.py --list` 看 Hermes 實際的行程名/PID。
"""

import argparse
import json
import socket
import sys
import time

# ---- 埠 / Ports (需與桌寵一致;嘴型埠 = 事件埠 + 1) --------------------------------------------
EVENT_HOST, EVENT_PORT = "127.0.0.1", 39217   # VoiceBridge (HTTP/TCP) — speaking/idle 事件
MOUTH_HOST, MOUTH_PORT = "127.0.0.1", 39218   # MouthStream  (UDP)      — 振幅串流

# ---- 共用可調參數 / Shared tunables ----------------------------------------------------------
FRAME_SEC        = 0.03      # 每幀 ~30Hz | per-frame period
START_AMP        = 0.08      # 升過此值 → 開始說話 (遲滯上緣) | rise above → start speaking
SILENCE_AMP      = 0.04      # 低於此值 → 視為靜音 (遲滯下緣) | below → silence
SILENCE_HANG_SEC = 0.5       # 靜音持續多久 → 送 idle 關閘門 | silence this long → idle
SPEAK_REARM_SEC  = 2.0       # 說話中每隔多久重送 speaking (續命桌寵逾時保險) | re-arm watchdog
SPEAK_DURATION   = 3.0       # 每次 speaking 事件帶的時長 (逾時保險視窗) | duration_sec per event
RELEASE          = 0.55      # 包絡平滑釋放係數 (峰值較尖,慢放讓嘴不抖) | envelope release factor

# ---- 各模式預設增益 (per-process 讀「峰值」已 0~1,整機讀「RMS」偏小) --------------------------
GAIN_PROCESS      = 2.5      # per-process peak → 0~1
GAIN_WHOLE        = 8.0      # whole-system RMS → 0~1


def post_event(obj: dict):
    """對 VoiceBridge 送事件 (極簡 HTTP POST /pet/event);失敗不致命 (桌寵可能沒開)。"""
    body = json.dumps(obj).encode("utf-8")
    req = (
        b"POST /pet/event HTTP/1.1\r\n"
        b"Host: 127.0.0.1\r\n"
        b"Content-Type: application/json\r\n"
        b"Content-Length: " + str(len(body)).encode() + b"\r\n"
        b"Connection: close\r\n\r\n" + body
    )
    try:
        with socket.create_connection((EVENT_HOST, EVENT_PORT), timeout=1.0) as s:
            s.sendall(req)
            try:
                s.recv(64)
            except socket.timeout:
                pass
    except OSError:
        pass   # 桌寵沒開或埠不通 — 靜默略過


class MouthGate:
    """把 0~1 振幅轉成 speaking/idle 事件 + UDP 振幅串流。兩種擷取模式共用此後段邏輯。"""

    def __init__(self, gain: float, quiet: bool):
        self.gain = gain
        self.quiet = quiet
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.speaking = False
        self.last_loud = 0.0
        self.last_rearm = 0.0

    def feed(self, amp01: float):
        """amp01: 已平滑、已乘增益、clamp 到 0~1 的振幅。依此開關閘門並送封包。"""
        amp = max(0.0, min(1.0, amp01))
        now = time.time()
        if not self.speaking:
            if amp >= START_AMP:                          # 升過上緣 → 開嘴型閘門
                self.speaking = True
                self.last_loud = self.last_rearm = now
                post_event({"event": "speaking", "duration_sec": SPEAK_DURATION})
        else:
            byte = max(0, min(255, int(amp * 255)))
            self.udp.sendto(bytes([byte]), (MOUTH_HOST, MOUTH_PORT))
            if amp >= SILENCE_AMP:
                self.last_loud = now
            if now - self.last_rearm >= SPEAK_REARM_SEC:  # 續命逾時保險
                self.last_rearm = now
                post_event({"event": "speaking", "duration_sec": SPEAK_DURATION})
            if now - self.last_loud >= SILENCE_HANG_SEC:  # 靜音夠久 → 收尾關閘門
                self.speaking = False
                post_event({"event": "idle"})
        if not self.quiet:
            bar = "#" * int(amp * 30)
            state = "SPEAK" if self.speaking else "  -  "
            print(f"\r[{state}] amp={amp:4.2f} |{bar:<30}|", end="", flush=True)

    def close(self):
        if self.speaking:
            post_event({"event": "idle"})
        self.udp.close()


# ── per-process 模式 (預設) ────────────────────────────────────────────────────────────────────
def run_per_process(args):
    try:
        import session_meter as sm
    except ImportError:
        sys.exit("找不到 session_meter.py (應與本檔同目錄 tools/)")

    try:
        meter = sm.SessionMeter(name_substr=args.process, pid=args.pid)
    except OSError as e:
        sys.exit(f"WASAPI 初始化失敗: {e}")

    tgt = f"pid={args.pid}(含子孫)" if args.pid is not None else f'行程名含「{args.process}」'
    print(f"[mouth] per-process 模式 — 盯住 {tgt} → 嘴型 {MOUTH_HOST}:{MOUTH_PORT} / 事件 {EVENT_HOST}:{EVENT_PORT}")
    print(f"[mouth] GAIN={args.gain}  平滑={'關' if args.no_smooth else f'RELEASE={RELEASE}'}  (Ctrl+C 結束)")

    gate = MouthGate(args.gain, args.quiet)
    env = 0.0
    last_scan = 0.0
    try:
        while True:
            now = time.time()
            # 沒抓到目標 session 時掃快一點,抓到後放慢
            interval = 0.3 if not meter.matched() else args.rescan_sec
            if now - last_scan >= interval:
                meter.rescan()
                last_scan = now
            peak, dead = meter.peak()
            if dead:
                last_scan = 0.0                          # 有 session 失效 → 下圈立即重掃
            env = peak if (args.no_smooth or peak > env) else env * RELEASE  # 快起慢放
            gate.feed(env * args.gain)
            time.sleep(FRAME_SEC)
    except KeyboardInterrupt:
        print("\n[mouth] 結束,送 idle 收尾。")
    finally:
        gate.close()
        meter.close()


# ── whole-system 模式 (退路,舊行為) ────────────────────────────────────────────────────────────
def run_whole_system(args):
    try:
        import numpy as np
        import soundcard as sc
    except ImportError:
        sys.exit("--whole-system 需: pip install soundcard numpy")

    try:
        spk = sc.default_speaker()
        loop_mic = sc.get_microphone(id=str(spk.name), include_loopback=True)
    except Exception as e:
        sys.exit(f"找不到 loopback 裝置: {e}")

    samplerate = 48000
    chunk = int(samplerate * FRAME_SEC)
    print(f"[mouth] whole-system 模式 — 監聽喇叭「{spk.name}」→ 嘴型 {MOUTH_HOST}:{MOUTH_PORT}")
    print(f"[mouth] GAIN={args.gain}  ⚠ 整台系統聲音都會驅動嘴型  (Ctrl+C 結束)")

    gate = MouthGate(args.gain, args.quiet)
    try:
        with loop_mic.recorder(samplerate=samplerate) as rec:
            while True:
                data = rec.record(numframes=chunk)
                rms = float(np.sqrt(np.mean(np.square(data)))) if data.size else 0.0
                gate.feed(rms * args.gain)
    except KeyboardInterrupt:
        print("\n[mouth] 結束,送 idle 收尾。")
    finally:
        gate.close()


def main():
    ap = argparse.ArgumentParser(description="Path B 擷取端 → 桌寵 V2 真嘴型")
    ap.add_argument("--process", default="hermes", help="per-process:盯住行程名含此字串者 (預設 %(default)s)")
    ap.add_argument("--pid", type=int, default=None, help="per-process:改用指定 PID (含子孫行程)")
    ap.add_argument("--whole-system", action="store_true", help="改用舊的整機 loopback (需 soundcard/numpy)")
    ap.add_argument("--gain", type=float, default=None, help="0~1 增益 (預設:per-process 2.5 / whole-system 8.0)")
    ap.add_argument("--rescan-sec", type=float, default=1.0, help="per-process 重掃 session 間隔 (預設 %(default)s)")
    ap.add_argument("--no-smooth", action="store_true", help="per-process:關閉包絡平滑 (直接用原始峰值)")
    ap.add_argument("--quiet", action="store_true", help="不印即時 amp/狀態")
    args = ap.parse_args()

    if args.gain is None:
        args.gain = GAIN_WHOLE if args.whole_system else GAIN_PROCESS

    if args.whole_system:
        run_whole_system(args)
    else:
        run_per_process(args)


if __name__ == "__main__":
    main()
