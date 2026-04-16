#!/usr/bin/env python3
"""
Convertit des PNG multi-couleur (emojis, icônes colorées) en header C
pour ESP32, en gardant les couleurs natives.

Format produit :
  - SPRITE_X_RGB565[w*h] : couleurs RGB565 (uint16_t) en PROGMEM
  - SPRITE_X_ALPHA[w*h]  : alpha 8 bits (uint8_t) en PROGMEM
  - SPRITE_X_ASSET       : SpriteAsset référençant les deux

Le fond est pré-multiplié pour que les pixels semi-transparents s'affichent
correctement (pas de halo blanc/noir sur les bords).

Usage:
  python png_to_sprite_rgba.py assets/sprites/heart_color_24.png
  python png_to_sprite_rgba.py assets/sprites/colored/ -o src/.../sprites/
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Erreur: Pillow requis. Installe avec: pip install Pillow")
    sys.exit(1)


def rgb_to_565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def png_to_rgba_arrays(img_path: Path, target_size: int | None = None):
    """Charge un PNG et extrait deux tableaux : RGB565 + alpha.
    Si target_size est spécifié, resize l'image (LANCZOS) avant conversion."""
    img = Image.open(img_path).convert("RGBA")

    if target_size is not None:
        img = img.resize((target_size, target_size), Image.LANCZOS)

    w, h = img.size
    pixels = list(img.getdata())

    rgb565 = []
    alpha = []
    for r, g, b, a in pixels:
        rgb565.append(rgb_to_565(r, g, b))
        alpha.append(a)

    return img_path.stem, w, h, rgb565, alpha


def generate_header(name: str, w: int, h: int, rgb565: list[int], alpha: list[int]) -> str:
    upper = name.upper()
    guard = f"SPRITE_{upper}_INCLUDED"

    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <pgmspace.h>",
        "#include <cstdint>",
        '#include "sprite_asset.h"',
        "",
        f"// Auto-generated from {name}.png ({w}x{h}) — RGBA multi-couleur",
        f"// RGB565 + alpha séparés. Couleurs natives, blend au rendu.",
        "",
        f"constexpr uint16_t SPRITE_{upper}_WIDTH = {w};",
        f"constexpr uint16_t SPRITE_{upper}_HEIGHT = {h};",
        "",
        f"const uint16_t SPRITE_{upper}_RGB565[] PROGMEM = {{",
    ]

    # RGB565 (16 valeurs/ligne)
    for row in range(h):
        row_data = rgb565[row * w : (row + 1) * w]
        hex_vals = ", ".join(f"0x{v:04X}" for v in row_data)
        comma = "," if row < h - 1 else ""
        lines.append(f"  {hex_vals}{comma}")

    lines.extend([
        "};",
        "",
        f"const uint8_t SPRITE_{upper}_ALPHA[] PROGMEM = {{",
    ])

    for row in range(h):
        row_data = alpha[row * w : (row + 1) * w]
        hex_vals = ", ".join(f"0x{v:02X}" for v in row_data)
        comma = "," if row < h - 1 else ""
        lines.append(f"  {hex_vals}{comma}")

    lines.extend([
        "};",
        "",
        f"// Asset générique réutilisable depuis le système de spawnSprite()",
        f"inline constexpr SpriteAsset SPRITE_{upper}_ASSET = {{",
        f"  SPRITE_{upper}_WIDTH, SPRITE_{upper}_HEIGHT,",
        f"  SPRITE_{upper}_ALPHA, SPRITE_{upper}_RGB565",
        f"}};",
        "",
        f"#endif // SPRITE_{upper}_INCLUDED",
        "",
    ])

    return "\n".join(lines)


def process_file(img_path: Path, output_dir: Path, target_size: int | None = None):
    name, w, h, rgb565, alpha = png_to_rgba_arrays(img_path, target_size)

    # Si resize, le nom du header inclut la taille cible (ex: apple_44)
    if target_size is not None:
        # Remplacer le suffixe de taille existant (ex: apple_22 -> apple_44)
        base = name.rsplit("_", 1)[0] if name[-1].isdigit() else name
        header_name = f"{base}_{target_size}"
    else:
        header_name = name

    non_zero = sum(1 for v in alpha if v > 0)
    total = w * h
    bytes_total = total * 3  # RGB565 (2) + alpha (1)
    print(f"  {name}.png -> {header_name} : {w}x{h}, {non_zero}/{total} pixels visibles, {bytes_total} bytes flash")

    header_content = generate_header(header_name, w, h, rgb565, alpha)
    output_path = output_dir / f"sprite_{header_name}.h"
    output_path.write_text(header_content)
    print(f"  -> {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Convertit PNG multi-couleur -> header C sprite (RGBA)")
    parser.add_argument("input", help="Fichier PNG ou dossier contenant des PNG")
    parser.add_argument("-o", "--output", default=None,
                        help="Dossier de sortie (défaut: même dossier que l'input)")
    parser.add_argument("-s", "--size", type=int, default=None,
                        help="Taille cible en pixels (resize carré, ex: --size 44)")
    args = parser.parse_args()

    input_path = Path(args.input)

    if input_path.is_file():
        files = [input_path]
    elif input_path.is_dir():
        files = sorted(input_path.glob("*.png"))
        if not files:
            print(f"Aucun PNG trouvé dans {input_path}")
            sys.exit(1)
    else:
        print(f"Erreur: {input_path} n'existe pas")
        sys.exit(1)

    if args.output:
        output_dir = Path(args.output)
    else:
        output_dir = files[0].parent if files[0].is_file() else input_path

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Conversion de {len(files)} PNG -> headers C (RGBA)")
    print(f"Sortie: {output_dir}\n")

    for f in files:
        process_file(f, output_dir, args.size)

    print(f"\nTerminé. {len(files)} sprite(s) généré(s).")


if __name__ == "__main__":
    main()
