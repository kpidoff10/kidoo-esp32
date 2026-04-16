#!/usr/bin/env python3
"""
Convertit des PNG (blanc sur transparent) en header C pour ESP32.

Chaque pixel est stocké comme une valeur d'alpha (0-255).
Au rendu, alpha > 0 -> dessiner avec la couleur choisie dynamiquement.

Usage:
  python png_to_sprite.py assets/sprites/zzz_64.png
  python png_to_sprite.py assets/sprites/           # tous les PNG du dossier
  python png_to_sprite.py assets/sprites/ -o src/models/gotchi/face/overlay/sprites/

Le header généré contient:
  - Un tableau const uint8_t PROGMEM avec les valeurs d'alpha
  - Les dimensions (width, height)
  - Une struct SpriteData pour accès facile
"""

import argparse
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Erreur: Pillow requis. Installe avec: pip install Pillow")
    sys.exit(1)


def png_to_alpha_array(img_path: Path) -> tuple[str, int, int, list[int]]:
    """Charge un PNG et extrait le canal alpha (ou luminosité si pas d'alpha)."""
    img = Image.open(img_path).convert("RGBA")
    w, h = img.size
    pixels = list(img.getdata())

    alpha_values = []
    for r, g, b, a in pixels:
        # On combine luminosité et alpha pour capturer le glow
        # Blanc pur (#FFF) avec alpha 255 -> 255
        # Blanc avec alpha 100 -> 100 (glow semi-transparent)
        # Transparent -> 0
        luminosity = max(r, g, b)
        value = (luminosity * a) // 255
        alpha_values.append(value)

    name = img_path.stem  # ex: "zzz_64"
    return name, w, h, alpha_values


def generate_header(name: str, w: int, h: int, alpha: list[int]) -> str:
    """Génère le contenu du header C."""
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
        f"// Auto-generated from {name}.png ({w}x{h})",
        f"// Chaque byte = alpha du pixel (0=transparent, 255=opaque)",
        f"// Coloriser dynamiquement au rendu avec la couleur voulue",
        "",
        f"constexpr uint16_t SPRITE_{upper}_WIDTH = {w};",
        f"constexpr uint16_t SPRITE_{upper}_HEIGHT = {h};",
        "",
        f"const uint8_t SPRITE_{upper}_DATA[] PROGMEM = {{",
    ]

    # Écrire les données en lignes de 16 valeurs
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
        f"  SPRITE_{upper}_DATA, nullptr",
        f"}};",
        "",
        f"#endif // SPRITE_{upper}_INCLUDED",
        "",
    ])

    return "\n".join(lines)


def process_file(img_path: Path, output_dir: Path):
    """Convertit un PNG en header C."""
    name, w, h, alpha = png_to_alpha_array(img_path)

    # Stats
    non_zero = sum(1 for v in alpha if v > 0)
    total = w * h
    print(f"  {name}.png : {w}x{h}, {non_zero}/{total} pixels actifs, {total} bytes")

    header_content = generate_header(name, w, h, alpha)
    output_path = output_dir / f"sprite_{name}.h"
    output_path.write_text(header_content)
    print(f"  -> {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Convertit PNG -> header C sprite (alpha map)")
    parser.add_argument("input", help="Fichier PNG ou dossier contenant des PNG")
    parser.add_argument("-o", "--output", default=None,
                        help="Dossier de sortie (défaut: même dossier que l'input)")
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

    print(f"Conversion de {len(files)} PNG -> headers C")
    print(f"Sortie: {output_dir}\n")

    for f in files:
        process_file(f, output_dir)

    print(f"\nTerminé. {len(files)} sprite(s) généré(s).")


if __name__ == "__main__":
    main()
