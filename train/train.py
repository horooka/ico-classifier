import os
import ast
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torchvision import transforms
from PIL import Image
from sklearn.metrics import f1_score

CSV_PATH = "synth-data/synth-data.csv"
IMG_ROOT = "synth-data"
BATCH_SIZE = 64
EPOCHS = 100
LR = 1e-3
IMG_SIZE = 32
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

def parse_list(v):
    if pd.isna(v) or str(v).strip() == "":
        return []
    s = str(v).strip()
    if s.startswith("["):
        try:
            out = ast.literal_eval(s)
            return [str(x).strip().lower() for x in out if str(x).strip()]
        except Exception:
            pass
    return [x.strip().lower() for x in s.split(";") if x.strip()]

class IconDataset(Dataset):
    def __init__(self, frame, root, base2idx, mod2idx):
        self.frame = frame.reset_index(drop=True)
        self.root = root
        self.base2idx = base2idx
        self.mod2idx = mod2idx
        self.tf = transforms.Compose([
            transforms.Resize((IMG_SIZE, IMG_SIZE)),
            transforms.ToTensor(),
        ])

    def __len__(self):
        return len(self.frame)

    def __getitem__(self, i):
        r = self.frame.iloc[i]
        path = os.path.join(self.root, str(r["path"]))
        img = Image.open(path).convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img).convert("RGB")
        x = self.tf(img)

        base_y = torch.tensor(self.base2idx[str(r["base"])], dtype=torch.long)
        mod_y = torch.zeros(len(self.mod2idx), dtype=torch.float32)
        for t in parse_list(r["mods"]):
            if t in self.mod2idx:
                mod_y[self.mod2idx[t]] = 1.0

        return x, base_y, mod_y, str(r.get("name", ""))

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

def main():
    df = pd.read_csv(CSV_PATH)
    for c in ["base", "mods", "split", "name"]:
        if c not in df.columns:
            df[c] = ""

    df["base"] = df["base"].fillna("").astype(str).str.lower().str.strip()
    df["mods"] = df["mods"].fillna("").astype(str)
    df["split"] = df["split"].fillna("train").astype(str).str.lower().str.strip()

    base_vocab = sorted(set(x for x in df["base"].tolist() if x))
    mod_vocab = sorted(set(t for mods in df["mods"] for t in parse_list(mods)))

    if not base_vocab or not mod_vocab:
        raise ValueError(f"Empty vocab. base_vocab={base_vocab}, mod_vocab={mod_vocab}")

    base2idx = {b: i for i, b in enumerate(base_vocab)}
    mod2idx = {m: i for i, m in enumerate(mod_vocab)}

    train_df = df[df["split"] == "train"].reset_index(drop=True)
    val_df = df[df["split"] == "val"].reset_index(drop=True)

    train_loader = DataLoader(
        IconDataset(train_df, IMG_ROOT, base2idx, mod2idx),
        batch_size=BATCH_SIZE,
        shuffle=True,
        num_workers=0,
        pin_memory=False,
    )

    val_loader = DataLoader(
        IconDataset(val_df, IMG_ROOT, base2idx, mod2idx),
        batch_size=BATCH_SIZE,
        shuffle=False,
        num_workers=0,
        pin_memory=False,
    )

    model = SmallCNN(len(base_vocab), len(mod_vocab)).to(DEVICE)
    ce_loss = nn.CrossEntropyLoss()
    bce_loss = nn.BCEWithLogitsLoss()
    optimizer = optim.AdamW(model.parameters(), lr=LR)

    for epoch in range(EPOCHS):
        model.train()
        for x, base_y, mod_y, _ in train_loader:
            x = x.to(DEVICE)
            base_y = base_y.to(DEVICE)
            mod_y = mod_y.to(DEVICE)

            optimizer.zero_grad()
            base_logits, mod_logits = model(x)
            loss = ce_loss(base_logits, base_y) + bce_loss(mod_logits, mod_y)
            loss.backward()
            optimizer.step()

        print(f"epoch={epoch+1} done")

    torch.save(
    {
        "model_state": model.state_dict(),
        "base_vocab": base_vocab,
        "mod_vocab": mod_vocab,
        "base2idx": base2idx,
        "mod2idx": mod2idx,
        "img_size": IMG_SIZE,
    },
    "output/ico-classifier.pt")


if __name__ == "__main__":
    main()
