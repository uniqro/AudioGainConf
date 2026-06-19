#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Create airplane icon for Audio DSP Config application"""

import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("❌ PIL/Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

def draw_airplane(draw, size):
    """Draw an airplane on the given ImageDraw at the given size."""
    cx, cy = size // 2, size // 2
    scale = size / 256.0

    # Colors - RGB with alpha
    wing_color = (15, 52, 96, 255)        # Dark blue #0f3460
    wing_light = (26, 74, 122, 255)        # Light blue highlight
    body_color = (233, 69, 96, 255)        # Red #e94560
    body_light = (255, 107, 129, 255)      # Light red highlight
    outline_color = (15, 52, 96, 255)      # Dark blue outline
    window_color = (135, 206, 235, 255)    # Sky blue windows

    def ow():
        """Outline width scaled."""
        return max(1, int(1.5 * scale))

    # --- BODY (fuselage) ---
    draw.ellipse(
        [cx - 8*scale, cy - 100*scale, cx + 8*scale, cy - 88*scale],
        fill=body_light, outline=outline_color, width=ow()
    )
    draw.ellipse(
        [cx - 8*scale, cy + 40*scale - 12*scale, cx + 8*scale, cy + 40*scale],
        fill=body_color, outline=outline_color, width=ow()
    )
    draw.polygon(
        [
            (cx, cy - 95*scale),
            (cx + 8*scale, cy - 88*scale),
            (cx + 12*scale, cy + 35*scale),
            (cx + 8*scale, cy + 40*scale),
            (cx - 8*scale, cy + 40*scale),
            (cx - 12*scale, cy + 35*scale),
        ],
        fill=body_color, outline=outline_color, width=ow()
    )

    # --- FRONT WINGS (swept back) ---
    draw.polygon(
        [(cx - 10*scale, cy + 18*scale), (cx - 10*scale, cy + 25*scale),
         (cx - 80*scale, cy + 65*scale), (cx - 80*scale, cy + 55*scale)],
        fill=wing_color, outline=outline_color, width=ow()
    )
    draw.polygon(
        [(cx + 10*scale, cy + 18*scale), (cx + 10*scale, cy + 25*scale),
         (cx + 80*scale, cy + 65*scale), (cx + 80*scale, cy + 55*scale)],
        fill=wing_color, outline=outline_color, width=ow()
    )
    # Wing highlight strips
    draw.polygon(
        [(cx - 10*scale, cy + 20*scale), (cx - 75*scale, cy + 60*scale),
         (cx - 75*scale, cy + 57*scale), (cx - 10*scale, cy + 24*scale)],
        fill=wing_light
    )
    draw.polygon(
        [(cx + 10*scale, cy + 20*scale), (cx + 75*scale, cy + 60*scale),
         (cx + 75*scale, cy + 57*scale), (cx + 10*scale, cy + 24*scale)],
        fill=wing_light
    )

    # --- REAR / TAIL WINGS ---
    draw.polygon(
        [(cx - 10*scale, cy + 52*scale), (cx - 10*scale, cy + 58*scale),
         (cx - 30*scale, cy + 70*scale), (cx - 30*scale, cy + 67*scale)],
        fill=wing_color, outline=outline_color, width=ow()
    )
    draw.polygon(
        [(cx + 10*scale, cy + 52*scale), (cx + 10*scale, cy + 58*scale),
         (cx + 30*scale, cy + 70*scale), (cx + 30*scale, cy + 67*scale)],
        fill=wing_color, outline=outline_color, width=ow()
    )

    # --- VERTICAL STABILIZER (tail fin) ---
    draw.polygon(
        [(cx - 6*scale, cy + 32*scale), (cx, cy + 18*scale), (cx + 6*scale, cy + 32*scale),
         (cx + 6*scale, cy + 36*scale), (cx, cy + 22*scale), (cx - 6*scale, cy + 36*scale)],
        fill=wing_color, outline=outline_color, width=ow()
    )

    # --- WINDOWS ---
    win_r = max(2, int(2.5 * scale))
    for i in range(4):
        wy = cy + 10*scale + i * 7*scale
        draw.ellipse(
            [cx - win_r, wy - win_r, cx + win_r, wy + win_r],
            fill=window_color, outline=outline_color,
            width=max(1, int(scale * 6))
        )


def create_airplane_icon(output_path=None):
    """Create airplane icon — single 256x256 frame (supported by Pillow 9.1.1).
       Windows auto-scales to smaller sizes."""
    if output_path is None:
        output_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'resources', 'app_icon.ico'
        )

    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Pillow 9.1.1 only supports single-frame ICO write;
    # Windows will scale down the 256x256 image as needed.
    img = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_airplane(draw, 256)

    img.save(output_path, format='ICO')
    print(f'✅ Icon created: {output_path} (256x256)')
    return True

if __name__ == '__main__':
    create_airplane_icon()
