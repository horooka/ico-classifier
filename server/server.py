import io
from typing import List, Optional

import torch
import torch.nn as nn
from fastapi import FastAPI, File, UploadFile, Form, HTTPException
from PIL import Image
from torchvision import transforms

CHECKPOINT = "ico-classifier.pt"
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

app = FastAPI(title="Icon Classifier API")

class SmallCNN(nn.Module):
    def __init__(self, num_base, num_mod):
        super().__init__()
        self.backbone = nn.Sequential(
            nn.Conv2d(3, 32, 3, padding=1),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),
            nn.Conv2d(32, 64, 3, padding=1),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),
            nn.Conv2d(64, 128, 3, padding=1),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),
            nn.Conv2d(128, 256, 3, padding=1),
            nn.ReLU(inplace=True),
            nn.AdaptiveAvgPool2d(1),
        )
        self.base_head = nn.Linear(256, num_base)
        self.mod_head = nn.Linear(256, num_mod)

    def forward(self, x):
        f = self.backbone(x).flatten(1)
        return self.base_head(f), self.mod_head(f)

ckpt = torch.load(CHECKPOINT, map_location=DEVICE, weights_only=True)
base_vocab = ckpt["base_vocab"]
mod_vocab = ckpt["mod_vocab"]
IMG_SIZE = ckpt["img_size"]

model = SmallCNN(len(base_vocab), len(mod_vocab)).to(DEVICE)
model.load_state_dict(ckpt["model_state"])
model.eval()

tf = transforms.Compose([
    transforms.Resize((IMG_SIZE, IMG_SIZE)),
    transforms.ToTensor(),
])

def preprocess_image(data: bytes) -> torch.Tensor:
    img = Image.open(io.BytesIO(data)).convert("RGBA")
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, img).convert("RGB")
    x = tf(img).unsqueeze(0).to(DEVICE)
    return x

def infer_one(data: bytes):
    x = preprocess_image(data)
    with torch.no_grad():
        base_logits, mod_logits = model(x)
        base_prob = torch.softmax(base_logits, dim=1)[0]
        mod_prob = torch.sigmoid(mod_logits)[0]

    base_idx = int(base_prob.argmax().item())
    base_name = base_vocab[base_idx]
    base_conf = float(base_prob[base_idx].item())

    mod_pairs = []
    for i, name in enumerate(mod_vocab):
        p = float(mod_prob[i].item())
        if p >= 0.5:
            mod_pairs.append({"name": name, "prob": p})
    mod_pairs.sort(key=lambda x: x["prob"], reverse=True)

    top3_idx = base_prob.topk(min(3, len(base_vocab))).indices.tolist()
    top3 = [{"name": base_vocab[i], "prob": float(base_prob[i].item())} for i in top3_idx]

    return {
        "base": {"name": base_name, "confidence": base_conf},
        "modifiers": mod_pairs,
        "top3_base": top3,
    }

@app.get("/health")
def health():
    return {"status": "ok", "device": DEVICE}

@app.post("/infer")
async def infer(
    files: List[UploadFile] = File(...),
    names: Optional[str] = Form(None),
):
    if not files:
        raise HTTPException(status_code=400, detail="No files uploaded")

    custom_names = []
    if names:
        custom_names = [x.strip() for x in names.split(",")]

    results = []
    for i, f in enumerate(files):
        data = await f.read()
        label = custom_names[i] if i < len(custom_names) else f.filename
        try:
            pred = infer_one(data)
            results.append({"file": label, "ok": True, "result": pred})
        except Exception as e:
            results.append({"file": label, "ok": False, "error": str(e)})

    return {"count": len(results), "results": results}
