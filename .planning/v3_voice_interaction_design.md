# V3 — 反向收音 + 聊天室（實作藍圖）

> 建立：2026-07-20　｜前置：V1（喚醒詞）/ V1.5 / V2（真嘴型）全完工，見 memory `voice_integration_progress`。
> 原則：**不改 Hermes**（Path B）。Hermes 是唯一的腦；桌寵只讀它的 `state.db`、只往它的 conhost 注入。

---

## 0. 核心決策（已定案）

| 項目 | 決定 |
|---|---|
| 3a 觸發送鍵路徑 | **方案 A** — 桌寵 `QProcess` spawn 單發注入腳本（重用 wakeword 的 conhost 注入） |
| 3a 觸發手勢 | **雙擊** ＋ 右鍵選單「跟八千代說話」；單擊維持 TapBody 摸摸 |
| 3b 聊天室的腦 | **統一 Hermes**（記憶必須跟語音共享） |
| 3b 讀紀錄 | **唯讀查 `state.db` 的 `messages` 表**（語音＋文字自動含入） |
| 3b 打字 | **WriteConsoleInput 注入整句＋Enter 到語音那個 Hermes conhost**（同 process/session＝共享記憶） |
| 3b 顯示範圍 | 預設「當前 session」，可捲動載入更舊 |
| 3b 打字時 Hermes 狀態 | 需停在一般文字輸入列（非 `/voice` 錄音中）；實作先做 smoke test |

**為何不用 pipe / 不用 scrape**：pet-owned Hermes pipe 有 Windows TUI-pipe 壞掉風險且與語音實例分家；scrape conhost stdout 太脆。改「讀 DB＋寫注入」後，記憶天然共享、讀取穩定、零額外相依。

---

## 1. 已驗證的事實（de-risk 完成 2026-07-20）

- `%LOCALAPPDATA%\hermes\state.db`（SQLite + WAL）表 `messages`：
  欄位 `id, session_id, role('user'|'assistant'), content, timestamp, active, reasoning(不顯示)…`。
- **語音與文字對話都寫進同一張 `messages`**（同一 Hermes 引擎）。
- content 為正常 **UTF-8**；`file:state.db?mode=ro`（uri）唯讀開啟，**WAL 允許 Hermes 寫入同時桌寵讀**，不互鎖。
- 新訊息輪詢：`WHERE id > :lastSeenId ORDER BY id`。當前 session：`sessions` 依 `started_at` 最新一列。
- wakeword 注入現況（`tools/wakeword.py`）：`inject_conhost(win_pid)` = FreeConsole→AttachConsole(候選 PID：視窗 PID＋子孫)→`WriteConsoleInputW` 送 4 筆 INPUT_RECORD（Ctrl+B）。64-bit 必設 argtypes/restype；注入須在 `DETACHED_PROCESS` 子行程。`find_window(title, class)`、`descendant_pids()` 可重用。

---

## 2. 3a — 反向收音（雙擊 / 右鍵 → 開錄音）

### 2.1 注入腳本（Python，重用 wakeword）
- 從 `tools/wakeword.py` 抽出注入核心，新增可獨立呼叫的單發入口：
  `python tools/hermes_inject.py ctrl-b [--window-class ConsoleWindowClass]`
  - 內含：`find_window` 鎖 Hermes conhost → 取 win_pid → `DETACHED_PROCESS` 子行程跑 `inject_conhost`。
  - 作法：把 `wakeword.py` 現有的注入函式群（`_rec / descendant_pids / inject_conhost / find_window / list_windows`）搬到共用模組 `tools/hermes_inject.py`；`wakeword.py` 改 import 它（**不重複維護注入邏輯**）。
  - `hermes_inject.py` 同時提供 3b 打字用的 `type-text`（見 §3.4）。

### 2.2 桌寵側觸發
- `src/core/mainwindow.cpp`
  - 新增 `void mouseDoubleClickEvent(QMouseEvent*)`：Live2D 模式且 voiceEnabled 時 → 呼叫 `triggerVoiceRecord()`。
  - 右鍵選單（`contextMenuEvent`, 約 [mainwindow.cpp:147](../src/core/mainwindow.cpp)）加一項 `"跟八千代說話"` → 同 `triggerVoiceRecord()`。
  - `triggerVoiceRecord()`：
    1. `QProcess::startDetached(python, {tools/hermes_inject.py, "ctrl-b", "--window-class", "ConsoleWindowClass"})`。
    2. **樂觀** `enterVoiceState(Listening)`（重用既有 [mainwindow.cpp:732](../src/core/mainwindow.cpp) `onVoiceListening`/`enterVoiceState`），即時回饋；真正 speaking 仍由 `mouth_loopback` POST。
  - python 路徑與 tools 路徑：沿用專案既有取法（settings 或相對執行檔），避免寫死。

### 2.3 3a 驗收
- 雙擊桌寵 → 聽到 Hermes 880Hz 嗶聲（開始錄音）、桌寵切 Listening。
- 右鍵「跟八千代說話」同效果。
- 單擊仍只播 TapBody，不誤觸錄音。

---

## 3. 3b — 聊天室（右鍵開啟；讀 DB ＋ 寫注入）

### 3.1 新模組 `ChatStore`（讀 state.db）
- `src/modules/chatstore.{h,cpp}`（比照 `voicebridge` 的模組風格）。
- 用 Qt 內建 `QSQLITE` driver（windeployqt 會帶 plugin，零額外相依）；連線字串 `QSQLITE` + `QSqlDatabase` 設 `ConnectOptions("QSQLITE_OPEN_READONLY")`，DB 路徑用 `file:...?mode=ro`（或開唯讀 + WAL）。
- API：
  - `QString currentSessionId()` — `sessions` 依 `started_at DESC LIMIT 1`。
  - `QList<ChatMsg> loadSession(sessionId, beforeId=INT_MAX, limit=50)` — 捲動載入更舊。
  - `QList<ChatMsg> poll(qint64 &lastSeenId)` — `WHERE id > lastSeenId AND role IN('user','assistant') AND content IS NOT NULL ORDER BY id`。
  - `signal newMessages(QList<ChatMsg>)`。
