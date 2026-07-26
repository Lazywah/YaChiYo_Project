#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Hermes 注入器 — 共用送鍵核心 (conhost WriteConsoleInput / SendInput)。
Shared key-injection core for talking to a Hermes TUI running in a classic console (conhost).

被 wakeword.py 匯入 (喚醒詞命中→送 Ctrl+B)；也給桌寵 V3 用 (雙擊→Ctrl+B、聊天室打字→type-text)。
可獨立 CLI 使用 (Windows only)：
    python tools/hermes_inject.py ctrl-b     [--window-title hermes] [--window-class ConsoleWindowClass]
    python tools/hermes_inject.py type-text "你好八千代"  [--no-submit] [--window-class ConsoleWindowClass]
    python tools/hermes_inject.py list-windows

為何 conhost 直寫 (WriteConsoleInput)：跳過焦點與訊息翻譯、不搶焦點 (畫面不閃)，且對挑剔的終端 TUI 最穩。
    ⚠ 只對「傳統主控台 conhost」(類別 ConsoleWindowClass) 有效；Windows Terminal 對合成輸入免疫。
AttachConsole 需要「無自有 console」的行程，故實際直寫一律由本檔以 DETACHED_PROCESS 另起子行程執行
    (見 _detached_inject / --_do 隱藏分支)。此雷已於 V1 踩完，勿改。
"""

import argparse
import base64
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

# ── Windows 送鍵 / 視窗 (純 ctypes，無額外套件) ────────────────────────────────────────────────
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

VK_CONTROL, VK_B, VK_RETURN = 0x11, 0x42, 0x0D
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


def _write_records(recs: list) -> bool:
    """AttachConsole 後把一串 INPUT_RECORD 灌進 CONIN$。呼叫端須已 AttachConsole 成功。"""
    h = None
    try:
        h = kernel32.CreateFileW("CONIN$", GENERIC_RW, FILE_SHARE_RW, None, OPEN_EXISTING, 0, None)
        if not h or h == INVALID_HANDLE:
            return False
        n = len(recs)
        arr = (INPUT_RECORD * n)(*recs)
        written = wt.DWORD(0)
        ok = kernel32.WriteConsoleInputW(h, arr, n, ctypes.byref(written))
        return bool(ok) and written.value == n
    finally:
        if h and h != INVALID_HANDLE:
            kernel32.CloseHandle(h)


def _attach_any(win_pid: int) -> bool:
    """FreeConsole 後嘗試 attach 到視窗 PID 或其子孫 (真正持有 console 的常是子孫 shell/hermes)。"""
    kernel32.FreeConsole()  # 保險：若本行程仍附著任何 console，先卸掉才能 AttachConsole
    for pid in [win_pid] + descendant_pids(win_pid):
        if kernel32.AttachConsole(pid):
            return True
    return False


def inject_conhost(win_pid: int) -> bool:
    """AttachConsole + WriteConsoleInput 直送 Ctrl+B。務必在「無自有 console」的子行程(DETACHED_PROCESS)執行。"""
    if not _attach_any(win_pid):
        return False
    try:
        return _write_records([
            _rec(True,  VK_CONTROL, "\x00", LEFT_CTRL_PRESSED),
            _rec(True,  VK_B,       "\x02", LEFT_CTRL_PRESSED),
            _rec(False, VK_B,       "\x02", LEFT_CTRL_PRESSED),
            _rec(False, VK_CONTROL, "\x00", 0),
        ])
    finally:
        kernel32.FreeConsole()


def inject_text_conhost(win_pid: int, text: str, submit: bool = True) -> bool:
    """AttachConsole + WriteConsoleInput 灌入整串 Unicode 文字 (中文 OK)，可選補 Enter 送出。
    務必在「無自有 console」的子行程(DETACHED_PROCESS)執行。逐字元 down/up；換行用 VK_RETURN。"""
    if not _attach_any(win_pid):
        return False
    try:
        recs = []
        for ch in text:
            if ch in ("\r", "\n"):
                continue  # 內文換行不送 (避免提前送出)；真正送出交給結尾 Enter
            recs.append(_rec(True,  0, ch, 0))   # UnicodeChar 直帶字元，vk=0
            recs.append(_rec(False, 0, ch, 0))
        if submit:
            recs.append(_rec(True,  VK_RETURN, "\r", 0))
            recs.append(_rec(False, VK_RETURN, "\r", 0))
        return _write_records(recs) if recs else True
    finally:
        kernel32.FreeConsole()


# ── 視窗尋找 / 資訊 ─────────────────────────────────────────────────────────────────────────────
def win_title(hwnd) -> str:
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value


def win_class(hwnd) -> str:
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buf, 256)
    return buf.value


def window_pid(hwnd) -> int:
    pid = wt.DWORD(0)
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    return pid.value


def find_window(title_substr: str, class_name: str = None):
    """回傳第一個符合條件的可見頂層視窗 HWND；找不到回 None。

    title_substr: 標題含此片段 (空字串=不比標題)；class_name: 視窗類別需相等 (None=不比類別)。
    會略過本程式自己的主控台視窗，避免把鍵送給自己。
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


