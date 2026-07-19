#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Per-process 音量表 — 用 WASAPI 音訊工作階段(Audio Session)的峰值計讀「某個行程正在輸出的音量」。
Per-process meter: reads the peak-meter of one process's audio session via WASAPI (pure ctypes).

為什麼用這個、而不是 loopback 擷取整台系統？ / Why this instead of whole-system loopback?
  嘴型只需要「音量包絡(envelope)」,不需要原始波形。Windows 每個發聲行程都有一個 audio session,
  各自有 IAudioMeterInformation::GetPeakValue() 回傳 0~1 峰值(就是音量混音器裡那條跳動的綠棒)。
  只要找到 Hermes 的 session、每秒讀 30~60 次峰值,就得到「只屬於 Hermes」的嘴型振幅——
  完全不碰其他 App 的聲音,也不需要 process-loopback 那套非同步啟動 + COM 完成回呼 + 擷取緩衝的重機制。
  全部是同步 COM 呼叫,程式短、好測、耐操。

需求 / Requirements: 無額外套件 (純 ctypes + Windows WASAPI)。Windows 10 build 19041+。
獨立測試 / Test standalone (不需要 Hermes,拿任何在播音的 App 就能驗):
    python tools/session_meter.py --list             # 即時列出所有發聲行程 + 峰值棒(播音樂看哪條在跳)
    python tools/session_meter.py --process hermes    # 只盯含 "hermes" 的行程,印即時峰值棒
    python tools/session_meter.py --pid 12345         # 盯指定 PID(含子孫行程)
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time
from ctypes import POINTER, byref, c_float, c_int, c_long, c_ulong, c_void_p

ole32 = ctypes.windll.ole32
kernel32 = ctypes.windll.kernel32

CLSCTX_ALL           = 0x17
COINIT_MULTITHREADED = 0x0
TH32CS_SNAPPROCESS   = 0x00000002
INVALID_HANDLE       = ctypes.c_void_p(-1).value


# ── COM/GUID 基礎 ──────────────────────────────────────────────────────────────────────────────
class GUID(ctypes.Structure):
    _fields_ = [("Data1", wt.DWORD), ("Data2", wt.WORD), ("Data3", wt.WORD),
                ("Data4", ctypes.c_ubyte * 8)]


# 64-bit 下指標型別務必設 argtypes/restype,否則預設 c_int 會把指標截成 32-bit → 崩潰或默默失效。
ole32.CLSIDFromString.argtypes = [wt.LPCWSTR, POINTER(GUID)]
ole32.CLSIDFromString.restype = c_long
ole32.CoInitializeEx.argtypes = [c_void_p, wt.DWORD]
ole32.CoInitializeEx.restype = c_long
ole32.CoCreateInstance.argtypes = [POINTER(GUID), c_void_p, wt.DWORD, POINTER(GUID), POINTER(c_void_p)]
ole32.CoCreateInstance.restype = c_long


def guid(s: str) -> GUID:
    g = GUID()
    if ole32.CLSIDFromString(s, byref(g)) != 0:
        raise ValueError("bad guid " + s)
    return g


CLSID_MMDeviceEnumerator   = guid("{BCDE0395-E52F-467C-8E3D-C4579291692E}")
IID_IMMDeviceEnumerator    = guid("{A95664D2-9614-4F35-A746-DE8DB63617E6}")
IID_IAudioSessionManager2  = guid("{77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F}")
IID_IAudioSessionControl2  = guid("{BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D}")
IID_IAudioMeterInformation = guid("{C02216F6-8C67-4B5B-9D00-D008E73E0064}")


def _vcall(ptr, index, restype, argtypes, *args):
    """呼叫 COM 介面 ptr 的第 index 個 vtable 方法 (this 自動當第一參數)。ptr 為 c_void_p。"""
    vtable = ctypes.cast(ptr, POINTER(POINTER(c_void_p)))[0]
    proto = ctypes.WINFUNCTYPE(restype, c_void_p, *argtypes)
    return proto(vtable[index])(ptr, *args)


def _release(ptr):
    if ptr and getattr(ptr, "value", None):
        _vcall(ptr, 2, c_ulong, [])            # IUnknown::Release (index 2)
        ptr.value = None


def _qi(ptr, iid: GUID):
    out = c_void_p()
    hr = _vcall(ptr, 0, c_long, [POINTER(GUID), POINTER(c_void_p)], byref(iid), byref(out))  # QueryInterface
    return out if hr == 0 and out.value else None


# ── 行程快照 (pid→名稱、parent→children) ───────────────────────────────────────────────────────
class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", POINTER(wt.ULONG)), ("th32ModuleID", wt.DWORD),
                ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD),
                ("pcPriClassBase", wt.LONG), ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_char * 260)]


kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
kernel32.Process32First.argtypes = [wt.HANDLE, POINTER(PROCESSENTRY32)]
kernel32.Process32Next.argtypes = [wt.HANDLE, POINTER(PROCESSENTRY32)]
kernel32.CloseHandle.argtypes = [wt.HANDLE]


