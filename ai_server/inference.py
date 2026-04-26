from fastapi import FastAPI, Body
import torch
from diffusers import StableDiffusionImg2ImgPipeline
import base64, io
from PIL import Image

app = FastAPI()

# ZH: 預先載入模型 | EN: Preload the model
pipe = StableDiffusionImg2ImgPipeline.from_pretrained(
    "./models/stable-diffusion-v1-5", 
    torch_dtype=torch.float16
).to("cuda")

@app.post("/generate")
async def generate(data: dict = Body(...)):
    # ZH: 接收 Base64 圖片並轉換為 AI 可讀格式 | EN: Receive Base64 images and convert them into an AI-readable format
    init_img = Image.open(io.BytesIO(base64.b64decode(data['image']))).convert("RGB")
    
    # ZH: 執行圖生圖 (Strength 決定與原圖的相似度，0.6 是一個平衡點) | EN: The generated graph is produced by applying the Strength property to the original graph; 0.6 is a reasonable balance
    image = pipe(prompt=data['prompt'], image=init_img, strength=0.6).images[0]
    
    # ZH: 將結果轉回 Base64 傳回 Qt | EN: Convert the result back to Base64 and send it back to Qt
    buf = io.BytesIO()
    image.save(buf, format="PNG")
    return {"result": base64.b64encode(buf.getvalue()).decode()}