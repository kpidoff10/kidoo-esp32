#!/usr/bin/env python3
"""
Génère des taches de saleté réalistes (64x64) avec bords irréguliers,
variations d'opacité et forme organique pour le Gotchi.
"""

from PIL import Image, ImageDraw, ImageFilter
import random
import math

def perlin_noise_simple(x, y, seed=0):
    """Bruit pseudo-aléatoire simple pour des formes organiques."""
    n = x * 374761393 + y * 668265263 + seed * 1274126177
    n = (n ^ (n >> 13)) * 1274126177
    n = n ^ (n >> 16)
    return (n & 0x7fffffff) / 0x7fffffff

def draw_organic_stain(size: int, seed: int = 42) -> Image.Image:
    """Crée une tache organique réaliste avec bords irréguliers."""
    random.seed(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    cx, cy = size / 2, size / 2
    base_radius = size * 0.35

    # Palette de bruns réalistes
    browns = [
        (75, 50, 25),
        (90, 60, 30),
        (65, 42, 18),
        (100, 72, 38),
        (55, 35, 15),
        (85, 58, 28),
    ]

    # --- Couche 1 : Grande forme irrégulière principale ---
    for layer in range(5):
        color = random.choice(browns)
        alpha_base = 60 + layer * 20  # Plus opaque au centre
        r_variation = base_radius * (0.6 + layer * 0.08)
        offset_x = random.uniform(-4, 4)
        offset_y = random.uniform(-4, 4)

        # Générer un contour irrégulier avec du bruit
        points = []
        num_pts = 24
        for i in range(num_pts):
            angle = 2 * math.pi * i / num_pts
            # Rayon variable avec du bruit
            noise = perlin_noise_simple(
                int(math.cos(angle) * 100),
                int(math.sin(angle) * 100),
                seed + layer * 37
            )
            r = r_variation * (0.5 + noise * 0.7)
            px = cx + offset_x + r * math.cos(angle)
            py = cy + offset_y + r * math.sin(angle)
            points.append((px, py))

        if len(points) >= 3:
            draw.polygon(points, fill=(*color, alpha_base))

    # --- Couche 2 : Taches secondaires (éclaboussures) ---
    num_splats = random.randint(4, 8)
    for _ in range(num_splats):
        color = random.choice(browns)
        alpha = random.randint(50, 130)
        angle = random.uniform(0, 2 * math.pi)
        dist = random.uniform(base_radius * 0.3, base_radius * 1.1)
        sx = cx + dist * math.cos(angle)
        sy = cy + dist * math.sin(angle)
        sr = random.uniform(2, size * 0.12)

        # Forme elliptique aléatoire
        stretch = random.uniform(0.6, 1.4)
        draw.ellipse([
            sx - sr * stretch, sy - sr,
            sx + sr * stretch, sy + sr
        ], fill=(*color, alpha))

    # --- Couche 3 : Trainées (lignes organiques) ---
    for _ in range(3):
        color = random.choice(browns)
        alpha = random.randint(30, 80)
        x1 = cx + random.uniform(-base_radius, base_radius)
        y1 = cy + random.uniform(-base_radius, base_radius)
        angle = random.uniform(0, 2 * math.pi)
        length = random.uniform(8, size * 0.3)
        x2 = x1 + length * math.cos(angle)
        y2 = y1 + length * math.sin(angle)
        width = random.randint(2, 5)
        draw.line([(x1, y1), (x2, y2)], fill=(*color, alpha), width=width)

    # --- Couche 4 : Texture granuleuse ---
    for _ in range(60):
        color = random.choice(browns)
        alpha = random.randint(20, 90)
        x = random.uniform(0, size)
        y = random.uniform(0, size)
        # Seulement si dans la zone de la tache (pas trop loin du centre)
        d = math.sqrt((x - cx)**2 + (y - cy)**2)
        if d < base_radius * 1.2:
            r = random.uniform(0.5, 2)
            draw.ellipse([x-r, y-r, x+r, y+r], fill=(*color, alpha))

    # --- Couche 5 : Bord foncé subtil ---
    edge_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    edge_draw = ImageDraw.Draw(edge_img)
    points = []
    num_pts = 20
    for i in range(num_pts):
        angle = 2 * math.pi * i / num_pts
        noise = perlin_noise_simple(
            int(math.cos(angle) * 50),
            int(math.sin(angle) * 50),
            seed + 999
        )
        r = base_radius * (0.7 + noise * 0.5)
        points.append((cx + r * math.cos(angle), cy + r * math.sin(angle)))

    if len(points) >= 3:
        edge_draw.polygon(points, outline=(40, 25, 10, 40), width=2)

    img = Image.alpha_composite(img, edge_img)

    # Blur pour adoucir les bords
    img = img.filter(ImageFilter.GaussianBlur(radius=1.2))

    # Un deuxième pass de blur léger pour encore plus de douceur
    img = img.filter(ImageFilter.GaussianBlur(radius=0.5))

    return img


if __name__ == "__main__":
    for i in range(4):
        img = draw_organic_stain(64, seed=42 + i * 17)
        out = f"assets/sprites/dirt_big_{i}.png"
        img.save(out)
        print(f"  -> {out} (64x64, variante {i})")

    print("Done!")
