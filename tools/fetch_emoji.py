#!/usr/bin/env python3
"""
Télécharge un emoji depuis Twemoji par son codepoint Unicode et l'enregistre
en PNG dans assets/sprites/, prêt à être passé dans png_to_sprite.py
ou png_to_sprite_rgba.py.

Source : https://github.com/jdecked/twemoji (fork maintenu après l'arrêt
de Twitter, distribué via jsdelivr CDN). Format natif : 72×72 PNG RGBA.

Usage:
  python tools/fetch_emoji.py 2B50                       # ⭐ → assets/sprites/2b50_24.png
  python tools/fetch_emoji.py 2B50 --name star           # → assets/sprites/star_24.png
  python tools/fetch_emoji.py 2B50 --size 16             # redimensionne en 16×16
  python tools/fetch_emoji.py 2B50 --mono                # silhouette blanche colorisable
  python tools/fetch_emoji.py 1F469-200D-1F4BB           # emoji composite (femme + ordi)

Codepoints courants:
  ⭐ 2B50    ✨ 2728    ❗ 2757    ❓ 2753    💡 1F4A1
  ❤️  2764    💛 1F49B   💜 1F49C   🌟 1F31F   🎵 1F3B5
  💢 1F4A2   💤 1F4A4   🔥 1F525   ⚡ 26A1   ☀️  2600
  🍓 1F353   🍎 1F34E   🍕 1F355   🐱 1F431  🐶 1F436
"""

import argparse
import sys
import urllib.error
import urllib.request
from io import BytesIO
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Erreur: Pillow requis. Installe avec: pip install Pillow")
    sys.exit(1)

TWEMOJI_BASE = "https://cdn.jsdelivr.net/gh/jdecked/twemoji@latest/assets/72x72"


def normalize_codepoint(cp: str) -> str:
    """Normalise un codepoint en format Twemoji (lowercase, sans U+, tirets pour composite)."""
    cp = cp.strip().lower().replace("u+", "")
    # Twemoji utilise lowercase hex sans padding, séparé par tirets pour composite
    return cp


def fetch_emoji(codepoint: str) -> bytes:
    """Télécharge le PNG Twemoji pour le codepoint donné."""
    url = f"{TWEMOJI_BASE}/{codepoint}.png"
    print(f"Téléchargement: {url}")
    try:
        with urllib.request.urlopen(url, timeout=15) as response:
            return response.read()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            print(f"Erreur: codepoint '{codepoint}' introuvable sur Twemoji.")
            print("       Vérifie l'orthographe sur https://unicode.org/emoji/charts/full-emoji-list.html")
        else:
            print(f"Erreur HTTP {e.code}: {e.reason}")
        sys.exit(1)
    except Exception as e:
        print(f"Erreur réseau: {e}")
        sys.exit(1)


def to_silhouette(img: Image.Image) -> Image.Image:
    """Convertit une image RGBA en silhouette blanche (alpha conservé)."""
    img = img.convert("RGBA")
    pixels = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            _, _, _, a = pixels[x, y]
            pixels[x, y] = (255, 255, 255, a)
    return img


def main():
    parser = argparse.ArgumentParser(
        description="Télécharge un emoji depuis Twemoji et l'enregistre en PNG",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Exemples:\n"
               "  python tools/fetch_emoji.py 2B50 --name star --size 20\n"
               "  python tools/fetch_emoji.py 2728 --name sparkle --size 16 --mono"
    )
    parser.add_argument("codepoint",
                        help="Codepoint Unicode hex (ex: 2B50, U+2B50, 1F469-200D-1F4BB)")
    parser.add_argument("--name", default=None,
                        help="Nom de base du fichier (défaut: codepoint)")
    parser.add_argument("--size", type=int, default=24,
                        help="Taille en pixels carrée (défaut: 24)")
    parser.add_argument("--mono", action="store_true",
                        help="Convertit en silhouette blanche (pour png_to_sprite.py)")
    parser.add_argument("-o", "--output", default="assets/sprites",
                        help="Dossier de sortie (défaut: assets/sprites)")
    args = parser.parse_args()

    codepoint = normalize_codepoint(args.codepoint)
    base_name = args.name or codepoint.replace("-", "_")

    # Téléchargement
    png_bytes = fetch_emoji(codepoint)

    # Chargement
    img = Image.open(BytesIO(png_bytes)).convert("RGBA")
    src_w, src_h = img.size
    print(f"  Source: {src_w}×{src_h} RGBA")

    # Redimensionnement
    if args.size and (src_w != args.size or src_h != args.size):
        img = img.resize((args.size, args.size), Image.LANCZOS)
        print(f"  Redimensionné: {args.size}×{args.size}")

    # Silhouette mono
    if args.mono:
        img = to_silhouette(img)
        print("  Converti en silhouette blanche")

    # Sauvegarde
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = f"{base_name}_{args.size}.png"
    output_path = output_dir / filename
    img.save(output_path, "PNG")
    print(f"  -> {output_path}")
    print()
    print("Étape suivante (génération du header C) :")
    pipeline = "png_to_sprite.py" if args.mono else "png_to_sprite_rgba.py"
    print(f"  python tools/{pipeline} {output_path} \\")
    print("    -o src/models/gotchi/face/behavior/sprites/")


if __name__ == "__main__":
    main()
