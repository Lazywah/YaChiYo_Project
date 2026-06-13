# YaChiYo 桌寵專案 — 待實作功能清單

> 最後更新：2026-06-11

---

## 1. 圖生圖自開發 [ ]

## 2. 遊戲助手 AI 整合 [ ]

## 3. 語言模型整合 [ ]

## 4. 語言模型自開發 [ ]

## 5. AI 上網功能 [ ]

## 6. AI 管家化 [ ]

## 7. 音效系統 [ ]
- 整合 `QSoundEffect` 或 Qt Multimedia 模組
- 為落地、碰牆、拖曳、走路等互動加入音效
- 需準備對應的音效檔案

## 8. 完整國際化支援 (i18n) [ ]
- 導入 Qt Linguist 工具鏈
- 建立 `.ts` 翻譯檔
- 將所有 `tr()` 字串翻譯為英文 / 其他語言
- 目前已完成：程式碼中的雙語註解 (ZH: | EN:)

## 9. 疊層列表 (用於標示那些視窗不被桌寵覆蓋) [ ]

## 10. Hovering 狀態應加上 Sin wave [x]
- 已於 `updatePhysics()` Hovering case 實作（`PetPhysics::calcHoverY()`，振幅 8px）

## 11. 所有圖示應經由圖生圖 AI 重新生成 [ ]

## 12. 桌寵顯示方式應當用 (multi-Agent 並結合多個 AI 模組進行) 或 (anime-engine 方式進行) [ ]

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

## 15c. 會動的角色 — Live2D 路線 [ ]（設計藍圖見 `.planning/skin_animation_design.md`）
- 決策鏈：兩階段（外觀/動畫分離）→ 完全相同（變形而非生成）→ Live2D（業界標準）
- 前提（已接受）：需要綁好骨的 Live2D 模型，非平面圖；AI 僅輔助拆層/補背（遠期）
- L1 引擎整合（範例模型 + QOpenGLWidget + Cubism SDK，透明窗、眨眼/呼吸）
- L2 行為接線（狀態 → Live2D 動作；PetPhysics 管視窗、Live2D 管角色）
- L3 換上使用者自己的角色模型　L4（遠期）AI 輔助平面圖自動拆層綁骨
- L1 前置（使用者動作）：下載 Cubism SDK for Native + 範例模型、同意免費授權

## 15b. 所有內建圖示經由皮膚 AI 重新生成 [ ]（原 Item 11，待皮膚 AI 品質穩定後執行）

## 16. 工具 AI 群架構 (Tool AIs) [ ]
- 每個工具 AI 一個獨立 `*client` 模組（仿 `aiclient`，放 `src/modules/`），`PetConfig` 統一開關
- 右鍵選單擴充「AI 工具」子選單
- 涵蓋：遊戲助手、語言模型、AI 上網、AI 管家、程式碼審核員（Item 2~6、13 的架構基礎）