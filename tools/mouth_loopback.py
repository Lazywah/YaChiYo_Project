#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Path B 擷取端 — 監聽喇叭輸出，驅動桌寵 V2 真嘴型 (不改 Hermes)。
Path B capture — listens to the speaker output and drives the pet's V2 real mouth (no Hermes changes).

原理 / How it works:
  用 WASAPI loopback 錄「喇叭正在播的聲音」→ 每 ~30ms 算一次 RMS 音量。因為完全不改 Hermes，
  Hermes 不會主動送事件，所以這支腳本一手包辦兩件事：
    1. 偵測到有聲音 → 對事件埠 (39217) 送 speaking (開嘴型閘門)；持續靜音 → 送 idle (關閘門)。
    2. 說話期間 → 把 0~1 的音量用 UDP (1 byte) 連續送到嘴型埠 (39218)。
  桌寵側 (MouthStream + Live2DWidget::setMouthLevel) 已就緒，不需再改。

  This captures whole system output. If other audio plays (music/notifications) it will also move the
  mouth. Mitigations: the START/SILENCE thresholds below, and only run it while chatting with Hermes.
  A future refinement is per-process (Hermes-only) WASAPI loopback.

需求 / Requirements:
    pip install soundcard numpy

用法 / Usage:
    1) 先讓桌寵以 Live2D + 語音跑起來：YaChiYo_Project.exe --live2d --voice
    2) python tools/mouth_loopback.py
    3) 讓 Hermes 唸一段話 → 桌寵嘴巴應跟著音量起伏；停下來嘴巴閉起來。
  終端會即時印出 amp 與狀態，方便調 GAIN (太小=嘴開不夠、太大=一直爆開)。
"""

import argparse
import json
import math
import socket
import sys
import time

# ---- 埠 / Ports (需與桌寵一致；嘴型埠 = 事件埠 + 1) --------------------------------------------
EVENT_HOST, EVENT_PORT = "127.0.0.1", 39217   # VoiceBridge (HTTP/TCP) — speaking/idle 事件
MOUTH_HOST, MOUTH_PORT = "127.0.0.1", 39218   # MouthStream  (UDP)      — 振幅串流

# ---- 可調參數 / Tunables ---------------------------------------------------------------------
SAMPLERATE        = 48000     # 錄音取樣率 | capture sample rate
CHUNK_SEC         = 0.03      # 每塊長度 (30ms ≈ 33Hz) | chunk length
GAIN              = 8.0       # RMS→0~1 映射增益 (嘴開不夠就調大) | RMS→0~1 gain
START_AMP         = 0.08      # 升到此值以上 → 開始說話 (遲滯上緣) | rise above → start speaking (hysteresis high)
SILENCE_AMP       = 0.04      # 低於此值 → 視為靜音 (遲滯下緣) | below → treated as silence (hysteresis low)
SILENCE_HANG_SEC  = 0.5       # 靜音持續多久 → 送 idle 關閘門 | silence this long → idle
SPEAK_REARM_SEC   = 2.0       # 說話中每隔多久重送 speaking (續命桌寵逾時保險) | re-arm speaking watchdog
SPEAK_DURATION    = 3.0       # 每次 speaking 事件帶的時長 (逾時保險視窗) | duration_sec per speaking event


def post_event(obj: dict):
    """對 VoiceBridge 送事件 (極簡 HTTP POST /pet/event)；失敗不致命 (桌寵可能沒開)。"""
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


def main():
    global GAIN
    parser = argparse.ArgumentParser(description="Path B loopback → 桌寵 V2 真嘴型")
    parser.add_argument("--gain", type=float, default=GAIN, help="RMS→0~1 增益 (預設 %(default)s)")
    parser.add_argument("--quiet", action="store_true", help="不印即時 amp/狀態")
    args = parser.parse_args()
    GAIN = args.gain

    try:
        import numpy as np
        import soundcard as sc
    except ImportError:
        sys.exit("缺少套件，請先: pip install soundcard numpy")

    # ZH: 取「預設喇叭」的 loopback 麥克風 (錄的是喇叭輸出) | EN: loopback mic of the default speaker
    try:
        spk = sc.default_speaker()
        loop_mic = sc.get_microphone(id=str(spk.name), include_loopback=True)
    except Exception as e:
        sys.exit(f"找不到 loopback 裝置: {e}\n(確認有預設喇叭；或改用 pyaudiowpatch)")

    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    chunk = int(SAMPLERATE * CHUNK_SEC)

    print(f"[loopback] 監聽喇叭「{spk.name}」→ 嘴型 {MOUTH_HOST}:{MOUTH_PORT} / 事件 {EVENT_HOST}:{EVENT_PORT}")
    print(f"[loopback] GAIN={GAIN}  START={START_AMP}  SILENCE={SILENCE_AMP}  (Ctrl+C 結束)")

    speaking = False
    last_loud = 0.0
    last_rearm = 0.0

    try:
        with loop_mic.recorder(samplerate=SAMPLERATE) as rec:
            while True:
                data = rec.record(numframes=chunk)          # (frames, channels) float
                rms = float(np.sqrt(np.mean(np.square(data)))) if data.size else 0.0
                amp = max(0.0, min(1.0, rms * GAIN))
                now = time.time()

                if not speaking:
                    # 靜默中 — 音量升過上緣才開始說話 (開閘門)
                    if amp >= START_AMP:
                        speaking = True
                        last_loud = now
                        last_rearm = now
                        post_event({"event": "speaking", "duration_sec": SPEAK_DURATION})
                else:
                    # 說話中 — 持續送振幅；追蹤最後一次「夠大聲」的時間
                    byte = max(0, min(255, int(amp * 255)))
                    udp.sendto(bytes([byte]), (MOUTH_HOST, MOUTH_PORT))
                    if amp >= SILENCE_AMP:
                        last_loud = now
                    # 續命逾時保險 (重送 speaking)
                    if now - last_rearm >= SPEAK_REARM_SEC:
                        last_rearm = now
                        post_event({"event": "speaking", "duration_sec": SPEAK_DURATION})
                    # 靜音夠久 → 收尾關閘門
                    if now - last_loud >= SILENCE_HANG_SEC:
                        speaking = False
                        post_event({"event": "idle"})

                if not args.quiet:
                    bar = "#" * int(amp * 30)
                    state = "SPEAK" if speaking else "  -  "
                    print(f"\r[{state}] amp={amp:4.2f} |{bar:<30}|", end="", flush=True)
    except KeyboardInterrupt:
        print("\n[loopback] 結束，送 idle 收尾。")
        post_event({"event": "idle"})
    finally:
        udp.close()


if __name__ == "__main__":
    main()
