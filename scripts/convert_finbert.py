#!/usr/bin/env python3
"""Convert snunlp/KR-FinBert-SC to ONNX for the C++ news-pipeline.

Usage:
    pip install transformers torch onnx
    python scripts/convert_finbert.py --out /opt/lqc/models/finbert

Outputs:
    <out>/kr-finbert.onnx   — graph
    <out>/kr-finbert.vocab  — vocab (line-per-token, special tokens first)
"""

import argparse
import os
from pathlib import Path

import torch
from transformers import AutoModelForSequenceClassification, AutoTokenizer

MODEL_ID = "snunlp/KR-FinBert-SC"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="output dir")
    ap.add_argument("--seq-len", type=int, default=128)
    ap.add_argument("--opset", type=int, default=14)
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    print(f"[1/3] downloading {MODEL_ID} …")
    tokenizer = AutoTokenizer.from_pretrained(MODEL_ID)
    model = AutoModelForSequenceClassification.from_pretrained(MODEL_ID)
    model.eval()

    print(f"[2/3] exporting ONNX (seq_len={args.seq_len}, opset={args.opset}) …")
    dummy_ids = torch.zeros((1, args.seq_len), dtype=torch.long)
    dummy_mask = torch.ones((1, args.seq_len), dtype=torch.long)
    onnx_path = out / "kr-finbert.onnx"
    torch.onnx.export(
        model,
        (dummy_ids, dummy_mask),
        str(onnx_path),
        input_names=["input_ids", "attention_mask"],
        output_names=["logits"],
        dynamic_axes={"input_ids":      {0: "batch"},
                      "attention_mask": {0: "batch"},
                      "logits":         {0: "batch"}},
        opset_version=args.opset,
        do_constant_folding=True,
    )
    print(f"      wrote {onnx_path} ({os.path.getsize(onnx_path)/1e6:.1f} MB)")

    print("[3/3] writing vocab (line-per-token) …")
    vocab_path = out / "kr-finbert.vocab"
    vocab = tokenizer.get_vocab()  # {token: id}
    by_id = sorted(vocab.items(), key=lambda kv: kv[1])
    with vocab_path.open("w", encoding="utf-8") as f:
        for tok, _ in by_id:
            f.write(tok + "\n")
    print(f"      wrote {vocab_path} ({len(by_id)} tokens)")

    label_map = {v: k for k, v in model.config.id2label.items()}
    print("\nLabel map (logit index → label):")
    for i in sorted(model.config.id2label):
        print(f"  {i}: {model.config.id2label[i]}")
    print("\nC++ FinBertOnnx assumes [negative, neutral, positive] order.")
    print("If above labels are different, edit FinBertOnnx.predict() softmax3 args.")


if __name__ == "__main__":
    main()
