#!/usr/bin/env python3
"""
Génère un sprite de biberon (baby bottle) en PNG pour le Gotchi.
Style cartoon simple, cohérent avec les autres food sprites (22x22 et 44x44).
"""

from PIL import Image, ImageDraw

def draw_bottle(size: int) -> Image.Image:
    """Dessine un biberon cartoon sur fond transparent."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Proportions relatives à la taille
    s = size / 44.0  # facteur d'échelle (référence 44px)

    # Tétine (en haut) — rose/saumon
    nipple_color = (240, 140, 120, 255)
    nipple_top = int(1 * s)
    nipple_bot = int(12 * s)
    nipple_left = int(15 * s)
    nipple_right = int(29 * s)
    draw.rounded_rectangle(
        [nipple_left, nipple_top, nipple_right, nipple_bot],
        radius=int(4 * s), fill=nipple_color
    )
    # Pointe de la tétine
    draw.ellipse(
        [int(18 * s), int(0 * s), int(26 * s), int(6 * s)],
        fill=nipple_color
    )

    # Bague (anneau de vissage) — gris clair
    ring_color = (200, 200, 210, 255)
    ring_top = int(11 * s)
    ring_bot = int(16 * s)
    draw.rounded_rectangle(
        [int(12 * s), ring_top, int(32 * s), ring_bot],
        radius=int(2 * s), fill=ring_color
    )

    # Corps du biberon — blanc/bleu très clair (verre)
    body_color = (220, 235, 255, 240)
    body_top = int(15 * s)
    body_bot = int(40 * s)
    draw.rounded_rectangle(
        [int(13 * s), body_top, int(31 * s), body_bot],
        radius=int(4 * s), fill=body_color
    )

    # Lait à l'intérieur — blanc crème (2/3 du corps)
    milk_color = (255, 250, 240, 250)
    milk_top = int(24 * s)
    draw.rounded_rectangle(
        [int(15 * s), milk_top, int(29 * s), int(38 * s)],
        radius=int(3 * s), fill=milk_color
    )

    # Graduations — petites lignes bleues sur le corps
    grad_color = (150, 180, 220, 180)
    for i in range(3):
        y = int((20 + i * 6) * s)
        draw.line(
            [int(14 * s), y, int(18 * s), y],
            fill=grad_color, width=max(1, int(1 * s))
        )

    # Reflet — trait blanc vertical léger
    reflet_color = (255, 255, 255, 120)
    draw.line(
        [int(17 * s), int(17 * s), int(17 * s), int(36 * s)],
        fill=reflet_color, width=max(1, int(2 * s))
    )

    # Fond du biberon — arrondi
    bottom_color = (190, 200, 220, 255)
    draw.rounded_rectangle(
        [int(14 * s), int(38 * s), int(30 * s), int(43 * s)],
        radius=int(3 * s), fill=bottom_color
    )

    return img


if __name__ == "__main__":
    # Générer les deux tailles
    for sz in [22, 44]:
        img = draw_bottle(sz)
        out = f"assets/sprites/bottle_{sz}.png"
        img.save(out)
        print(f"  -> {out} ({sz}x{sz})")

    print("Done! Maintenant lancer png_to_sprite_rgba.py pour convertir.")