- `ChatMsg { qint64 id; QString role, content; double ts; }`（**忽略 `reasoning`**）。
- 更新機制：`QTimer` 每 ~500ms `poll()`（先求穩；之後可換 `QFileSystemWatcher` 盯 `state.db-wal`）。
- DB 路徑解析：`%LOCALAPPDATA%\hermes\state.db`（`qEnvironmentVariable("LOCALAPPDATA")`）；找不到時聊天室顯示「找不到 Hermes 資料庫」。

### 3.2 聊天室視窗 `ChatWindow`
- `src/ui/chatwindow.{h,cpp}`（獨立 `QWidget`，非 frameless overlay——這是正常視窗，可移動/縮放）。
- 版面：上＝訊息列表（`QListView`＋自訂 delegate，或先用 `QTextBrowser` 氣泡 HTML 快速成形）；下＝`QLineEdit`＋送出鈕。
- 渲染：user 靠右、assistant 靠左；只顯示 `content`。
- 捲到頂 → 呼叫 `loadSession(..., beforeId=最舊已載入id)` 載更舊。
- 收到 `ChatStore::newMessages` → append 並自動捲到底（若使用者原本就在底部）。
- 開啟：右鍵選單加 `"聊天室"` → 建立/顯示單例 `ChatWindow`（voiceEnabled 時才出現此選項）。

### 3.3 送出打字
- `ChatWindow` 送出 → 呼叫 `MainWindow::sendChatText(QString)`（或 ChatWindow 自己 spawn）：
  `QProcess::startDetached(python, {tools/hermes_inject.py, "type-text", text, "--window-class", "ConsoleWindowClass"})`。
- 送出後**不**自己把訊息塞進列表——等 `ChatStore::poll` 從 DB 讀回 user turn 再顯示（**單一真相來源＝DB**，避免重複/不同步）。
- 送出後桌寵可樂觀切 Thinking（可選）。

### 3.4 注入打字（Python，`hermes_inject.py type-text`）
- 擴充注入器：把字串逐字元轉成 `KEY_EVENT` INPUT_RECORD（`UnicodeChar` 直接帶字元，不需掃描碼；中文可用 `wVirtualKeyCode=0` + `UnicodeChar`），最後補一筆 `Enter`（`\r`）。
- 同樣走 `DETACHED_PROCESS` 子行程 + `AttachConsole` + `WriteConsoleInputW`。
- ⚠ **先做 smoke test**：確認 Hermes 一般輸入列能吃 `WriteConsoleInput` 灌入的整串 Unicode（含中文）＋ Enter 觸發送出。若 TUI 對貼上式輸入有節流，改為分批/加微延遲（可在子行程內 sleep 幾 ms/字元）。

### 3.5 3b 驗收
- 右鍵「聊天室」開窗 → 看到**當前 session 的完整往來**（含之前用**語音**講的內容）。
- 捲到頂載入更舊訊息。
- 打字送出 → Hermes 收到並回覆 → 幾百 ms 內聊天室自動出現 user + assistant 兩則（來自 DB 輪詢）。
- 打字這句之後用語音再問 → 八千代記得打字說過的（同一 session/記憶）。

---

## 4. 檔案清單

**新增**
- `tools/hermes_inject.py` — 共用注入器（`ctrl-b` / `type-text`），承接 wakeword 的注入函式群。
- `src/modules/chatstore.{h,cpp}` — 唯讀讀 `state.db`。
- `src/ui/chatwindow.{h,cpp}` — 聊天室視窗。

**修改**
- `tools/wakeword.py` — 注入函式群移到 `hermes_inject.py`，改 import（不變行為）。
- `src/core/mainwindow.{h,cpp}` — `mouseDoubleClickEvent`、右鍵兩個新項、`triggerVoiceRecord()`、`sendChatText()`、持有 `ChatStore`/`ChatWindow`。
- `CMakeLists.txt` — 加入新 `.cpp`；確認連結 `Qt6::Sql`。

---

## 5. 實作順序（建議分兩批 commit）

1. **3a 先落地**（自我完備、低風險、Path B 無傷）：
   `hermes_inject.py` 抽共用 + `ctrl-b` → 桌寵雙擊/右鍵觸發 + 樂觀 Listening。驗收 §2.3。
2. **3b 讀取先於寫入**：
   a. `hermes_inject.py type-text` **smoke test**（§3.4）先跑通再說。
   b. `ChatStore` 唯讀讀 DB → `ChatWindow` 純顯示（含語音紀錄）＋捲動載入。驗收「看紀錄」。
   c. 接打字送出（§3.3）。驗收完整往返 §3.5。

---

## 6. 風險與備援

- **type-text 注入被 TUI 吃字/亂序**（最大未知）→ 先 smoke test；不行則分批＋微延遲，或退而求其次：打字也走「先寫入剪貼簿 + 注入貼上熱鍵」（若 Hermes TUI 支援）。
- **state.db 路徑/檔名未來變動**（Hermes 升級）→ `ChatStore` 找不到表時優雅報錯，不崩桌寵。
- **同時多讀一寫的鎖**→ 已驗證 `mode=ro`+WAL 可行；仍偶發 busy 時加短重試。
- **Hermes 沒在跑**→ 3a 注入找不到 conhost（提示「請先開啟 Hermes」）；3b 聊天室仍可讀歷史 DB。
