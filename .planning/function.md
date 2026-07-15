# YaChiYo 桌寵專案 — 已實作功能清單

> 最後更新：2026-06-11

---

## 0. 專案架構（模組化）

```
src/
├── main.cpp           ← 程式入口 + PetConfig 功能開關（physics / behavior / ai 可獨立關閉）
├── core/
│   └── mainwindow     ← 狀態機、事件協調、動畫皮膚、托盤、滑鼠互動
├── modules/
│   ├── petphysics     ← 物理引擎（純邏輯類別，無 Qt Widget 依賴）
│   ├── petbehavior    ← 行為 AI 決策（純邏輯類別）
│   └── aiclient       ← HTTP / AI 通訊（QObject，signal/slot 回傳結果）
└── ui/
    └── settingscenter ← 設定中心（設定分頁 + 開發者監控分頁）
resources/
├── resources.qrc      ← 資源清單（執行期路徑 :/res/images/...）
└── images/            ← 角色圖片
ai_server/
└── inference.py       ← Python FastAPI + Stable Diffusion Img2Img 後端
```

新增功能模組時：在 `src/modules/` 放一對 `.h/.cpp`，並在 `CMakeLists.txt` 的 `PROJECT_SOURCES` 加入即可（include 路徑已透過 `target_include_directories` 設定）。

---

## 1. 視窗與外觀

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 無邊框透明視窗 | 移除標題列與邊框，背景透明，桌寵融入桌面 | `src/core/mainwindow.cpp` 建構子 |
| 視窗永遠置頂 | `WindowStaysOnTopHint`，可由設定中心開關 | `src/core/mainwindow.cpp` `setAlwaysOnTop()` |
| 圖片自適應顯示 | QLabel + QPixmap，250×250 × petScale 縮放並保持比例 | `src/core/mainwindow.cpp` `updatePetSkin()` |
| 水平翻轉 | 角色朝左移動時自動水平翻轉（Qt 6 `flipped()`） | `src/core/mainwindow.cpp` `updatePetSkin()` |
| GIF / PNG 皮膚切換 | `petSkinType`（預設 GIF 優先），設定中心可切換；GIF 不存在時自動退回 PNG | `src/core/mainwindow.cpp` `updatePetSkin()`、`setPetSkinType()` |

---

## 2. 滑鼠互動

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 左鍵拖曳 | 拖曳桌寵到任意位置，拖曳時進入 `Captured` 狀態 | `src/core/mainwindow.cpp` mouse events |
| 釋放判定 | 鬆開時距地面 >10px 進入 `Hovering`，否則 `Standing` | `src/core/mainwindow.cpp` `mouseReleaseEvent()` |
| 右鍵選單 | 設定中心 / AI 變身 / 關閉隱藏桌寵 | `src/core/mainwindow.cpp` `contextMenuEvent()` |

---

## 3. 物理引擎（PetPhysics 模組）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 重力系統 | 未落地時以 `gravity = 0.8`（可調）持續下落 | `src/modules/petphysics.cpp` `applyGravity()` |
| 地面碰撞 + 回彈 | 落地速度 >1.5 時以 `bounceFactor = -0.5` 回彈 | `src/modules/petphysics.cpp` `resolveGroundCollision()` |
| 左右邊界碰撞 | 碰邊緣反彈（`wallBounceFactor = -1`） | `src/modules/petphysics.cpp` `resolveBoundaryCollision()` |
| 水平加速 / 摩擦 | `acceleration = 0.2` 加速、`friction = 0.15` 減速 | `src/modules/petphysics.cpp` `updateWalkVelocity()` |
| Hovering Sin Wave | 振幅 8px、相位增量 0.08 的上下浮動 | `src/modules/petphysics.cpp` `calcHoverY()` |
| Flying 直線飛行 | 歸一化方向向量 × `flySpeed = 1.5`，抵達後轉 Hovering | `src/modules/petphysics.cpp` `calcFlyStep()` |
| 60 FPS 更新 | `physicsTimer` 每 16ms 觸發 | `src/core/mainwindow.cpp` `initAllConnect()` |

---

## 4. 行為 AI（PetBehavior 模組）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 定時決策 | 每 `behaviorInterval`（預設 5s，可調）執行一次 | `src/core/mainwindow.cpp` `decideNextAction()` |
| 地面機率行動 | 50% 散步、10% 起飛、40% 原地站立 | `src/modules/petbehavior.cpp` `decideOnGround()` |
| 空中機率行動 | 50% 繼續浮空、30% 飛去新位置、20% 落地 | `src/modules/petbehavior.cpp` `decideInAir()` |
| 位置感知方向 | 越靠右→往左機率越高，避免走出螢幕 | `src/modules/petbehavior.cpp` `decideOnGround()` |
| 隨機步數 | 每次散步隨機 90~210 步 | `src/modules/petbehavior.cpp` |
| Captured 保護 | 被抓取時強制停止一切行動決策 | `src/core/mainwindow.cpp` `decideNextAction()` |

---

