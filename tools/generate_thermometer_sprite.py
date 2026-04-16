#!/usr/bin/env python3
"""
Génère un sprite de thermomètre médical en PNG pour le Gotchi.
Style cartoon, cohérent avec les autres food sprites.
"""

from PIL import Image, ImageDraw

def draw_thermometer(size: int) -> Image.Image:
    """Dessine un thermomètre médical cartoon sur fond transparent."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    s = size / 66.0  # facteur d'échelle (référence 66px)

    # Bulbe en bas (mercure) — rouge
    bulb_color = (220, 50, 50, 255)
    bulb_cx = int(33 * s)
    bulb_cy = int(52 * s)
    bulb_r = int(9 * s)
    draw.ellipse(
        [bulb_cx - bulb_r, bulb_cy - bulb_r, bulb_cx + bulb_r, bulb_cy + bulb_r],
        fill=bulb_color
    )

    # Corps du thermomètre — blanc/gris clair
    body_color = (240, 245, 250, 245)
    body_left = int(28 * s)
    body_right = int(38 * s)
    body_top = int(8 * s)
    body_bot = int(48 * s)
    draw.rounded_rectangle(
        [body_left, body_top, body_right, body_bot],
        radius=int(4 * s), fill=body_color
    )

    # Mercure (ligne rouge dans le tube) — monte du bulbe
    mercury_color = (220, 50, 50, 255)
    merc_left = int(31 * s)
    merc_right = int(35 * s)
    merc_top = int(18 * s)  # niveau du mercure (monte si fièvre)
    merc_bot = int(48 * s)
    draw.rectangle(
        [merc_left, merc_top, merc_right, merc_bot],
        fill=mercury_color
    )

    # Graduations — petites lignes grises sur le côté
    grad_color = (160, 170, 185, 200)
    for i in range(6):
        y = int((15 + i * 5) * s)
        draw.line(
            [int(25 * s), y, int(28 * s), y],
            fill=grad_color, width=max(1, int(1 * s))
        )

    # Bout du thermomètre (en haut) — arrondi argenté
    tip_color = (200, 210, 225, 255)
    draw.rounded_rectangle(
        [int(29 * s), int(5 * s), int(37 * s), int(12 * s)],
        radius=int(3 * s), fill=tip_color
    )

    # Reflet — trait blanc vertical
    reflet_color = (255, 255, 255, 100)
    draw.line(
        [int(30 * s), int(12 * s), int(30 * s), int(44 * s)],
        fill=reflet_color, width=max(1, int(2 * s))
    )

    # Contour subtil du bulbe (cercle plus foncé)
    bulb_outline = (180, 30, 30, 120)
    draw.ellipse(
        [bulb_cx - bulb_r - 1, bulb_cy - bulb_r - 1,
         bulb_cx + bulb_r + 1, bulb_cy + bulb_r + 1],
        outline=bulb_outline, width=max(1, int(1 * s))
    )

    return img


if __name__ == "__main__":
    for sz in [22, 66]:
        img = draw_thermometer(sz)
        out = f"assets/sprites/thermometer_{sz}.png"
        img.save(out)
        print(f"  -> {out} ({sz}x{sz})")

    print("Done! Lancer png_to_sprite_rgba.py pour convertir.")
