# -*- coding: utf-8 -*-
"""
YaChiYo 皮膚 AI 後端 (FastAPI + SD1.5 + ControlNet Canny + IP-Adapter)

設計 / Design:
  - ControlNet Canny  : 從「姿勢來源圖」抽邊緣當骨架 (保持姿勢)
  - IP-Adapter        : 從「參考圖」取角色身份/風格 (讓產物像你上傳的角色)
  - Negative prompt   : 抑制動漫 SD1.5 常見的崩壞 (多手指/扭曲臉/壞解剖)
  - 固定 seed         : 整套幀共用同一 seed，保持一致性

端點 / Endpoints:
  POST /generate_skin  批次整套皮膚                    { frames[], reference?, prompt, negative_prompt?, seed?, ip_scale? } -> { results[], seed }
  GET  /health         健康檢查

啟動 / Run:
  uvicorn inference:app --host 0.0.0.0 --port 8000
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
from transformers import CLIPVisionModelWithProjection
from diffusers import (
    StableDiffusionControlNetPipeline,
    ControlNetModel,
    UniPCMultistepScheduler,
)

# =============================================================================
# ZH: 設定 (可用環境變數覆寫) | EN: Configuration (overridable via env vars)
# =============================================================================

MODEL_ID      = os.environ.get("YACHIYO_MODEL", "stablediffusionapi/anything-v5")
CONTROLNET_ID = os.environ.get("YACHIYO_CONTROLNET", "lllyasviel/sd-controlnet-canny")
IPADAPTER_ID  = os.environ.get("YACHIYO_IPADAPTER", "h94/IP-Adapter")

STEPS      = int(os.environ.get("YACHIYO_STEPS", "28"))
GUIDANCE   = float(os.environ.get("YACHIYO_GUIDANCE", "7.5"))
IP_SCALE   = float(os.environ.get("YACHIYO_IP_SCALE", "0.7"))   # ZH: 身份還原強度 (越高越像參考圖) | EN: identity strength
CANNY_LOW  = int(os.environ.get("YACHIYO_CANNY_LOW", "100"))
CANNY_HIGH = int(os.environ.get("YACHIYO_CANNY_HIGH", "200"))
MAX_SIDE   = int(os.environ.get("YACHIYO_MAX_SIDE", "640"))

# ZH: 預設負面提示詞 — 抑制崩壞，是動漫 SD1.5 出好圖的關鍵 | EN: default negative prompt (critical for anime SD1.5)
NEGATIVE_DEFAULT = os.environ.get(
    "YACHIYO_NEGATIVE",
    "lowres, worst quality, low quality, jpeg artifacts, blurry, "
    "bad anatomy, bad hands, missing fingers, extra digit, fewer digits, extra limbs, "
    "malformed limbs, fused fingers, mutated, deformed, disfigured, ugly, "
    "extra arms, extra legs, cropped, watermark, text, signature",
)

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
DTYPE  = torch.float16 if DEVICE == "cuda" else torch.float32

# =============================================================================
# ZH: 載入模型 (啟動時一次) | EN: Load models once at startup
# =============================================================================

print(f"[YaChiYo] device={DEVICE} dtype={DTYPE} model={MODEL_ID}")

controlnet = ControlNetModel.from_pretrained(CONTROLNET_ID, torch_dtype=DTYPE)

# ZH: IP-Adapter 需要的影像編碼器 (CLIP vision) | EN: CLIP vision encoder required by IP-Adapter
image_encoder = CLIPVisionModelWithProjection.from_pretrained(
    IPADAPTER_ID, subfolder="models/image_encoder", torch_dtype=DTYPE
)

if MODEL_ID.endswith((".safetensors", ".ckpt")):
    pipe = StableDiffusionControlNetPipeline.from_single_file(
        MODEL_ID, controlnet=controlnet, image_encoder=image_encoder,
        torch_dtype=DTYPE, safety_checker=None,
    )
else:
    pipe = StableDiffusionControlNetPipeline.from_pretrained(
        MODEL_ID, controlnet=controlnet, image_encoder=image_encoder,
        torch_dtype=DTYPE, safety_checker=None,
    )

pipe.scheduler = UniPCMultistepScheduler.from_config(pipe.scheduler.config)

# ZH: 掛載 IP-Adapter | EN: attach IP-Adapter
pipe.load_ip_adapter(IPADAPTER_ID, subfolder="models", weight_name="ip-adapter_sd15.bin")
pipe.set_ip_adapter_scale(IP_SCALE)

pipe = pipe.to(DEVICE)
if DEVICE == "cuda":
    # ZH: 注意！不可用 enable_attention_slicing() — 它會覆蓋 IP-Adapter 的注意力處理器，
    #     造成 "'tuple' object has no attribute 'shape'"。vae_slicing 不影響注意力，安全。
    # EN: Do NOT use enable_attention_slicing() — it overwrites the IP-Adapter attention
    #     processors and breaks with "'tuple' object has no attribute 'shape'". vae_slicing is safe.
    pipe.enable_vae_slicing()
    # ZH: 若 VRAM 不足 (OOM)，改用下行 (與 IP-Adapter 相容) | EN: if OOM, use this (IP-Adapter compatible)
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
    """ZH: 生成尺寸：最長邊 <= MAX_SIDE 且為 8 的倍數 | EN: gen size capped to MAX_SIDE, multiples of 8."""
    scale = min(1.0, MAX_SIDE / max(w, h))
    gw = max(8, int(round(w * scale / 8)) * 8)
    gh = max(8, int(round(h * scale / 8)) * 8)
    return gw, gh


def make_canny(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    arr = np.array(img.resize(size))
    edges = cv2.Canny(arr, CANNY_LOW, CANNY_HIGH)
    edges = np.stack([edges] * 3, axis=-1)
    return Image.fromarray(edges)


def restyle(pose: Image.Image, prompt: str, seed: int,
            reference: Image.Image, negative: str) -> Image.Image:
    """ZH: 以 ControlNet Canny (姿勢) + IP-Adapter (身份) 重繪一張
       EN: Restyle one image: pose from ControlNet Canny, identity from IP-Adapter."""
    w, h = pose.size
    gw, gh = gen_size(w, h)
    control = make_canny(pose, (gw, gh))
    generator = torch.Generator(device=DEVICE).manual_seed(seed)
    out = pipe(
        prompt=prompt,
        negative_prompt=negative,
        image=control,
        ip_adapter_image=reference,
        num_inference_steps=STEPS,
        guidance_scale=GUIDANCE,
        generator=generator,
    ).images[0]
    return out.resize((w, h))


def pick_seed(data: dict) -> int:
    if data.get("seed") is not None:
        return int(data["seed"])
    return random.randint(0, 2**31 - 1)

# =============================================================================
# ZH: 端點 | EN: Endpoints
# =============================================================================

@app.post("/generate_skin")
async def generate_skin(data: dict = Body(...)):
    """ZH: 批次生成整套皮膚。有 reference 時用它當身份，否則以各幀自身為身份。
       EN: Batch skin generation. Uses 'reference' as identity if given, else each frame itself."""
    seed = pick_seed(data)
    prompt = data["prompt"]
    neg = data.get("negative_prompt") or NEGATIVE_DEFAULT

    if "ip_scale" in data and data["ip_scale"] is not None:
        pipe.set_ip_adapter_scale(float(data["ip_scale"]))

    ref = b64_to_pil(data["reference"]) if data.get("reference") else None

    results = []
    for f in data["frames"]:
        pose = b64_to_pil(f)
        result = restyle(pose, prompt, seed, reference=(ref if ref is not None else pose), negative=neg)
        results.append(pil_to_b64(result))

    pipe.set_ip_adapter_scale(IP_SCALE)   # ZH: 還原預設 | EN: restore default
    return {"results": results, "seed": seed}


@app.get("/health")
async def health():
    return {"status": "ok", "device": DEVICE, "model": MODEL_ID}
