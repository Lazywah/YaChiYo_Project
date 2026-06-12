# YaChiYo 皮膚 AI 後端

以 **Stable Diffusion + ControlNet (Canny)** 為桌寵生成風格化皮膚。
ControlNet 從來源圖抽邊緣當骨架、配合固定 seed，讓整套序列幀在改變風格時**保持姿勢與角色一致**。

> 硬體需求：NVIDIA GPU（建議 ≥ 6GB VRAM）。本機環境為 RTX 4060 / 8GB，足夠。

---

## 0. 用 Docker（推薦）

完全隔離、不動本機 Python，且基底映像內建 CUDA 版 PyTorch + Python 3.11，
繞過本機 Python 3.13 的相容風險。

### 前置
- **Docker Desktop**（啟用 WSL2 後端）
- **NVIDIA 驅動**（已有；WSL2 會透過它讓 GPU 進容器，無需另裝 CUDA Toolkit）

### 啟動
```powershell
cd ai_server
docker compose up --build
```
首次會建置映像並在第一次推論時下載模型（數 GB，存進 `hf-cache` 卷，之後不再重複下載）。
看到 `Uvicorn running on http://0.0.0.0:8000` 即就緒。

### 驗證 GPU 有進容器
```powershell
docker compose run --rm yachiyo-ai python -c "import torch; print(torch.cuda.is_available())"
# 應印出 True
```

### 常用指令
```powershell
docker compose up -d        # 背景執行
docker compose logs -f      # 看日誌
docker compose down         # 停止
```

### 換模型
編輯 `docker-compose.yml` 的 `YACHIYO_MODEL`（HF repo id），或把 Civitai 的
`.safetensors` 放進 `ai_server/models/`（已掛載到容器 `/app/models`）後設為
`YACHIYO_MODEL=/app/models/your-model.safetensors`。

> 走 Docker 路線就**不需要**下面第 1～3 節（那是非 Docker 的本機安裝替代方案）。
> 端點、調校、VRAM 說明（第 4～6 節）兩種方式皆適用。

---

## 1. 安裝步驟（非 Docker 替代方案）

### (建議) 建立虛擬環境
```powershell
cd ai_server
python -m venv venv
.\venv\Scripts\Activate.ps1
```

### 安裝 CUDA 版 PyTorch（重要！不要用 requirements.txt 裝 torch）
你目前的 torch 是 CPU 版，必須換成 CUDA 版才能用上 GPU：
```powershell
pip uninstall -y torch torchvision
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124
```
驗證：
```powershell
python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0))"
# 應印出: True NVIDIA GeForce RTX 4060
```

### 安裝其餘套件
```powershell
pip install -r requirements.txt
```

---

## 2. 選擇基底模型（動漫風格）

預設模型在 `inference.py` 頂端的 `MODEL_ID`，也可用環境變數覆寫。兩種來源：

### A. HuggingFace 倉庫（首次執行自動下載，數 GB）
```powershell
# 預設值，無需設定。或改用其他動漫模型：
$env:YACHIYO_MODEL = "gsdf/Counterfeit-V2.5"
```

### B. Civitai 單檔 .safetensors（多數動漫模型的格式）
從 [Civitai](https://civitai.com) 下載 `.safetensors` 後，放到 `ai_server/models/` 並指定路徑：
```powershell
$env:YACHIYO_MODEL = "./models/your-anime-model.safetensors"
```

> ControlNet 模型 `lllyasviel/sd-controlnet-canny` 會自動下載（約 1.4GB），無需手動處理。

---

## 3. 啟動後端

```powershell
uvicorn inference:app --host 127.0.0.1 --port 8000
```

首次啟動會下載模型（數 GB，需等待）。看到 `Uvicorn running on http://127.0.0.1:8000` 即就緒。
桌寵程式的 AI 功能就會連到這個位址。

健康檢查：瀏覽器開 <http://127.0.0.1:8000/health> 應回傳 `{"status":"ok","device":"cuda",...}`。

---

## 4. 端點說明

| 端點 | 用途 | 輸入 | 輸出 |
|------|------|------|------|
| `POST /transform` | 單張變身（右鍵「AI 變身」） | `{ image, prompt, seed? }` | `{ result, seed }` |
| `POST /generate_skin` | 批次整套皮膚幀（固定 seed 保持一致） | `{ frames[], prompt, seed? }` | `{ results[], seed }` |
| `GET /health` | 健康檢查 | — | `{ status, device, model }` |

---

## 5. 調校（環境變數）

| 變數 | 預設 | 說明 |
|------|------|------|
| `YACHIYO_MODEL` | `stablediffusionapi/anything-v5` | 基底模型（HF repo 或本機 .safetensors） |
| `YACHIYO_STEPS` | `25` | 取樣步數（越高越精細但越慢） |
| `YACHIYO_GUIDANCE` | `7.5` | 提示詞遵循強度 |
| `YACHIYO_CANNY_LOW` / `_HIGH` | `100` / `200` | Canny 邊緣偵測閾值 |
| `YACHIYO_MAX_SIDE` | `768` | 生成時最長邊上限（影響 VRAM 與速度） |

---

## 6. VRAM 不足時（OOM）

`inference.py` 已預設開啟 attention/vae slicing。若 8GB 仍不夠：
1. 調低 `YACHIYO_MAX_SIDE`（如 `512`）
2. 取消 `inference.py` 中 `pipe.enable_model_cpu_offload()` 那行的註解（較慢但省 VRAM）

---

## 注意

- 本後端為**本機執行**，桌寵程式預設連 `http://127.0.0.1:8000`。
- 首次下載模型需要時間與磁碟空間（SD1.5 約 2GB + ControlNet 約 1.4GB）。
- 模型權重、虛擬環境、`models/` 已由 `.gitignore` 排除，不會進版控。
