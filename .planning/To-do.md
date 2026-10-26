# YaChiYo 桌寵專案 — 待實作功能清單

> 最後更新：2026-10-26
> 標記：`[x]` 完成　`[~]` 部分完成　`[ ]` 未開始

---

## 1. 圖生圖自開發 [ ]
- 目前皮膚 AI 用現成 SD1.5 + ControlNet + IP-Adapter（見 Item 15），尚非自開發

## 2. 遊戲助手 AI 整合 [ ]

## 3. 語言模型整合 [x]
- 改走「雙進程」：桌寵是身體/臉，語音 AI 助手 **Hermes**（`NousResearch/hermes-agent`）是大腦，兩者靠 localhost 溝通
- 主模型 Claude Haiku 5.5，Gemini 退為備援
- 桌寵端：`VoiceBridge`（事件 `127.0.0.1:39217`）、`MouthStream`（嘴型振幅 UDP 39218）、Listening/Thinking/Speaking 狀態
- V2 真嘴型（`tools/mouth_loopback.py` + `session_meter.py`，只讀 Hermes 行程樹的音量）、喚醒詞（`tools/wakeword.py`）、中文語音
- V3：雙擊/右鍵開錄音（`tools/hermes_inject.py`）、聊天室（`ChatStore` 讀 Hermes `state.db` + `ChatWindow` 打字注入）
- 一鍵啟動/關閉：`tools/start_yachiyo.ps1` / `stop_yachiyo.ps1`
- 設計藍圖：`.planning/v3_voice_interaction_design.md`

## 4. 語言模型自開發 [ ]

## 5. AI 上網功能 [~]
- [x] Hermes `browser` 工具（`agent-browser`）已實測可自行開網頁查資料（需 `PYTHONUTF8=1`）
- [ ] `web_search` / `web_extract` 仍缺搜尋後端金鑰（Tavily 需綁卡已排除；Exa/Firecrawl 待確認）

## 6. AI 管家化 [~]
- [x] 指令層：Hermes `terminal` + `file` 工具可用
- [ ] GUI 層：`computer_use`（cua-driver 0.22.2）已安裝、健檢全綠，toolset 尚未啟用（`hermes tools enable computer_use`）

## 7. 音效系統 [~]
- [x] `PetSound` 框架（`QSoundEffect`，Qt Multimedia 可選）：落地 / 碰牆 / 抓取 / 釋放，檔案缺失時靜默
- [ ] 準備音效檔（執行檔旁 `sounds/`：`land.wav` / `wall.wav` / `grab.wav` / `release.wav`）
- [ ] 走路音效
- [ ] 建置環境安裝 `qt6-multimedia`（目前 CMake 偵測不到 → 音效實際停用）

## 8. 完整國際化支援 (i18n) [ ]
- 導入 Qt Linguist 工具鏈
- 建立 `.ts` 翻譯檔
- 將所有 `tr()` 字串翻譯為英文 / 其他語言
- 目前已完成：程式碼中的雙語註解 (ZH: | EN:)

## 9. 疊層列表 (用於標示那些視窗不被桌寵覆蓋) [x]
- `PetZGuard`（`src/modules/petzguard.*`）：前景為讓位清單程式或全螢幕程式時，桌寵以 `SetWindowPos` 退到其正後方，切走後恢復置頂
- 設定中心「視窗設定」：全螢幕時讓開 + 不覆蓋的程式清單（可挑執行中程式或手動輸入 exe）；僅在視窗置頂時有效

## 10. Hovering 狀態應加上 Sin wave [x]
- 已於 `updatePhysics()` Hovering case 實作（`PetPhysics::calcHoverY()`）；飛行改直線，sin wave 僅留懸浮原地

## 11. 所有圖示應經由圖生圖 AI 重新生成 → 併入 Item 15b

## 12. 桌寵顯示方式應當用 (multi-Agent 並結合多個 AI 模組進行) 或 (anime-engine 方式進行) [x]
- 已定案走 **Live2D**（見 Item 15c）；3D 路線評估見 `.planning/skin_animation_design.md` §8

## 13. 資訊安全管理 (建立一個 AI 程式碼審核員，使用 LLM 進行 code review，以及安全性的風險評估，輸出一個報告讓我知道風險等級、以及如何修補) [ ]

## 14. 遠端連線桌面 [ ]

## 15. 桌寵皮膚系統 (Skin System)
- **基礎建設**
  - [x] 資料驅動皮膚格式（`PetSkin` + `skin.json`），格式說明見 `resources/skins/README.md`
  - [x] 設定中心皮膚下拉選單 + 掃描 `skins/` 目錄 + `PetSettings` 記憶選擇
- **皮膚 AI 生成**
  - [x] 後端 Docker 化（FastAPI + SD1.5），設定見 `ai_server/README.md`
  - [x] ControlNet Canny（姿勢）+ IP-Adapter（身份）+ 負面提示詞，固定 seed 保持一致
  - [x] 右鍵「AI 生成皮膚 (上傳圖片)」：上傳參考圖 → 批次生成整套 → 寫成 `<執行檔>/skins/ai_<時間戳>/` 並即時套用
  - [x] 生成中提示視窗 + 完成/失敗系統托盤通知
  - [x] 設定中心每次開啟重新掃描皮膚清單（剛生成的 AI 皮膚立即出現，免重啟）
  - [ ] 待改進：寫入位置改用使用者資料夾（避免 Program Files 權限問題）；Captured 動圖（gif）生成
  - [ ] 待調校：IP_SCALE / 提示詞 / 步數的最佳預設（依實際生成品質微調）
  - 備註：此「擴散生成皮膚」保留為平行的**幀動畫**功能；「會動的角色」願景改走 Live2D（見下）
  - **2026-07 狀態**：右鍵選單項**暫時隱藏**（實際跑過可行，但預設參數生成結果崩壞/很恐怖，待 IP_SCALE/提示詞/步數調校穩定後再開回）。程式碼與 `/generate_skin` 端點保留。
  - **2026-07 移除**：早期「AI 變身」單張 img2img（右鍵）+ 後端 `/transform` 端點已刪（QLabel 時代產物，Live2D 主線下失效，功能亦被整套生成涵蓋）。

