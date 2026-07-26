#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
喚醒詞層 (方案 A) — 常駐聽麥克風，偵測「八千代」→ 送 Ctrl+B 給 Hermes 開始錄音。
Wake-word layer (Plan A): an always-on mic listener. When it hears the wake phrase it
brings the Hermes TUI window to the foreground and sends Ctrl+B, so Hermes starts its
own VAD-bounded recording. This script does ONE job: spot the wake word.

架構定位 / Where this sits (與 mouth_loopback.py 對稱的另一支擷取端):
  Vosk(此腳本) 只負責「聽到喚醒詞」。聽到後送一次 Ctrl+B，剩下的斷句 / STT / 回覆 / TTS
  全交給 Hermes 現成的 VAD continuous 迴圈；桌寵嘴型與事件仍走既有 VoiceBridge / MouthStream。

  ⚠️ 轉接有 ~0.5-1s 空檔 (Vosk 偵測 + 送鍵 + Hermes 開麥)。所以互動節奏是：
        「八千代」 →（Hermes 嗶一聲 880Hz 開耳朵）→ 才講你的指令
  一口氣「八千代幫我開 VS Code」連句，指令開頭會落在空檔而遺失 —— 這是方案 A 的已知取捨。

需求 / Requirements:
    pip install vosk sounddevice
    中文小模型 vosk-model-small-cn-0.22 (~42MB, 動態圖可用 grammar)：
      https://alphacephei.com/vosk/models
    解壓到 (預設) tools/models/vosk-small-cn/  或用 --model 指定。

用法 / Usage:
    1) Hermes TUI 跑起來並在裡面 /voice on
    2) (可選) 桌寵 --live2d --voice 跑著，純為 listening 視覺提示
    3) python tools/wakeword.py --window-title hermes
  不知道 Hermes 視窗叫什麼？先跑： python tools/wakeword.py --list-windows
  Windows only（送鍵與視窗聚焦走 user32）。
