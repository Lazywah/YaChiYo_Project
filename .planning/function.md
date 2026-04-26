# YaChiYo 桌寵專案 — 已實作功能清單

> 最後更新：2026-04-26

---

## 1. 視窗與外觀

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 無邊框透明視窗 | 移除標題列與邊框，背景透明，桌寵融入桌面 | `mainwindow.cpp` L26-32 |
| 視窗永遠置頂 | 使用 `WindowStaysOnTopHint`，桌寵不會被其他視窗遮擋 | `mainwindow.cpp` L29 |
| 圖片自適應顯示 | 透過 QLabel + QPixmap 顯示角色，固定縮放至 250×250 並保持比例 | `mainwindow.cpp` L150 |
| 水平翻轉 | 角色朝左移動時自動水平翻轉圖片 (使用 Qt 6 `flipped()`) | `mainwindow.cpp` L140-148 |

---

## 2. 滑鼠互動

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 左鍵拖曳 | 按住左鍵可拖曳桌寵到任意位置，拖曳時進入 `Captured` 狀態 | `mainwindow.cpp` L86-117 |
| 右鍵選單 | 右鍵彈出功能選單，目前包含「設定中心」與「關閉桌寵」兩個選項 | `mainwindow.cpp` L68-84 |
| 抓取 / 釋放 | 左鍵按下時暫停物理引擎 (Captured)，鬆開後恢復站立並重啟物理引擎 | `mainwindow.cpp` L90, L112-113 |

---

## 3. 物理引擎

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 重力系統 | 未落地時以 `gravity = 0.8` 的加速度持續下落 | `mainwindow.cpp` L306-318 |
| 地面碰撞偵測 | 偵測螢幕底部（避開工作列），到達地面後停止下落 | `mainwindow.cpp` L320-347 |
| 垂直回彈 | 落地瞬間若速度大於 1.5，以 `bounceFactor = -0.5` 回彈 | `mainwindow.cpp` L330-334 |
| 左右邊界碰撞 | 碰到螢幕左右邊緣時反彈（`wallBounceFactor = -1`） | `mainwindow.cpp` L349-366 |
| 水平加速 / 減速 | 行走時朝目標速度以 `acceleration = 0.2` 加速，步數用完後以 `friction = 0.15` 摩擦減速 | `mainwindow.cpp` L234-250 |
| 60 FPS 更新 | 物理引擎計時器每 16ms 觸發一次 (`physicsTimer`) | `mainwindow.cpp` L54 |

---

## 4. 行為 AI（隨機行為決策）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 定時決策 | 每 5 秒執行一次 `decideNextAction()` | `mainwindow.cpp` L58 |
| 機率行動 | 60% 開始散步，40% 原地站立 | `mainwindow.cpp` L401-422 |
| 位置感知方向 | 散步方向受當前位置影響：越靠右→往左機率越高，避免走出螢幕 | `mainwindow.cpp` L396-410 |
| 隨機步數 | 每次散步隨機 90~210 步 | `mainwindow.cpp` L414 |
| Captured 保護 | 被抓取時強制停止一切行動決策 | `mainwindow.cpp` L385-392 |

---

## 5. 動畫系統

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 序列幀動畫 | Walking 狀態使用 6 幀 PNG 序列幀播放動畫 | `mainwindow.cpp` L47, `resources.qrc` |
| AnimationConfig 結構 | 以 `QMap<State, AnimationConfig>` 管理各狀態的幀數與播放速度 | `mainwindow.h` L62-66, L80 |
| 動態播放速度 | 行走動畫頻率隨移動速度動態調整（速度越快動畫越快，區間 80~350ms） | `mainwindow.cpp` L252-261 |
| 狀態切換自動重置 | `setState()` 時自動停止計時器並重置 `currentSetNumber` | `mainwindow.cpp` L187-218 |

---

## 6. 狀態機

| 狀態 | 實作情況 | 說明 |
|------|----------|------|
| `Standing` | ✅ 已實作 | 啟用重力 + 地面碰撞，桌寵靜止站立 |
| `Walking` | ✅ 已實作 | 水平移動 + 序列幀動畫 + 重力 + 邊界碰撞 |
| `Captured` | ✅ 已實作 | 暫停物理引擎，允許拖曳 |
| `AI_Processing` | ✅ 已實作（基本框架） | 發送圖片至 Python 後端，等待結果回傳 |
| `Flying` | ❌ 預留（feat-1） | 預計：關閉重力，飛向滑鼠位置 |
| `Hovering` | ❌ 預留（feat-2） | 預計：零重力 + Sin Wave 微浮動 |

---

## 7. AI 通訊模組

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| Qt → Python HTTP POST | 擷取當前 Label 畫面轉 Base64，連同 prompt 以 JSON 發送至 `http://127.0.0.1:8000/transform` | `mainwindow.cpp` L425-448 |
| 接收 AI 結果 | 解析回傳的 Base64 圖片並更新至 Label | `mainwindow.cpp` L450-464 |
| Python 後端 (FastAPI) | 使用 Stable Diffusion v1.5 的 Img2Img Pipeline，strength=0.6 | `ai_server/inference.py` |
| 防重複請求 | AI_Processing 狀態中不接受新的 AI 請求 | `mainwindow.cpp` L427-428 |

---

## 8. 設定中心（SettingsCenter）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 獨立 QDialog 視窗 | 從右鍵選單開啟，置中顯示 | `settingscenter.cpp` L39-56 |
| 雙分頁 TabWidget | 「設定 (Settings)」分頁（空）+ 「開發者 (Developer)」分頁 | `settingscenter.ui` |
| 開發者監控面板 | 自動讀取 MainWindow 的 `Q_PROPERTY` 列表，以勾選方式即時監測變數 | `settingscenter.cpp` L58-108 |
| 即時數值刷新 | 切換至開發者分頁時以 100ms 間隔刷新所有勾選的屬性值 | `settingscenter.cpp` L26-32, L73-108 |
| 可監測的屬性 | `currentState`、`currentVelocityX`、`targetVelocityX`、`walkSteps`、`decisionTimerRemaining`、`actionRoll`、`ImageSwitchTimerRemaining`、`currentSetNumber` | `mainwindow.h` L30-37 |

---

## 9. 資源管理

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| Qt Resource System (.qrc) | 將角色圖片嵌入執行檔，包含 Standing.png、Captured.png、Captured.gif、Walking 序列幀 (1-6) | `resources.qrc` |
| 雙圖片路徑 | `characterAnimation/` 放靜態主圖，`testImageSet/` 放序列幀動畫 | `mainwindow.h` L78-79 |
| .gitignore 完善 | 排除 build 產物、AI 模型權重、Python 虛擬環境、資料集等 | `.gitignore` |
