# YaChiYo 皮膚格式說明 (Skin Format)

> 本文件說明桌寵皮膚的資料夾結構與 `skin.json` 格式。
> 適用對象：手動製作皮膚的使用者、以及未來自動生成皮膚的 AI（To-do Item 15）。

---

## 皮膚從哪裡載入

程式啟動時會掃描兩個位置，列出所有「含 `skin.json` 的資料夾」作為可選皮膚：

| 來源 | 路徑 | 說明 |
|------|------|------|
| 內建 | `:/res/skins/`（編譯進執行檔） | 隨程式發布，例如 `default` |
| 使用者 | `<執行檔同層>/skins/` | 自行新增的皮膚放這裡 |

> 同名（資料夾名相同）時，**使用者皮膚會覆蓋內建皮膚**。
> 切換皮膚：設定中心 → 外觀設定 → 「皮膚」下拉選單。選擇會被記住（下次啟動自動套用）。

---

## 資料夾結構

一套皮膚 = 一個資料夾，資料夾名即為皮膚的唯一 id：

```
my_skin/
├── skin.json              ← 必須，描述各狀態
├── Standing.png           ← png 型狀態：<狀態名>.png
├── Captured.gif           ← gif 型狀態：<狀態名>.gif
└── Walking/               ← frames 型狀態：<狀態名>/ 子資料夾
    ├── Walking-1.png       ←   檔名格式：<狀態名>-<編號>.png（編號從 1 起）
    ├── Walking-2.png
    └── ...（共 frames 張）
```

---

## skin.json 格式

```json
{
  "name": "顯示名稱",
  "author": "作者",
  "scale": 250,
  "states": {
    "Standing":      { "type": "png" },
    "Walking":       { "type": "frames", "frames": 6, "interval": 150 },
    "Captured":      { "type": "gif" },
    "Hovering":      { "type": "png", "fallback": "Standing" },
    "Flying":        { "type": "png", "fallback": "Standing" },
    "AI_Processing": { "type": "png", "fallback": "Standing" }
  }
}
```

### 欄位說明

| 欄位 | 說明 |
|------|------|
| `name` | 顯示在皮膚下拉選單的名稱 |
| `author` | 作者（選填，目前僅記錄用） |
| `scale` | 基準顯示像素（預設 250）；實際大小 = `scale × 設定中心的桌寵大小%` |
| `states` | 各狀態的呈現方式（見下） |

### 狀態 (states) 的 type

| type | 對應檔案 | 額外欄位 |
|------|---------|---------|
| `png` | `<狀態名>.png` | — |
| `gif` | `<狀態名>.gif` | 需設定中心「GIF 動畫皮膚」開啟才會播放 |
| `frames` | `<狀態名>/<狀態名>-N.png` | `frames`（總幀數）、`interval`（每幀毫秒） |

### fallback

某狀態若沒有自己的圖，可用 `"fallback": "狀態名"` 指定退回顯示哪個狀態的圖。
程式的解析順序為：**序列幀 → 該狀態 png → fallback 狀態 png → 最終退回 `Standing`**。
因此一套皮膚**至少要有 `Standing.png`**，其餘狀態都可以靠 fallback 共用它。

---

## 六個狀態

桌寵狀態機共有六個狀態，皮膚可為每個狀態提供圖：

| 狀態名 | 時機 |
|--------|------|
| `Standing` | 站立（必備） |
| `Walking` | 行走（建議用 frames 做序列幀動畫） |
| `Flying` | 飛往目標位置 |
| `Hovering` | 空中浮動 |
| `Captured` | 被滑鼠拖曳中 |
| `AI_Processing` | AI 變身處理中 |

---

## 最小皮膚範例

只要一張圖也能成為一套皮膚——其餘狀態全部 fallback 到 `Standing`：

```
minimal/
├── skin.json
└── Standing.png
```

```json
{
  "name": "極簡皮膚",
  "scale": 250,
  "states": {
    "Standing":      { "type": "png" },
    "Walking":       { "type": "png", "fallback": "Standing" },
    "Flying":        { "type": "png", "fallback": "Standing" },
    "Hovering":      { "type": "png", "fallback": "Standing" },
    "Captured":      { "type": "png", "fallback": "Standing" },
    "AI_Processing": { "type": "png", "fallback": "Standing" }
  }
}
```

---

## 給 AI 生成皮膚的備註（Item 15）

未來的皮膚 AI 應直接**輸出符合本格式的資料夾**：
1. 以一套基底皮膚（如 `default`）的每幀為條件，用 ControlNet (Canny) + 固定 seed 保持幀間一致
2. 依本文件的檔名慣例輸出圖片
3. 自動產生對應的 `skin.json`
4. 寫入 `<執行檔>/skins/<新皮膚 id>/`，重開程式即出現在下拉選單

如此「AI 生成」與「手動製作」產出的格式完全相同，使用層無需區分。
