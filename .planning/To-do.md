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

## 15. 桌寵皮膚 AI (Skin AI) [ ]
- 輸入角色原圖，自動生成整套狀態皮膚（含 Walking 序列幀），幀間風格一致
- 技術路線：Stable Diffusion + ControlNet (Canny) + 固定 seed；`aiclient` 增加批次幀 API
- 產出後自動存入 `resources/images/` 並重載 `animConfigs`
- 前置：GPU ≥ 6GB VRAM、`inference.py` 改造為 ControlNet pipeline

## 16. 工具 AI 群架構 (Tool AIs) [ ]
- 每個工具 AI 一個獨立 `*client` 模組（仿 `aiclient`，放 `src/modules/`），`PetConfig` 統一開關
- 右鍵選單擴充「AI 工具」子選單
- 涵蓋：遊戲助手、語言模型、AI 上網、AI 管家、程式碼審核員（Item 2~6、13 的架構基礎）