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
import ctypes
import ctypes.wintypes as wt
import json
import os
import socket
import subprocess
import sys
import time

# ---- 埠 / Ports (與桌寵一致；只用事件埠送 listening 視覺提示) --------------------------------
EVENT_HOST, EVENT_PORT = "127.0.0.1", 39217   # VoiceBridge (HTTP/TCP)

# ---- 預設 / Defaults --------------------------------------------------------------------------
DEFAULT_MODEL     = os.path.join(os.path.dirname(__file__), "models", "vosk-small-cn")
DEFAULT_KEYWORD   = "八千代"
DEFAULT_COOLDOWN  = 6.0        # 命中後暫停偵測秒數 (防連續誤觸、避免自我喚醒) | cooldown after a hit
SAMPLERATE        = 16000      # Vosk 模型取樣率 | Vosk expects 16kHz
BLOCKSIZE         = 8000       # 每次讀取樣本數 (~0.5s) | frames per read


# ── Windows 送鍵 / 視窗 (純 ctypes，無額外套件) ────────────────────────────────────────────────
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

VK_CONTROL, VK_B = 0x11, 0x42
KEYEVENTF_KEYUP  = 0x0002
INPUT_KEYBOARD   = 1
SW_RESTORE       = 9
MAPVK_VK_TO_VSC  = 0


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(wt.ULONG))]


class _INPUTunion(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wt.DWORD), ("u", _INPUTunion)]


def _key(vk: int, up: bool = False):
    # 帶掃描碼：終端 TUI (Ink 讀 stdin) 需要 scan code 才能把合成鍵翻成控制字元(Ctrl+B=0x02)；
    # 只給 wVk、wScan=0 常被終端翻譯失敗 → 等於沒收到。同時給 wVk+wScan 相容性最好。
    scan = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    flags = KEYEVENTF_KEYUP if up else 0
    return INPUT(type=INPUT_KEYBOARD, u=_INPUTunion(ki=KEYBDINPUT(vk, scan, flags, 0, None)))


# ── conhost 直寫輸入緩衝 / WriteConsoleInput (最硬，跳過焦點與訊息翻譯，僅傳統主控台可用) ─────────
KEY_EVENT          = 0x0001
LEFT_CTRL_PRESSED  = 0x0008
STD_INPUT_HANDLE   = -10
GENERIC_RW         = 0x80000000 | 0x40000000
FILE_SHARE_RW      = 0x00000001 | 0x00000002
OPEN_EXISTING      = 3
DETACHED_PROCESS   = 0x00000008           # 子行程完全不帶 console → AttachConsole 才不會被擋
TH32CS_SNAPPROCESS = 0x00000002
INVALID_HANDLE     = ctypes.c_void_p(-1).value


class KEY_EVENT_RECORD(ctypes.Structure):
    _fields_ = [("bKeyDown", wt.BOOL), ("wRepeatCount", wt.WORD),
                ("wVirtualKeyCode", wt.WORD), ("wVirtualScanCode", wt.WORD),
                ("UnicodeChar", wt.WCHAR), ("dwControlKeyState", wt.DWORD)]


class INPUT_RECORD(ctypes.Structure):
    _fields_ = [("EventType", wt.WORD), ("KeyEvent", KEY_EVENT_RECORD)]


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(wt.ULONG)), ("th32ModuleID", wt.DWORD),
                ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD),
                ("pcPriClassBase", wt.LONG), ("dwFlags", wt.DWORD), ("szExeFile", ctypes.c_char * 260)]


# 64-bit 下 HANDLE/指標必須設 argtypes/restype，否則預設 c_int 會把回傳/傳入的 handle 截成 32-bit → 失效。
_LPDWORD = ctypes.POINTER(wt.DWORD)
kernel32.GetConsoleWindow.restype = wt.HWND
kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
kernel32.Process32First.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
kernel32.Process32Next.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.AttachConsole.argtypes = [wt.DWORD]
kernel32.CreateFileW.restype = wt.HANDLE
kernel32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p,
                                 wt.DWORD, wt.DWORD, wt.HANDLE]
kernel32.WriteConsoleInputW.argtypes = [wt.HANDLE, ctypes.POINTER(INPUT_RECORD), wt.DWORD, _LPDWORD]
kernel32.WriteConsoleInputW.restype = wt.BOOL


