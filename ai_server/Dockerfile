# YaChiYo 皮膚 AI 後端 — Docker 映像
# 基底：PyTorch 官方 CUDA 映像 (內建 torch + torchvision + CUDA runtime + Python 3.11)
#       直接繞過本機 Python 3.13 的相容問題，也不需手動安裝 CUDA 版 PyTorch。
FROM pytorch/pytorch:2.5.1-cuda12.4-cudnn9-runtime

WORKDIR /app

# ZH: opencv 在執行期需要的系統函式庫 | EN: system libs required by opencv at runtime
RUN apt-get update && \
    apt-get install -y --no-install-recommends libgl1 libglib2.0-0 && \
    rm -rf /var/lib/apt/lists/*

# ZH: 先裝相依 (torch/torchvision 已在基底映像，requirements.txt 不含它們)
# EN: install deps first (torch/torchvision already in base image; not in requirements.txt)
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# ZH: 複製後端程式 | EN: copy the backend
COPY inference.py .

EXPOSE 8000

# ZH: 綁 0.0.0.0 讓容器外 (host) 可連入；host 透過 port mapping 對應到 127.0.0.1
# EN: bind 0.0.0.0 so the host can reach it; host maps it to 127.0.0.1 via port mapping
CMD ["uvicorn", "inference:app", "--host", "0.0.0.0", "--port", "8000"]
