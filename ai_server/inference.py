# -*- coding: utf-8 -*-
"""
YaChiYo 皮膚 AI 後端 (FastAPI + Stable Diffusion + ControlNet Canny)

ZH: 以 ControlNet Canny 從來源圖抽邊緣當骨架，配合固定 seed，讓整套序列幀
    在改變風格的同時保持姿勢與角色一致性 (避免每幀長得不一樣)。
EN: Uses ControlNet Canny to keep pose/identity consistent across a frame set while
    restyling, by extracting edges as the control map and reusing a fixed seed.

端點 / Endpoints:
  POST /transform      單張變身 (給右鍵「AI 變身」用)   { image, prompt, seed? } -> { result }
  POST /generate_skin  批次整套皮膚幀 (固定 seed 保持一致) { frames[], prompt, seed? } -> { results[], seed }

啟動 / Run:
  uvicorn inference:app --host 127.0.0.1 --port 8000
"""

import os
import io
import base64
import random

import numpy as np
import cv2
from PIL import Image

import torch
from fastapi import FastAPI, Body
from diffusers import (
    StableDiffusionControlNetPipeline,
    ControlNetModel,
    UniPCMultistepScheduler,
)

# =============================================================================
# ZH: 設定 (可用環境變數覆寫) | EN: Configuration (overridable via env vars)
# =============================================================================

# ZH: 基底模型 — 可為 HuggingFace repo id，或本機單檔 .safetensors (Civitai 常見格式)
# EN: Base model — a HuggingFace repo id, or a local single-file .safetensors (common on Civitai)
#   動漫推薦 / anime picks: "stablediffusionapi/anything-v5"、"gsdf/Counterfeit-V2.5"
#   或下載 Civitai 的 .safetensors 後設為其路徑 (例: "./models/anime.safetensors")
MODEL_ID      = os.environ.get("YACHIYO_MODEL", "stablediffusionapi/anything-v5")
CONTROLNET_ID = os.environ.get("YACHIYO_CONTROLNET", "lllyasviel/sd-controlnet-canny")

# ZH: 生成參數 | EN: Generation params
STEPS          = int(os.environ.get("YACHIYO_STEPS", "25"))
GUIDANCE       = float(os.environ.get("YACHIYO_GUIDANCE", "7.5"))
CANNY_LOW      = int(os.environ.get("YACHIYO_CANNY_LOW", "100"))
CANNY_HIGH     = int(os.environ.get("YACHIYO_CANNY_HIGH", "200"))
MAX_SIDE       = int(os.environ.get("YACHIYO_MAX_SIDE", "768"))   # ZH: 生成時最長邊上限 | EN: cap longest side at generation time

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
DTYPE  = torch.float16 if DEVICE == "cuda" else torch.float32

# =============================================================================
# ZH: 載入模型 (啟動時一次) | EN: Load models once at startup
# =============================================================================

print(f"[YaChiYo] device={DEVICE} dtype={DTYPE} model={MODEL_ID}")

controlnet = ControlNetModel.from_pretrained(CONTROLNET_ID, torch_dtype=DTYPE)

if MODEL_ID.endswith((".safetensors", ".ckpt")):
    # ZH: 本機單檔模型 (Civitai) | EN: local single-file checkpoint (Civitai)
    pipe = StableDiffusionControlNetPipeline.from_single_file(
        MODEL_ID, controlnet=controlnet, torch_dtype=DTYPE, safety_checker=None
    )
else:
    # ZH: HuggingFace diffusers 倉庫 | EN: HuggingFace diffusers repo
    pipe = StableDiffusionControlNetPipeline.from_pretrained(
        MODEL_ID, controlnet=controlnet, torch_dtype=DTYPE, safety_checker=None
    )

pipe.scheduler = UniPCMultistepScheduler.from_config(pipe.scheduler.config)
pipe = pipe.to(DEVICE)

# ZH: 8GB VRAM 省記憶體設定 | EN: memory savers for 8GB VRAM
if DEVICE == "cuda":
    pipe.enable_attention_slicing()
    pipe.enable_vae_slicing()
    # ZH: 若仍 OOM，取消下行註解改用 CPU offload (較慢) | EN: uncomment if OOM (slower)
    # pipe.enable_model_cpu_offload()

app = FastAPI()

# =============================================================================
# ZH: 工具函式 | EN: Helpers
# =============================================================================

def b64_to_pil(s: str) -> Image.Image:
    return Image.open(io.BytesIO(base64.b64decode(s))).convert("RGB")


def pil_to_b64(img: Image.Image) -> str:
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return base64.b64encode(buf.getvalue()).decode()


def gen_size(w: int, h: int) -> tuple[int, int]:
    """ZH: 計算生成尺寸：縮到最長邊 <= MAX_SIDE 且寬高為 8 的倍數
       EN: Generation size: cap longest side at MAX_SIDE, round to multiples of 8."""
    scale = min(1.0, MAX_SIDE / max(w, h))
    gw = max(8, int(round(w * scale / 8)) * 8)
    gh = max(8, int(round(h * scale / 8)) * 8)
    return gw, gh


def make_canny(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    """ZH: 產生 Canny 邊緣控制圖 | EN: Build the Canny edge control image."""
    resized = img.resize(size)
    arr = np.array(resized)
    edges = cv2.Canny(arr, CANNY_LOW, CANNY_HIGH)
    edges = np.stack([edges] * 3, axis=-1)   # ZH: 單通道轉三通道 | EN: 1ch -> 3ch
    return Image.fromarray(edges)


def restyle(img: Image.Image, prompt: str, seed: int) -> Image.Image:
    """ZH: 單張圖以 ControlNet Canny 重繪 | EN: Restyle one image via ControlNet Canny."""
    w, h = img.size
    gw, gh = gen_size(w, h)
    control = make_canny(img, (gw, gh))
    generator = torch.Generator(device=DEVICE).manual_seed(seed)
    out = pipe(
        prompt=prompt,
        image=control,
        num_inference_steps=STEPS,
        guidance_scale=GUIDANCE,
        generator=generator,
    ).images[0]
    return out.resize((w, h))   # ZH: 還原為原始尺寸 | EN: back to original size


def pick_seed(data: dict) -> int:
    if "seed" in data and data["seed"] is not None:
        return int(data["seed"])
    return random.randint(0, 2**31 - 1)

# =============================================================================
# ZH: 端點 | EN: Endpoints
# =============================================================================

@app.post("/transform")
async def transform(data: dict = Body(...)):
    """ZH: 單張變身 (給右鍵「AI 變身」) | EN: Single-image restyle (right-click "AI Transform")."""
    seed = pick_seed(data)
    img = b64_to_pil(data["image"])
    result = restyle(img, data["prompt"], seed)
    return {"result": pil_to_b64(result), "seed": seed}


@app.post("/generate_skin")
async def generate_skin(data: dict = Body(...)):
    """ZH: 批次生成整套皮膚幀，全部用同一 seed 以保持風格一致
       EN: Batch-generate a whole frame set, all with the same seed for consistency."""
    seed = pick_seed(data)
    prompt = data["prompt"]
    results = [pil_to_b64(restyle(b64_to_pil(f), prompt, seed)) for f in data["frames"]]
    return {"results": results, "seed": seed}


@app.get("/health")
async def health():
    """ZH: 健康檢查 (供 Qt 端確認後端就緒) | EN: Health check (Qt can verify the backend is up)."""
    return {"status": "ok", "device": DEVICE, "model": MODEL_ID}