def _rec(down: bool, vk: int, ch: str, ctrl: int) -> INPUT_RECORD:
    r = INPUT_RECORD()
    r.EventType = KEY_EVENT
    r.KeyEvent.bKeyDown = 1 if down else 0
    r.KeyEvent.wRepeatCount = 1
    r.KeyEvent.wVirtualKeyCode = vk
    r.KeyEvent.wVirtualScanCode = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    r.KeyEvent.UnicodeChar = ch
    r.KeyEvent.dwControlKeyState = ctrl
    return r


def descendant_pids(root: int, max_depth: int = 4) -> list:
    """回傳 root 底下各層子孫行程 PID (conhost→powershell→hermes)。單次快照建 parent→children 再 BFS。"""
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if not snap or snap == INVALID_HANDLE:
        return []
    kids = {}
    try:
        e = PROCESSENTRY32()
        e.dwSize = ctypes.sizeof(PROCESSENTRY32)
        ok = kernel32.Process32First(snap, ctypes.byref(e))
        while ok:
            kids.setdefault(e.th32ParentProcessID, []).append(e.th32ProcessID)
            ok = kernel32.Process32Next(snap, ctypes.byref(e))
    finally:
        kernel32.CloseHandle(snap)

    out, frontier, seen, depth = [], [root], {root}, 0
    while frontier and depth < max_depth:
        nxt = []
        for p in frontier:
            for c in kids.get(p, []):
                if c not in seen:
                    seen.add(c)
                    out.append(c)
                    nxt.append(c)
        frontier, depth = nxt, depth + 1
    return out


def inject_conhost(win_pid: int) -> bool:
    """AttachConsole + WriteConsoleInput 直送 Ctrl+B。務必在「無自有 console」的子行程(DETACHED_PROCESS)執行。

    AttachConsole 要的是「共用該主控台的某個行程」PID；視窗擁有者常是 conhost 本身，未必能 attach，
    故候選 = 視窗 PID + 其子孫(真正的 shell/hermes)，逐一嘗試直到 attach 成功。
    """
    kernel32.FreeConsole()  # 保險：若本行程仍附著任何 console，先卸掉才能 AttachConsole
    attached = False
    for pid in [win_pid] + descendant_pids(win_pid):
        if kernel32.AttachConsole(pid):
            attached = True
            break
    if not attached:
        return False
    h = None
    try:
        h = kernel32.CreateFileW("CONIN$", GENERIC_RW, FILE_SHARE_RW, None, OPEN_EXISTING, 0, None)
        if not h or h == INVALID_HANDLE:
            return False
        recs = (INPUT_RECORD * 4)(
            _rec(True,  VK_CONTROL, "\x00", LEFT_CTRL_PRESSED),
            _rec(True,  VK_B,       "\x02", LEFT_CTRL_PRESSED),
            _rec(False, VK_B,       "\x02", LEFT_CTRL_PRESSED),
            _rec(False, VK_CONTROL, "\x00", 0),
        )
        written = wt.DWORD(0)
        ok = kernel32.WriteConsoleInputW(h, recs, 4, ctypes.byref(written))
        return bool(ok) and written.value == 4
    finally:
        if h and h != INVALID_HANDLE:
            kernel32.CloseHandle(h)
        kernel32.FreeConsole()


def find_window(title_substr: str, class_name: str = None):
    """回傳第一個符合條件的可見頂層視窗 HWND；找不到回 None。

    title_substr: 標題含此片段 (空字串=不比標題)；class_name: 視窗類別需相等 (None=不比類別)。
    會略過本程式自己的主控台視窗，避免把 Ctrl+B 送給自己。
    """
    target = (title_substr or "").lower()
    self_hwnd = kernel32.GetConsoleWindow()
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def _cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd) or hwnd == self_hwnd:
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        if target and target not in buf.value.lower():
            return True
        if class_name and win_class(hwnd) != class_name:
            return True
        if not target and not class_name:
            return True
        found.append(hwnd)
        return False

    user32.EnumWindows(_cb, 0)
    return found[0] if found else None