def snapshot():
    """回傳 (names: pid→exe名, kids: ppid→[pid])。單次快照。"""
    names, kids = {}, {}
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if not snap or snap == INVALID_HANDLE:
        return names, kids
    try:
        e = PROCESSENTRY32()
        e.dwSize = ctypes.sizeof(PROCESSENTRY32)
        ok = kernel32.Process32First(snap, byref(e))
        while ok:
            names[e.th32ProcessID] = e.szExeFile.decode("mbcs", "ignore")
            kids.setdefault(e.th32ParentProcessID, []).append(e.th32ProcessID)
            ok = kernel32.Process32Next(snap, byref(e))
    finally:
        kernel32.CloseHandle(snap)
    return names, kids


def process_tree(root: int, kids: dict, max_depth: int = 5) -> set:
    """root 底下各層子孫 PID (含 root 自身)。"""
    out, frontier, seen, depth = {root}, [root], {root}, 0
    while frontier and depth < max_depth:
        nxt = []
        for p in frontier:
            for c in kids.get(p, []):
                if c not in seen:
                    seen.add(c)
                    out.add(c)
                    nxt.append(c)
        frontier, depth = nxt, depth + 1
    return out


# ── SessionMeter：盯住符合條件的 session,回傳最大峰值 ──────────────────────────────────────────
class SessionMeter:
    """
    match 規則: 指定 pid → 比對 {pid} ∪ 子孫; 否則 → 比對 session 行程名是否含 name_substr。
    match_all=True 用於 --list (全收)。
    """

    def __init__(self, name_substr: str = None, pid: int = None, match_all: bool = False):
        self.name_substr = (name_substr or "").lower()
        self.pid = pid
        self.match_all = match_all
        self._enum = self._device = self._mgr = None
        self._target_pids = set()
        self._meters = []          # [(spid, name, meter_ptr)]
        self._init_com()

    def _init_com(self):
        ole32.CoInitializeEx(None, COINIT_MULTITHREADED)   # S_FALSE(已初始化) 也無妨
        enum = c_void_p()
        hr = ole32.CoCreateInstance(byref(CLSID_MMDeviceEnumerator), None, CLSCTX_ALL,
                                    byref(IID_IMMDeviceEnumerator), byref(enum))
        if hr != 0 or not enum.value:
            raise OSError(f"CoCreateInstance(MMDeviceEnumerator) 失敗 hr=0x{hr & 0xffffffff:08X}")
        self._enum = enum
        device = c_void_p()      # GetDefaultAudioEndpoint(eRender=0, eConsole=0, &device) vtable#4
        hr = _vcall(enum, 4, c_long, [c_int, c_int, POINTER(c_void_p)], 0, 0, byref(device))
        if hr != 0 or not device.value:
            raise OSError(f"GetDefaultAudioEndpoint 失敗 hr=0x{hr & 0xffffffff:08X}(沒有預設喇叭?)")
        self._device = device
        mgr = c_void_p()         # IMMDevice::Activate(IID_ASM2, CLSCTX_ALL, NULL, &mgr) vtable#3
        hr = _vcall(device, 3, c_long, [POINTER(GUID), wt.DWORD, c_void_p, POINTER(c_void_p)],
                    byref(IID_IAudioSessionManager2), CLSCTX_ALL, None, byref(mgr))
        if hr != 0 or not mgr.value:
            raise OSError(f"Activate(IAudioSessionManager2) 失敗 hr=0x{hr & 0xffffffff:08X}")
        self._mgr = mgr

    def _match(self, spid: int, names: dict) -> bool:
        if self.match_all:
            return True
        if self.pid is not None:
            return spid in self._target_pids
        return bool(self.name_substr) and self.name_substr in names.get(spid, "").lower()

    def rescan(self):
        """重建符合條件的 meter 清單 (session 會來來去去,故定期重掃)。回傳 [(pid,name,meter)]。"""
        for _, _, m in self._meters:
            _release(m)
        self._meters = []
        names, kids = snapshot()
        if self.pid is not None:
            self._target_pids = process_tree(self.pid, kids)

        senum = c_void_p()       # IAudioSessionManager2::GetSessionEnumerator(&e) vtable#5
        if _vcall(self._mgr, 5, c_long, [POINTER(c_void_p)], byref(senum)) != 0 or not senum.value:
            return self._meters
        try:
            count = c_int(0)     # IAudioSessionEnumerator::GetCount(&n) vtable#3
            if _vcall(senum, 3, c_long, [POINTER(c_int)], byref(count)) != 0:
                return self._meters
            for i in range(count.value):
                ctrl = c_void_p()   # GetSession(i, &ctrl) vtable#4
                if _vcall(senum, 4, c_long, [c_int, POINTER(c_void_p)], i, byref(ctrl)) != 0 or not ctrl.value:
                    continue
                try:
                    spid = c_ulong(0)
                    ctrl2 = _qi(ctrl, IID_IAudioSessionControl2)
                    if ctrl2:
                        _vcall(ctrl2, 14, c_long, [POINTER(c_ulong)], byref(spid))  # GetProcessId vtable#14
                        _release(ctrl2)
                    if not self._match(spid.value, names):
                        continue
                    meter = _qi(ctrl, IID_IAudioMeterInformation)
                    if meter:
                        self._meters.append((spid.value, names.get(spid.value, "?"), meter))
                finally:
                    _release(ctrl)
        finally:
            _release(senum)
        return self._meters

    def peak(self):
        """回傳 (最大峰值 0~1, 是否偵測到失效 session)。失效通常代表該 session 結束,呼叫端可重掃。"""
        mx, dead = 0.0, False
        for _, _, meter in self._meters:
            f = c_float(0.0)       # IAudioMeterInformation::GetPeakValue(&f) vtable#3
            if _vcall(meter, 3, c_long, [POINTER(c_float)], byref(f)) != 0:
                dead = True
                continue
            if f.value > mx:
                mx = f.value
        return mx, dead

    def matched(self):
        return list(self._meters)

    def close(self):
        for _, _, m in self._meters:
            _release(m)
        self._meters = []
        _release(self._mgr)
        _release(self._device)
        _release(self._enum)


