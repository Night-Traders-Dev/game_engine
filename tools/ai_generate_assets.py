#!/usr/bin/env python3
"""
AI Pixel Art Asset Generator for Twilight Engine
Uses Stable Diffusion XL + Pixel Art LoRA for generating 2D game assets.

Usage:
    python3 tools/ai_generate_assets.py --prompt "a treasure chest, pixel art" --output chest.png
    python3 tools/ai_generate_assets.py --prompt "oak tree top-down view, pixel art" --size 64 --output tree.png
    python3 tools/ai_generate_assets.py --batch prompts.txt --outdir assets/textures/ai/
    python3 tools/ai_generate_assets.py --spritesheet "warrior character" --output warrior.png

Requirements: pip install torch diffusers transformers accelerate safetensors
GPU: NVIDIA with 6GB+ VRAM (RTX 4060 8GB recommended)
"""

import argparse
import os
import sys
import torch

MODEL_ID = "stabilityai/sdxl-turbo"  # 3.5GB, fast inference, fits 8GB VRAM
LORA_ID = "nerijs/pixel-art-xl"
CACHE_DIR = os.path.expanduser("~/.cache/twilight_ai_models")

def load_pipeline():
    """Load SDXL-Turbo + pixel art LoRA. Downloads on first run (~3.5GB).
    SDXL-Turbo is distilled for fast 1-4 step inference and fits in 8GB VRAM."""
    from diffusers import AutoPipelineForText2Image

    print(f"[AI] Loading SDXL-Turbo model...")
    pipe = AutoPipelineForText2Image.from_pretrained(
        MODEL_ID,
        torch_dtype=torch.float16,
        variant="fp16",
        use_safetensors=True,
        cache_dir=CACHE_DIR,
    )

    print(f"[AI] Loading pixel art LoRA...")
    try:
        pipe.load_lora_weights(LORA_ID, cache_dir=CACHE_DIR)
        print(f"[AI] LoRA loaded successfully")
    except Exception as e:
        print(f"[AI] Warning: LoRA failed ({e}), using base model with prompt engineering")

    # Use model CPU offloading to fit in 8GB VRAM with desktop compositor running
    pipe.enable_model_cpu_offload()
    pipe.enable_attention_slicing()

    print(f"[AI] Pipeline ready (GPU: {torch.cuda.get_device_name()}, VRAM: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f}GB, CPU offload enabled)")
    return pipe


def generate_image(pipe, prompt, size=512, steps=4, guidance=0.0, seed=None):
    """Generate a single image from a text prompt.
    SDXL-Turbo uses 1-4 steps with guidance_scale=0.0 (no CFG needed)."""
    generator = None
    if seed is not None:
        generator = torch.Generator("cuda").manual_seed(seed)

    # Prepend pixel art style keywords
    full_prompt = f"pixel art, {prompt}, game asset, clean lines, retro style, 16-bit, sprite"
    negative = "blurry, photorealistic, 3d render, text, watermark, signature, low quality, deformed"

    image = pipe(
        prompt=full_prompt,
        negative_prompt=negative,
        width=size,
        height=size,
        num_inference_steps=steps,
        guidance_scale=guidance,
        generator=generator,
    ).images[0]

    return image


def generate_spritesheet(pipe, subject, size=64, seed=42):
    """Generate a 3x3 sprite sheet (down/up/right x idle/walk0/walk1)."""
    from PIL import Image

    directions = ["facing down", "facing up", "facing right"]
    poses = ["standing idle", "walking left foot forward", "walking right foot forward"]

    sheet = Image.new("RGBA", (size * 3, size * 3), (0, 0, 0, 0))

    for row, direction in enumerate(directions):
        for col, pose in enumerate(poses):
            prompt = f"{subject}, {direction}, {pose}, pixel art sprite, game character, 32x32"
            img = generate_image(pipe, prompt, size=512, steps=20, seed=seed + row * 3 + col)
            img = img.resize((size, size), Image.LANCZOS)
            # Convert to RGBA
            if img.mode != "RGBA":
                img = img.convert("RGBA")
            sheet.paste(img, (col * size, row * size))
            print(f"  [{row},{col}] {direction} {pose}")

    return sheet


def main():
    parser = argparse.ArgumentParser(description="AI Pixel Art Asset Generator")
    parser.add_argument("--prompt", help="Text prompt for single image generation")
    parser.add_argument("--spritesheet", help="Generate 3x3 sprite sheet for a character subject")
    parser.add_argument("--batch", help="File with one prompt per line for batch generation")
    parser.add_argument("--output", "-o", default="output.png", help="Output file path")
    parser.add_argument("--outdir", default=".", help="Output directory for batch mode")
    parser.add_argument("--size", type=int, default=512, help="Image size in pixels (default 512)")
    parser.add_argument("--steps", type=int, default=25, help="Inference steps (default 25)")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility")
    parser.add_argument("--guidance", type=float, default=7.5, help="Guidance scale (default 7.5)")
    parser.add_argument("--download-only", action="store_true", help="Just download models, don't generate")
    args = parser.parse_args()

    # Load pipeline (downloads models on first run)
    pipe = load_pipeline()

    if args.download_only:
        print("[AI] Models downloaded successfully.")
        return

    if args.spritesheet:
        print(f"[AI] Generating 3x3 sprite sheet for: {args.spritesheet}")
        sheet = generate_spritesheet(pipe, args.spritesheet, size=args.size, seed=args.seed or 42)
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        sheet.save(args.output)
        print(f"[AI] Saved sprite sheet: {args.output} ({sheet.size[0]}x{sheet.size[1]})")

    elif args.batch:
        os.makedirs(args.outdir, exist_ok=True)
        with open(args.batch) as f:
            prompts = [line.strip() for line in f if line.strip() and not line.startswith("#")]
        print(f"[AI] Batch generating {len(prompts)} images...")
        for i, prompt in enumerate(prompts):
            name = prompt.replace(" ", "_").replace(",", "")[:40]
            outpath = os.path.join(args.outdir, f"{i:03d}_{name}.png")
            img = generate_image(pipe, prompt, args.size, args.steps, args.guidance,
                               seed=args.seed + i if args.seed else None)
            img.save(outpath)
            print(f"  [{i+1}/{len(prompts)}] {outpath}")
        print(f"[AI] Batch complete: {len(prompts)} images in {args.outdir}")

    elif args.prompt:
        print(f"[AI] Generating: {args.prompt}")
        img = generate_image(pipe, args.prompt, args.size, args.steps, args.guidance, args.seed)
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        img.save(args.output)
        print(f"[AI] Saved: {args.output} ({img.size[0]}x{img.size[1]})")

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