"""

import argparse
import json
import os
import socket
import sys
import time

# 送鍵 / 視窗核心已抽到共用模組 hermes_inject (桌寵 V3 也用同一套)。此檔只負責「聽到喚醒詞」。
from hermes_inject import (
    find_window, list_windows, win_class, win_title,
    window_pid, send_ctrl_b, trigger_ctrl_b_pid,
)

# ---- 埠 / Ports (與桌寵一致；只用事件埠送 listening 視覺提示) --------------------------------
EVENT_HOST, EVENT_PORT = "127.0.0.1", 39217   # VoiceBridge (HTTP/TCP)

# ---- 預設 / Defaults --------------------------------------------------------------------------
DEFAULT_MODEL     = os.path.join(os.path.dirname(__file__), "models", "vosk-small-cn")
DEFAULT_KEYWORD   = "八千代"
DEFAULT_COOLDOWN  = 6.0        # 命中後暫停偵測秒數 (防連續誤觸、避免自我喚醒) | cooldown after a hit
SAMPLERATE        = 16000      # Vosk 模型取樣率 | Vosk expects 16kHz
BLOCKSIZE         = 8000       # 每次讀取樣本數 (~0.5s) | frames per read


# ── 桌寵事件 (視覺提示，可關) ──────────────────────────────────────────────────────────────────
def post_event(obj: dict):
    """對 VoiceBridge 送事件 (極簡 HTTP)；失敗不致命 (桌寵可能沒開)。"""
    body = json.dumps(obj).encode("utf-8")
    req = (b"POST /pet/event HTTP/1.1\r\nHost: 127.0.0.1\r\n"
           b"Content-Type: application/json\r\n"
           b"Content-Length: " + str(len(body)).encode() +
           b"\r\nConnection: close\r\n\r\n" + body)
    try:
        with socket.create_connection((EVENT_HOST, EVENT_PORT), timeout=1.0) as s:
            s.sendall(req)
            try:
                s.recv(64)
            except socket.timeout:
                pass
    except OSError:
        pass


def main():
    ap = argparse.ArgumentParser(description="喚醒詞層 (方案 A): 聽到「八千代」→ 送 Ctrl+B 給 Hermes")
    ap.add_argument("--model", default=DEFAULT_MODEL, help="Vosk 中文模型資料夾 (預設 %(default)s)")
    ap.add_argument("--keyword", default=DEFAULT_KEYWORD, help="喚醒詞 (預設 %(default)s)")
    ap.add_argument("--window-title", default=None, help="Hermes 視窗標題片段 (未給且有 --window-class 時=不比標題)")
    ap.add_argument("--window-class", default=None, help="用視窗類別精準鎖定 (conhost=ConsoleWindowClass)")
    ap.add_argument("--cooldown", type=float, default=DEFAULT_COOLDOWN, help="命中後暫停偵測秒數 (預設 %(default)s)")
    ap.add_argument("--device", type=int, default=None, help="輸入裝置索引 (預設系統預設麥克風)")
    ap.add_argument("--no-grammar", action="store_true", help="不限定關鍵字 grammar，改全辨識+含詞比對 (誤觸較多，除錯用)")
    ap.add_argument("--inject", choices=["auto", "conhost", "sendinput"], default="auto",
                    help="送鍵方式：auto=傳統主控台用直寫緩衝、其餘用 SendInput (預設)；可強制指定")
    ap.add_argument("--no-restore-focus", action="store_true", help="SendInput 模式送鍵後不還原原本前景視窗")
    ap.add_argument("--no-pet-event", action="store_true", help="不對桌寵送 listening 視覺提示")
    ap.add_argument("--list-windows", action="store_true", help="列出目前可見視窗後結束")
    ap.add_argument("--quiet", action="store_true", help="不印即時狀態")
    args = ap.parse_args()

    if args.list_windows:
        list_windows()
        return

    # 解析有效標題：未給 --window-title 時，有 --window-class 就不比標題(空)、否則沿用預設 hermes。
    # (PowerShell 會把空字串引數吃掉，所以用「不給」代表 class-only，不需傳 --window-title "")
    wtitle = args.window_title
    if wtitle is None:
        wtitle = "" if args.window_class else "hermes"

    try:
        import sounddevice as sd
        from vosk import KaldiRecognizer, Model, SetLogLevel
    except ImportError:
        sys.exit("缺少套件，請先: pip install vosk sounddevice")

    if not os.path.isdir(args.model):
        sys.exit(f"找不到模型資料夾: {args.model}\n"
                 f"請到 https://alphacephei.com/vosk/models 下載 vosk-model-small-cn-0.22 解壓到此路徑，或用 --model 指定。")

    SetLogLevel(-1)  # 靜音 Vosk 內部 log
    model = Model(args.model)

    # grammar 把辨識限縮到關鍵字 → 壓低 CPU 與誤觸；[unk] 讓其他話落到未知桶。
    # 中文分詞不一定切成「八千代」整詞，故命中比對時把空白去掉再做「含詞」判斷 (見下)。
    kw_spaced = " ".join(args.keyword)  # "八千代" -> "八 千 代"
    if args.no_grammar:
        rec = KaldiRecognizer(model, SAMPLERATE)
    else:
        grammar = json.dumps([kw_spaced, "[unk]"], ensure_ascii=False)
        rec = KaldiRecognizer(model, SAMPLERATE, grammar)

    kw_bare = args.keyword.replace(" ", "")

    def hit(text: str) -> bool:
        return bool(text) and kw_bare in text.replace(" ", "")

    print(f"[wakeword] 模型「{os.path.basename(args.model.rstrip(os.sep))}」  喚醒詞「{args.keyword}」")
    tgt_desc = (f"標題含「{wtitle}」" if wtitle else "任意標題") + (f" 且類別={args.window_class}" if args.window_class else "")
    print(f"[wakeword] 目標視窗 {tgt_desc}  冷卻 {args.cooldown}s  (Ctrl+C 結束)")
    if find_window(wtitle, args.window_class) is None:
        print(f"[wakeword] ⚠ 目前找不到符合的視窗——先開好 Hermes，或用 --list-windows 挑名稱/類別。")

    cooling_until = 0.0
    try:
        with sd.RawInputStream(samplerate=SAMPLERATE, blocksize=BLOCKSIZE, device=args.device,
                               dtype="int16", channels=1) as stream:
            while True:
                data, _ = stream.read(BLOCKSIZE)
                now = time.time()
                cooling = now < cooling_until

                fired = False
                if rec.AcceptWaveform(bytes(data)):
                    text = json.loads(rec.Result()).get("text", "")
                    if not cooling and hit(text):
                        fired = True
                else:
                    # partial 讓反應快一點；仍受冷卻閘門管制
                    ptext = json.loads(rec.PartialResult()).get("partial", "")
                    if not cooling and hit(ptext):
                        fired = True

                if fired:
                    hwnd = find_window(wtitle, args.window_class)
                    if hwnd:
                        if not args.no_pet_event:
                            post_event({"event": "listening"})
                        cls = win_class(hwnd)
                        use_conhost = args.inject == "conhost" or (args.inject == "auto" and cls == "ConsoleWindowClass")
                        print(f"\n[wakeword] 🔔 命中「{args.keyword}」→ 目標「{win_title(hwnd)}」[{cls}]")
                        if use_conhost:
                            ok = trigger_ctrl_b_pid(window_pid(hwnd))
                            print(f"[wakeword]    直寫緩衝 WriteConsoleInput ({'OK' if ok else '失敗，可試 --inject sendinput'})")
                        else:
                            ok, diag = send_ctrl_b(hwnd, restore_focus=not args.no_restore_focus)
                            print(f"[wakeword]    SendInput ({'OK' if ok else '聚焦被擋?'})  {diag}")
                    else:
                        print(f"\n[wakeword] 🔔 命中但找不到目標視窗——沒送鍵。")
                    rec.Reset()
                    cooling_until = time.time() + args.cooldown

                if not args.quiet:
                    state = f"冷卻{cooling_until - now:4.1f}s" if cooling else "  聽著   "
                    print(f"\r[{state}]", end="", flush=True)
    except KeyboardInterrupt:
        print("\n[wakeword] 結束。")


if __name__ == "__main__":
    main()
