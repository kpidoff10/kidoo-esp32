#!/usr/bin/env python3
"""
Génère un sprite de gélule/capsule médicale en PNG pour le Gotchi.
Style cartoon, bicolore rouge/blanc classique.
"""

from PIL import Image, ImageDraw

def draw_capsule(size: int) -> Image.Image:
    """Dessine une capsule médicale cartoon sur fond transparent."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    s = size / 44.0

    # Capsule horizontale, légèrement inclinée
    # Moitié gauche : rouge
    red = (220, 50, 60, 255)
    # Moitié droite : blanc
    white = (245, 245, 250, 250)

    # Corps capsule — rectangle arrondi
    cap_left = int(6 * s)
    cap_right = int(38 * s)
    cap_top = int(14 * s)
    cap_bot = int(30 * s)
    cap_mid = (cap_left + cap_right) // 2
    radius = int(8 * s)

    # Moitié gauche (rouge)
    draw.rounded_rectangle(
        [cap_left, cap_top, cap_mid + int(2 * s), cap_bot],
        radius=radius, fill=red
    )
    # Moitié droite (blanc)
    draw.rounded_rectangle(
        [cap_mid - int(2 * s), cap_top, cap_right, cap_bot],
        radius=radius, fill=white
    )

    # Bande de séparation au milieu
    sep_color = (200, 200, 210, 200)
    draw.rectangle(
        [cap_mid - int(1 * s), cap_top + int(2 * s),
         cap_mid + int(1 * s), cap_bot - int(2 * s)],
        fill=sep_color
    )

    # Reflet sur la partie rouge
    reflet = (255, 100, 100, 100)
    draw.rounded_rectangle(
        [cap_left + int(3 * s), cap_top + int(2 * s),
         cap_mid - int(2 * s), cap_top + int(5 * s)],
        radius=int(2 * s), fill=reflet
    )

    # Reflet sur la partie blanche
    reflet_w = (255, 255, 255, 120)
    draw.rounded_rectangle(
        [cap_mid + int(3 * s), cap_top + int(2 * s),
         cap_right - int(3 * s), cap_top + int(5 * s)],
        radius=int(2 * s), fill=reflet_w
    )

    # Petit + médical sur la partie rouge
    plus_color = (255, 255, 255, 180)
    cx = cap_left + int(10 * s)
    cy = (cap_top + cap_bot) // 2
    pw = int(2 * s)
    ph = int(5 * s)
    # Horizontal
    draw.rectangle([cx - ph, cy - pw, cx + ph, cy + pw], fill=plus_color)
    # Vertical
    draw.rectangle([cx - pw, cy - ph, cx + pw, cy + ph], fill=plus_color)

    return img


if __name__ == "__main__":
    for sz in [22, 44]:
        img = draw_capsule(sz)
        out = f"assets/sprites/medicine_{sz}.png"
        img.save(out)
        print(f"  -> {out} ({sz}x{sz})")

    print("Done!")
