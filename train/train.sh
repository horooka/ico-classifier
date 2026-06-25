# Compose with ./images dir
mkdir -p synth-data
python compose-dataset.py

# Train
pip install -r requirements.txt
python train.py
