# tools/ — 開發與打包工具

本目錄存放與建置、打包相關的輔助腳本。

| 檔案 | 用途 |
|------|------|
| `make_icon.py` | 從角色圖產生應用程式圖示 (`app.ico` + `app.png`) |
| `deploy.ps1` | 將編譯好的 exe 打包成可獨立執行的資料夾 |
| `installer.iss` | Inno Setup 腳本，把部署資料夾做成安裝程式 |

---

## 1. 產生圖示 — `make_icon.py`

需要 Python + Pillow (`pip install Pillow`)。

```bash
python tools/make_icon.py                      # 用預設來源 skins/default/Standing.png
python tools/make_icon.py path/to/new_art.png  # 換成新素材 (如 AI 生成圖)
```

輸出 `resources/icons/app.ico`（exe 圖示，多尺寸）與 `resources/icons/app.png`（視窗/托盤）。
更換素材後**重新編譯**即可讓 exe / 視窗 / 托盤套用新圖示。

---

## 2. 發布完整流程

### 步驟 1：建置 Release
用 Qt Creator 切到 **Release** 組態建置（發布版務必用 Release，勿用 Debug）。

### 步驟 2：部署 — `deploy.ps1`
把 exe 與所有相依檔收集成 `dist/YaChiYo/` 可獨立執行資料夾。

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
```

常用參數：

| 參數 | 說明 | 預設 |
|------|------|------|
| `-ExePath` | 指定要打包的 exe | 自動找 `build\` 下最新的 |
| `-QtBin`   | Qt / MinGW 的 bin 目錄 | `C:\msys64\ucrt64\bin` |
| `-DistDir` | 輸出目錄 | `dist\YaChiYo` |

腳本會自動：
- 執行 `windeployqt` 收集 Qt DLL 與外掛（含 GIF 需要的 `qgif.dll`）
- 補上 MinGW 執行期 DLL（`libgcc_s_seh-1` / `libstdc++-6` / `libwinpthread-1`，windeployqt 不會帶）
- 附上皮膚範例 `skins/` 與 `sounds/` 佔位目錄

> 產出的 `dist/YaChiYo/` 可直接壓縮分發。**建議在沒裝 Qt 的乾淨電腦上實測**，確認沒有缺 DLL。

### 步驟 3（選用）：製作安裝程式 — `installer.iss`
需先安裝 [Inno Setup 6](https://jrsoftware.org/isdl.php)。

- 開啟 `tools\installer.iss` → 按 Build，或用命令列 `ISCC.exe tools\installer.iss`
- 產出 `tools\Output\YaChiYo-Setup-<版本>.exe`
- 安裝程式包含：開始選單捷徑、選用桌面捷徑、自動解除安裝（並清除開機自啟登錄項）

---

## 注意事項

- **音效**：`dist/sounds/` 是空佔位目錄。音效需 (1) 以含 Qt Multimedia 的環境重新編譯、(2) 放入對應 `.wav` 檔，才會實際播放。
- **deploy.ps1 為純英文輸出**：Windows PowerShell 5.1 在繁中系統會把無 BOM 的 UTF-8 中文讀成亂碼，故腳本刻意只用 ASCII。
- **版本號**：發布新版時，記得同步更新 `installer.iss` 的 `AppVersion` 與 `resources/app.rc` 的版本資訊。