def list_windows():
    """列出目前所有有標題的可見視窗 (含類別)，幫使用者挑 --window-title / --window-class。"""
    rows = []
    self_hwnd = kernel32.GetConsoleWindow()

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def _cb(hwnd, _):
        if user32.IsWindowVisible(hwnd) and hwnd != self_hwnd:
            n = user32.GetWindowTextLengthW(hwnd)
            if n > 0:
                buf = ctypes.create_unicode_buffer(n + 1)
                user32.GetWindowTextW(hwnd, buf, n + 1)
                if buf.value.strip():
                    rows.append((buf.value, win_class(hwnd)))
        return True

    user32.EnumWindows(_cb, 0)
    print("目前可見視窗 (挑一個當 --window-title；conhost 類別為 ConsoleWindowClass)：")
    for t, c in rows:
        print(f"  - {t}    [{c}]")


def win_title(hwnd) -> str:
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value


def win_class(hwnd) -> str:
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buf, 256)
    return buf.value


def force_foreground(hwnd) -> bool:
    """比單純 SetForegroundWindow 可靠地把鍵盤焦點交給 hwnd。

    Windows 前景鎖常讓 SetForegroundWindow「回 OK 但焦點沒真的過去」。標準解法是暫時把本執行緒
    的輸入佇列附掛(AttachThreadInput)到目前前景視窗與目標視窗的執行緒，再 SetForegroundWindow+SetFocus。
    """
    user32.ShowWindow(hwnd, SW_RESTORE)
    fg = user32.GetForegroundWindow()
    cur = kernel32.GetCurrentThreadId()
    fg_tid = user32.GetWindowThreadProcessId(fg, None) if fg else 0
    tgt_tid = user32.GetWindowThreadProcessId(hwnd, None)
    attached = [t for t in {fg_tid, tgt_tid} if t and t != cur]
    for t in attached:
        user32.AttachThreadInput(cur, t, True)
    user32.BringWindowToTop(hwnd)
    ok = user32.SetForegroundWindow(hwnd)
    user32.SetFocus(hwnd)
    for t in attached:
        user32.AttachThreadInput(cur, t, False)
    return bool(ok)


def send_ctrl_b(hwnd, restore_focus: bool = True):
    """把 Hermes 視窗帶到前景(可靠奪焦)並送 Ctrl+B；回傳 (ok, 診斷字串)。"""
    prev = user32.GetForegroundWindow()
    ok = force_foreground(hwnd)
    time.sleep(0.18)  # 給焦點切換一點時間 | let focus settle

    # 命中當下實際被聚焦的視窗——用來判斷焦點是否真的落在 Hermes、以及它是哪種終端。
    fg = user32.GetForegroundWindow()
    diag = f"焦點→「{win_title(fg)}」[{win_class(fg)}]  目標[{win_class(hwnd)}]"

    # 分兩批送 (按下組合、放開組合)，中間留空檔，比一次四鍵更像真實按壓、對挑剔的 TUI 較穩。
    down = (INPUT * 2)(_key(VK_CONTROL), _key(VK_B))
    up = (INPUT * 2)(_key(VK_B, up=True), _key(VK_CONTROL, up=True))
    user32.SendInput(2, ctypes.byref(down), ctypes.sizeof(INPUT))
    time.sleep(0.03)
    user32.SendInput(2, ctypes.byref(up), ctypes.sizeof(INPUT))

    if restore_focus and prev and prev != hwnd:
        time.sleep(0.05)
        user32.SetForegroundWindow(prev)
    return bool(ok), diag


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
    ap.add_argument("--_inject-pid", type=int, default=None, help=argparse.SUPPRESS)  # 內部：子行程直寫用
    args = ap.parse_args()

    # 內部子行程模式：AttachConsole 需「無自有 console」，故直寫由本檔以 DETACHED_PROCESS 另起子行程執行。
    if args._inject_pid is not None:
        sys.exit(0 if inject_conhost(args._inject_pid) else 1)

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
                            pid = wt.DWORD(0)
                            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                            r = subprocess.run([sys.executable, os.path.abspath(__file__),
                                                "--_inject-pid", str(pid.value)],
                                               creationflags=DETACHED_PROCESS)
                            print(f"[wakeword]    直寫緩衝 WriteConsoleInput ({'OK' if r.returncode == 0 else '失敗，可試 --inject sendinput'})")
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