def force_foreground(hwnd) -> bool:
    """比單純 SetForegroundWindow 可靠地把鍵盤焦點交給 hwnd (AttachThreadInput 破前景鎖)。"""
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
    """SendInput 版：把視窗帶到前景並送 Ctrl+B (非 conhost 情境備援)；回傳 (ok, 診斷字串)。"""
    prev = user32.GetForegroundWindow()
    ok = force_foreground(hwnd)
    time.sleep(0.18)  # 給焦點切換一點時間 | let focus settle
    fg = user32.GetForegroundWindow()
    diag = f"焦點→「{win_title(fg)}」[{win_class(fg)}]  目標[{win_class(hwnd)}]"
    down = (INPUT * 2)(_key(VK_CONTROL), _key(VK_B))
    up = (INPUT * 2)(_key(VK_B, up=True), _key(VK_CONTROL, up=True))
    user32.SendInput(2, ctypes.byref(down), ctypes.sizeof(INPUT))
    time.sleep(0.03)
    user32.SendInput(2, ctypes.byref(up), ctypes.sizeof(INPUT))
    if restore_focus and prev and prev != hwnd:
        time.sleep(0.05)
        user32.SetForegroundWindow(prev)
    return bool(ok), diag


# ── 高階觸發 (供 wakeword / 桌寵呼叫)：一律以 DETACHED_PROCESS 子行程做實際直寫 ────────────────────
def _detached_inject(*args) -> int:
    """以 DETACHED_PROCESS 另起本檔子行程執行注入 (AttachConsole 需無自有 console)。回傳 returncode。"""
    r = subprocess.run([sys.executable, os.path.abspath(__file__), *args],
                       creationflags=DETACHED_PROCESS)
    return r.returncode


def trigger_ctrl_b_pid(win_pid: int) -> bool:
    """對指定視窗 PID 的 conhost 送 Ctrl+B (開錄音)。成功回 True。"""
    return _detached_inject("--_do", "ctrl-b", "--_pid", str(win_pid)) == 0


def trigger_text_pid(win_pid: int, text: str, submit: bool = True) -> bool:
    """對指定視窗 PID 的 conhost 灌入文字 (可選送出)。成功回 True。"""
    b64 = base64.b64encode(text.encode("utf-8")).decode("ascii")
    a = ["--_do", "type-text", "--_pid", str(win_pid), "--_b64", b64]
    if not submit:
        a.append("--_no-submit")
    return _detached_inject(*a) == 0


def _resolve_pid(wtitle, wclass):
    """解析目標視窗 → (pid, hwnd, 說明)；找不到回 (None, None, 說明)。"""
    # 未給 title 時：有 class 就不比標題 (空)，否則沿用預設 hermes。
    if wtitle is None:
        wtitle = "" if wclass else "hermes"
    hwnd = find_window(wtitle, wclass)
    if not hwnd:
        return None, None, "找不到目標視窗 (先開好 Hermes，或用 list-windows 挑名稱/類別)"
    return window_pid(hwnd), hwnd, f"「{win_title(hwnd)}」[{win_class(hwnd)}]"


# ── CLI ─────────────────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description="Hermes 注入器 (conhost WriteConsoleInput)")
    # cmd 可選：內部 detached 子行程走 --_do 分支、不帶位置參數 (見下)；一般 CLI 才需要 cmd。
    ap.add_argument("cmd", nargs="?", choices=["ctrl-b", "type-text", "list-windows"], default=None, help="動作")
    ap.add_argument("text", nargs="?", default="", help="type-text 的文字")
    ap.add_argument("--window-title", default=None, help="視窗標題片段 (未給且有 --window-class=不比標題)")
    ap.add_argument("--window-class", default=None, help="視窗類別精準鎖定 (conhost=ConsoleWindowClass)")
    ap.add_argument("--no-submit", action="store_true", help="type-text 不補 Enter 送出 (只填字)")
    # 內部：由 _detached_inject 以 DETACHED_PROCESS 子行程呼叫，真正做 AttachConsole+直寫
    ap.add_argument("--_do", choices=["ctrl-b", "type-text"], default=None, help=argparse.SUPPRESS)
    ap.add_argument("--_pid", type=int, default=None, help=argparse.SUPPRESS)
    ap.add_argument("--_b64", default="", help=argparse.SUPPRESS)
    ap.add_argument("--_no-submit", dest="_no_submit", action="store_true", help=argparse.SUPPRESS)
    args = ap.parse_args()

    # 子行程分支：無自有 console，直接 AttachConsole + 直寫。
    if args._do is not None:
        if args._do == "ctrl-b":
            sys.exit(0 if inject_conhost(args._pid) else 1)
        else:
            txt = base64.b64decode(args._b64.encode("ascii")).decode("utf-8") if args._b64 else ""
            sys.exit(0 if inject_text_conhost(args._pid, txt, submit=not args._no_submit) else 1)

    if args.cmd is None:
        ap.error("需要動作: ctrl-b / type-text / list-windows")

    if args.cmd == "list-windows":
        list_windows()
        return

    pid, hwnd, desc = _resolve_pid(args.window_title, args.window_class)
    if pid is None:
        print(f"[inject] ⚠ {desc}")
        sys.exit(2)

    if args.cmd == "ctrl-b":
        ok = trigger_ctrl_b_pid(pid)
        print(f"[inject] ctrl-b → {desc} ({'OK' if ok else '失敗'})")
        sys.exit(0 if ok else 1)
    else:
        if not args.text:
            print("[inject] type-text 需要文字：hermes_inject.py type-text \"你好\"")
            sys.exit(2)
        ok = trigger_text_pid(pid, args.text, submit=not args.no_submit)
        print(f"[inject] type-text({len(args.text)}字) → {desc} ({'OK' if ok else '失敗'})")
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