## 5. 動畫與皮膚系統（資料驅動）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 資料驅動皮膚 | 每套皮膚 = 一資料夾 + `skin.json`，定義各狀態的 png/frames/gif；格式見 `resources/skins/README.md` | `src/modules/petskin.*`、`resources/skins/default/skin.json` |
| 皮膚掃描 | 掃描內建 `:/res/skins/` 與使用者 `<執行檔>/skins/`，同 id 使用者覆蓋內建 | `src/modules/petskin.cpp` `PetSkin::available()` |
| 皮膚切換 + 記憶 | 設定中心下拉選單切換，選擇存入 `PetSettings.currentSkin`，重啟自動套用 | `src/ui/settingscenter.cpp`、`src/core/mainwindow.cpp` `setSkin()` |
| 序列幀動畫 | 幀數與間隔由 `skin.json` 定義（如 Walking 6 幀 / 150ms） | `src/core/mainwindow.cpp` `turnImageSet()` |
| 動態播放速度 | 行走動畫頻率隨移動速度調整（80~350ms） | `src/core/mainwindow.cpp` `updatePhysics()` Walking case |
| GIF 動畫 | 狀態為 gif 型且「GIF 動畫皮膚」開啟時播放（如 Captured.gif） | `src/core/mainwindow.cpp` `updatePetSkin()` |
| 路徑解析與退回 | 序列幀 → 該狀態 png → fallback 狀態 → 預設 Standing | `src/modules/petskin.cpp`、`updatePetSkin()` |

---

## 6. 狀態機

| 狀態 | 實作情況 | 說明 |
|------|----------|------|
| `Standing` | ✅ 已實作 | 重力 + 地面碰撞，靜止站立 |
| `Walking` | ✅ 已實作 | 水平移動 + 序列幀動畫 + 重力 + 邊界碰撞 |
| `Flying` | ✅ 已實作 | 隨機目標直線飛行，抵達後轉 Hovering |
| `Hovering` | ✅ 已實作 | 零重力 + Sin Wave 微浮動 |
| `Captured` | ✅ 已實作 | 暫停物理引擎，允許拖曳，播放 Captured.gif |
| `AI_Processing` | ✅ 已實作 | 等待 AI 後端回傳，期間拒絕新請求 |

---

## 7. AI 模組（AIClient + 皮膚 AI）

> 註：「單張變身（AI 變身）」及後端 `/transform` 端點已於 2026-07 移除（QLabel 時代產物，Live2D 主線下失效）。
> 「AI 生成皮膚」右鍵選單項**暫時隱藏**（實際跑過可行，但預設參數生成結果崩壞，待調校後再開回）；程式碼與 `/generate_skin` 端點保留。

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 生成整套皮膚（選單暫隱） | `requestSkinGeneration()`：上傳參考圖 + 當前皮膚姿勢幀 → POST `/generate_skin` | `src/modules/aiclient.cpp` `generateSkin()`、`mainwindow.cpp` `requestSkinGeneration()` |
| 寫入新皮膚 | 回傳整套寫成 `<執行檔>/skins/ai_<時間戳>/` + 複製改名的 `skin.json`，並 `setSkin()` 立即套用 | `src/core/mainwindow.cpp` `onSkinReady()` |
| 訊號回傳 | `skinReady(QList<QImage>)` / `errorOccurred(QString)`；`pendingKind` 區分端點 | `src/modules/aiclient.h` |
| 提示與通知 | 生成中 `QProgressDialog` 忙碌視窗，完成/失敗 `QSystemTrayIcon` 通知 + `QToolTip` | `src/core/mainwindow.cpp` `showBusy()`/`onSkinReady()`/`onAIError()` |
| 防重複請求 | `isBusy()` + `AI_Processing` 狀態 + `decideNextAction` 守衛 | `src/modules/aiclient.cpp`、`src/core/mainwindow.cpp` |
| Python 後端 | FastAPI + SD1.5 + **ControlNet Canny（姿勢）+ IP-Adapter（身份）+ 負面提示詞**，Docker 化執行 | `ai_server/inference.py`、`ai_server/docker-compose.yml` |

> ⚠️ IP-Adapter 與 `enable_attention_slicing()` 衝突（會覆蓋其注意力處理器，報
> `'tuple' object has no attribute 'shape'`）——後端僅用 `enable_vae_slicing()`，OOM 時改 `enable_model_cpu_offload()`。

---

## 8. 設定中心（SettingsCenter）

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 設定分頁 | 行走速度、決策間隔、桌寵大小、重力強度、視窗置頂、GIF 皮膚、AI 提示詞 | `src/ui/settingscenter.cpp` `initSettingsInterface()` |
| 開發者監控面板 | 自動讀取 MainWindow `Q_PROPERTY` 列表，勾選即時監測（100ms 刷新） | `src/ui/settingscenter.cpp` `initDeveloperInterface()`、`refreshDebugInfo()` |
| 可監測屬性 | `currentState`、`currentVelocityX`、`targetVelocityX`、`walkSteps`、`decisionTimerRemaining`、`actionRoll`、`ImageSwitchTimerRemaining`、`currentSetNumber`、`lastAIError` | `src/core/mainwindow.h` Q_PROPERTY 區 |

---

## 9. 系統托盤

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| 托盤圖示 + 選單 | 顯示桌寵 / 設定中心 / 退出程式 | `src/core/mainwindow.cpp` `initTrayIcon()` |
| 雙擊喚回 | 雙擊托盤圖示重新顯示桌寵 | 同上 |

---

## 10. 資源管理

| 功能 | 說明 | 相關檔案 |
|------|------|----------|
| Qt Resource System | 角色圖片嵌入執行檔（Standing.png、Captured.png/gif、Walking 1-6） | `resources/resources.qrc` |
| 雙圖片路徑 | `characterAnimation/` 靜態主圖、`testImageSet/` 序列幀 | `src/core/mainwindow.h` |
| .gitignore | 排除 build 產物、AI 模型權重、Python venv、開發紀錄截圖等 | `.gitignore` |
