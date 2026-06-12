#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_icon.py — 從來源圖產生 YaChiYo 的應用程式圖示

ZH: 產出兩個檔案到 resources/icons/
    - app.ico : 多尺寸 Windows 圖示 (供 resources/app.rc 嵌入 exe)
    - app.png : 256x256 PNG (供視窗 / 系統托盤，嵌入 resources.qrc)
EN: Generates app.ico (multi-size, embedded into the exe via app.rc) and
    app.png (256x256, used for window / tray icon via resources.qrc).

用法 / Usage:
    python tools/make_icon.py [來源圖路徑]
    python tools/make_icon.py                     # 預設用 Standing.png
    python tools/make_icon.py path/to/new_art.png # 換成新素材 (如 AI 生成圖)

依賴 / Requires: Pillow (pip install Pillow)

備註 / Note:
    來源圖會以「置中正方裁切」處理 (取較短邊、水平/垂直置中)，
    確保小尺寸圖示填滿且不變形。若來源已是正方形去背圖，效果最佳。
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("錯誤：需要 Pillow。請執行  pip install Pillow")

# ZH: 專案根目錄 = 本檔的上一層 | EN: Project root = parent of this file's dir
ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = ROOT / "resources/images/characterAnimation/Standing.png"
OUT_DIR = ROOT / "resources/icons"
ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def square_crop(img: Image.Image) -> Image.Image:
    """ZH: 置中正方裁切 | EN: Centre square crop."""
    w, h = img.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    return img.crop((left, top, left + side, top + side))


def main() -> None:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SRC
    if not src.exists():
        sys.exit(f"錯誤：找不到來源圖 {src}")

    img = Image.open(src).convert("RGBA")
    sq = square_crop(img)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ico_path = OUT_DIR / "app.ico"
    png_path = OUT_DIR / "app.png"

    sq.save(ico_path, format="ICO", sizes=ICO_SIZES)
    sq.resize((256, 256)).save(png_path)

    print(f"來源 / source : {src}")
    print(f"已產生 / wrote: {ico_path}  (sizes: {[s[0] for s in ICO_SIZES]})")
    print(f"已產生 / wrote: {png_path}  (256x256)")
    print("提示：若更換了素材，重新編譯即可讓 exe / 視窗 / 托盤套用新圖示。")


if __name__ == "__main__":
    main()