# ── 獨立測試用 CLI ─────────────────────────────────────────────────────────────────────────────
def _bar(v: float, width: int = 28) -> str:
    return "#" * int(max(0.0, min(1.0, v)) * width)


def _run_list(rescan_sec: float):
    sm = SessionMeter(match_all=True)
    print("[session_meter] 列出所有發聲行程的即時峰值 (播點音樂看哪條在跳；Ctrl+C 結束)\n")
    prev_lines = 0
    try:
        last_scan = 0.0
        rows = []
        while True:
            now = time.time()
            if now - last_scan >= rescan_sec:
                sm.rescan()
                last_scan = now
            rows = [(pid, name, sm_peak(meter)) for pid, name, meter in sm.matched()]
            # 游標移回起點覆蓋列印 (ANSI;Windows Terminal / conhost 皆支援)
            if prev_lines:
                sys.stdout.write(f"\033[{prev_lines}F")
            lines = [f"  pid={pid:<6} {name[:24]:<24} {peak:4.2f} |{_bar(peak):<28}|\033[K"
                     for pid, name, peak in rows] or ["  (目前沒有任何發聲的 session)\033[K"]
            sys.stdout.write("\n".join(lines) + "\n")
            sys.stdout.flush()
            prev_lines = len(lines)
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\n[session_meter] 結束。")
    finally:
        sm.close()


def sm_peak(meter) -> float:
    f = c_float(0.0)
    if _vcall(meter, 3, c_long, [POINTER(c_float)], byref(f)) != 0:
        return 0.0
    return f.value


def _run_target(name_substr, pid, rescan_sec):
    sm = SessionMeter(name_substr=name_substr, pid=pid)
    tgt = f"pid={pid}(含子孫)" if pid is not None else f'行程名含「{name_substr}」'
    print(f"[session_meter] 盯住 {tgt} 的音量峰值 (Ctrl+C 結束)")
    try:
        last_scan = 0.0
        while True:
            now = time.time()
            # 沒抓到目標時掃快一點,抓到後放慢
            interval = 0.3 if not sm.matched() else rescan_sec
            if now - last_scan >= interval:
                sm.rescan()
                last_scan = now
            peak, dead = sm.peak()
            if dead:
                last_scan = 0.0     # 有 session 失效 → 下一圈立即重掃
            who = ",".join(str(p) for p, _, _ in sm.matched()) or "—"
            print(f"\r[session {who:<12}] peak={peak:4.2f} |{_bar(peak):<28}|", end="", flush=True)
            time.sleep(0.03)
    except KeyboardInterrupt:
        print("\n[session_meter] 結束。")
    finally:
        sm.close()


def main():
    ap = argparse.ArgumentParser(description="WASAPI per-process 音量峰值表 (獨立測試)")
    ap.add_argument("--list", action="store_true", help="列出所有發聲行程的即時峰值")
    ap.add_argument("--process", default=None, help="盯住行程名含此字串者 (如 hermes)")
    ap.add_argument("--pid", type=int, default=None, help="盯住指定 PID (含子孫行程)")
    ap.add_argument("--rescan-sec", type=float, default=1.0, help="重掃 session 間隔秒 (預設 %(default)s)")
    args = ap.parse_args()

    if os.name != "nt":
        sys.exit("session_meter 僅支援 Windows。")
    if args.list:
        _run_list(args.rescan_sec)
    elif args.process or args.pid is not None:
        _run_target(args.process, args.pid, args.rescan_sec)
    else:
        ap.error("請給 --list、--process <名稱> 或 --pid <PID> 其中之一")


if __name__ == "__main__":
    main()