## 15c. 會動的角色 — Live2D 路線（設計藍圖見 `.planning/skin_animation_design.md`）
- 決策鏈：兩階段（外觀/動畫分離）→ 完全相同（變形而非生成）→ Live2D（業界標準）
- 前提（已接受）：需要綁好骨的 Live2D 模型，非平面圖；AI 僅輔助拆層/補背（遠期）
- 程式碼：`src/live2d/live2dwidget.*`（QOpenGLWidget + Cubism）；CMake 可選式 `YACHIYO_HAS_LIVE2D`
- 啟用方式：`YaChiYo_Project.exe --live2d`（整隻桌寵用 Live2D）/ `--live2d-test`（單獨測試窗）
- [x] **L1 引擎整合**：Core DLL 直連(MinGW)、Framework 編譯、QOpenGLWidget 渲染 Hiyori、
  透明懸浮窗、自動眨眼/呼吸、Pose 圖層、shader 從檔案載入(POST_BUILD 複製 DLL+shader)
- [x] **L2 接進桌寵**：`--live2d` 模式、PetPhysics 驅動移動/拖曳、idle 動作播放、
  走路看向方向(轉頭/眼神)、貼合高度投影、桌寵大小滑桿縮放
- [~] **L2.4**：
  - [x] 拖曳看向滑鼠（2D 轉頭+眼神）、點擊身體 TapBody 動作（Batch B）
  - [x] Live2D 設定 UI：Live2D 模式開關（重啟生效）、視窗寬度（Batch A）
  - [x] 物理手感：落地弧形緩速、取消彈跳、飛行改直線（Batch C）
  - [x] 語音嘴型：說話時嘴巴跟 Hermes TTS 音量開合（見 Item 3）
  - [ ] 狀態 → 表情（Expression）
- [ ] L3：換上使用者自己的角色模型（需在 Cubism Editor 綁骨 / 現成分層素材）
- [ ] L4（遠期）：AI 輔助平面圖自動拆層綁骨

## 15b. 所有內建圖示經由皮膚 AI 重新生成 [ ]（原 Item 11，待皮膚 AI 品質穩定後執行）

## 16. 工具 AI 群架構 (Tool AIs) [ ]
- 原構想：每個工具 AI 一個獨立 `*client` 模組（仿 `aiclient`，放 `src/modules/`），`PetConfig` 統一開關；右鍵選單擴充「AI 工具」子選單
- **待重新評估**：上網 / 管家等工具能力目前改由 Hermes 的 toolset 提供（Item 5、6），桌寵端未必需要另寫 `*client`

## 17. iPhone 語音入口 [~]
- 分工：喚醒 / 開 App 交給 iPhone 捷徑，思考交給電腦上的 Hermes（手機只傳話）；iOS 不能常駐收音、不能操作別的 App
- [x] 「記一下」捷徑：聽寫 → 附加到備忘錄「隨手記」→ 通知（背面輕點 / 嘿 Siri）
- [x] Hermes `api_server` 本機打通：`127.0.0.1:8642`、Bearer key 驗證、`X-Hermes-Session-Id` 延續對話、對話寫入 `state.db`（聊天室看得到）
- [x] **定案路線：捷徑直連 Claude API + 電腦當信箱**（不經 iCloud）— 設定步驟見 `.planning/iphone_shortcut_setup.md`
  - [x] 捷徑「八千代」：聽寫 → Claude API（`claude-haiku-5-5`）→ 朗讀 → 存手機 `inbox.txt` → 送電腦 → 成功就刪
  - [x] `tools/note_receiver.py`：只綁 Tailscale IP `:39220`、Bearer token、只寫 `inbox.md` / 讀 `summary.md`（不碰 Hermes）
  - [x] 防火牆規則「YaChiYo note receiver (Tailscale)」；隨 `start/stop_yachiyo.ps1` 啟停
  - [x] 2026-10-26 端到端實測成功
  - [x] 開 App（回覆標記 `[開啟|App]` + 白名單「如果 → 打開 App」）、上網搜尋（`web_search` 伺服器工具）— 2026-10-27 實測成功
  - [x] 接收器開機自動啟動（`tools/start_note_receiver.ps1` + 啟動資料夾捷徑，等 Tailscale 就緒）
- [ ] 實測中文喚醒詞（iOS 18 語音快捷指令能否認「八千代」）
- [ ] Hermes 消化 `inbox.md` 進記憶，並寫 `summary.md`；捷徑送成功時 `GET /summary` 存手機、塞進 `system`
- [ ] （選）走 Hermes `api_server` 讓手機能叫電腦做事（需 `API_SERVER_HOST` 綁 Tailscale IP、gateway 常駐）
- 安全：入口必須驗證、`approvals: manual` 不放寬、不開公網；手機端不接 `/v1/runs`（持 key 可遠端核准危險指令）
