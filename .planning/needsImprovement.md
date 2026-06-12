# YaChiYo 桌寵專案 — 需改進項目

> 最後更新：2026-06-11

---

## 🔴 高優先級

### 1. AI 通訊端點不一致 [x]
- **問題**：Qt 端發送 POST 至 `/transform`，但 Python 後端只定義了 `/generate`
- **修正**：兩端統一為 `/transform`（`src/modules/aiclient.cpp`、`ai_server/inference.py`）

### 2. AI 錯誤處理不足 [x]
- **問題**：收到錯誤時只做 `deleteLater()` 就直接切回 Standing，沒有任何錯誤通知
- **修正**：`AIClient` 改用 `resultReady` / `errorOccurred` 訊號；錯誤透過 `QToolTip` 顯示，並存入 `lastAIError`（可在 Developer 面板監測）

### 3. AI 請求缺少觸發入口 [x]
- **問題**：`requestAIProcessing()` 已實作但沒有任何地方呼叫它
- **修正**：右鍵選單加入「AI 變身」選項

---

## 🟡 中優先級

### 1. Flying / Hovering 狀態未實作 [x]
- **問題**：狀態機中 `Flying` 和 `Hovering` 只有空殼
- **修正**：兩者均已實作於 `PetPhysics`（`calcFlyStep()` 直線飛行、`calcHoverY()` Sin Wave 浮動），決策邏輯於 `PetBehavior::decideInAir()`

### 2. 設定分頁完全為空 [x]
- **問題**：「設定 (Settings)」分頁沒有任何控制項
- **修正**：已加入行走速度、決策間隔、桌寵大小、重力強度、視窗置頂、GIF 皮膚模式、AI 提示詞（`src/ui/settingscenter.cpp` `initSettingsInterface()`）

### 3. 動畫資源不完整 [x]
- **問題**：GIF 載入邏輯被註解掉、`petSkinType` 從未被使用
- **修正**：GIF 載入已啟用（Captured.gif 正常播放）；`petSkinType` 連接至設定中心「GIF 動畫皮膚」開關，可切換 PNG / GIF 模式
- **後續**：Standing 等其他狀態的序列幀 / GIF 待皮膚 AI（To-do Item 15）產出

### 4. 行為決策計時器註解與實際不一致 [x]
- **修正**：間隔已改為可配置變數 `behaviorInterval`（預設 5000ms），並可由設定中心調整

---

## 🟢 低優先級（代碼品質）

### 1. 未使用多螢幕支援 [x]
- **修正**：`getCurrentScreenRect()` 已使用 `QGuiApplication::screenAt()`，找不到時退回 `primaryScreen()`

### 2. 記憶體管理疑慮 [x]
- **修正**：`imageSwitchTimer` 已改為 `new QTimer(this)`，由父物件管理生命週期

### 3. 硬編碼數值過多 [x]
- **修正**：重力、行走速度、決策間隔、桌寵大小已連接設定中心 UI 即時調整
- **備註**：回彈係數、摩擦力等物理常數集中於 `src/modules/petphysics.h`，如需調整可單點修改

### 4. UI 佈局使用絕對座標 [x]
- **修正**：`settingscenter.ui` 已改用 `QVBoxLayout` 管理整個對話框

### 5. 缺少系統托盤整合 [x]
- **修正**：已加入 `QSystemTrayIcon`（`initTrayIcon()`），支援顯示桌寵 / 設定中心 / 退出 / 雙擊喚回

### 6. 缺少音效系統 [ ]
- **問題**：所有互動（拖曳、落地、走路、碰牆）都沒有音效
- **建議**：整合 `QSoundEffect`，規劃為 `src/modules/petsound` 模組（對應 To-do Item 7）

### 7. 缺乏國際化支援 [ ]
- **問題**：右鍵選單文字和設定中心 UI 皆為硬編碼中文
- **建議**：導入 Qt Linguist (`tr()`) 支援多語系（對應 To-do Item 8）

### 8. MainWindow UI 設計冗餘 [x]
- **修正**：`mainwindow.ui` 已移除 `QMenuBar` 與 `QStatusBar`，僅保留顯示用 QLabel
