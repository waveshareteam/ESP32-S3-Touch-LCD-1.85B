"""Regenerate the approved Watch launcher artwork (requires Pillow)."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
SCALE = 8
SIZE = 112


def bounds(values):
    return tuple(round(v * SCALE) for v in values)


def point(degrees, radius):
    a = math.radians(degrees - 90)
    return 56 + radius * math.cos(a), 56 + radius * math.sin(a)


icon = Image.new('RGBA', (SIZE * SCALE, SIZE * SCALE))
mask = Image.new('L', icon.size)
ImageDraw.Draw(mask).rounded_rectangle(bounds((1, 1, 111, 111)), radius=26*SCALE, fill=255)
surface = Image.new('RGBA', icon.size)
pixels = surface.load()
for y in range(surface.height):
    t = y / (surface.height - 1)
    color = tuple(round(a + (b-a)*t) for a, b in zip((167, 171, 176), (139, 144, 150))) + (255,)
    for x in range(surface.width):
        pixels[x, y] = color
icon.paste(surface, (0, 0), mask)
draw = ImageDraw.Draw(icon)
draw.rounded_rectangle(bounds((1.7, 1.7, 110.3, 110.3)), radius=25.3*SCALE,
                       outline='#bcc0c4', width=round(.65*SCALE))

shadow = Image.new('RGBA', icon.size)
ImageDraw.Draw(shadow).ellipse(bounds((10, 13, 102, 105)), fill=(0, 0, 0, 60))
icon = Image.alpha_composite(icon, shadow.filter(ImageFilter.GaussianBlur(2*SCALE)))
draw = ImageDraw.Draw(icon)
draw.ellipse(bounds((10.7, 10.7, 101.3, 101.3)), fill='#858b91')
draw.ellipse(bounds((11.5, 11.5, 100.5, 100.5)), fill='#d7d1c5')
draw.ellipse(bounds((12.2, 12.2, 99.8, 99.8)), fill='#f4f0e7')
for index in range(12):
    major = index % 3 == 0
    draw.line(bounds((*point(index*30, 32.3 if major else 35), *point(index*30, 39))),
              fill='#333b45' if major else '#92948f', width=round((2.4 if major else 1.55)*SCALE))


def hand(angle, length, width, tail=3):
    draw.line(bounds((*point(angle, -tail), *point(angle, length))),
              fill='#26313c', width=round(width*SCALE))
    for radius in (-tail, length):
        x, y = point(angle, radius)
        draw.ellipse(bounds((x-width/2, y-width/2, x+width/2, y+width/2)), fill='#26313c')


hand(305, 21.5, 4.8)
hand(60, 29.5, 3.3)
draw.line(bounds((*point(174, -7), *point(174, 34.5))), fill='#ce4854', width=round(1.05*SCALE))
draw.ellipse(bounds((52.9, 52.9, 59.1, 59.1)), fill='#ce4854')
draw.ellipse(bounds((54.9, 54.9, 57.1, 57.1)), fill='#f4f0e7')
icon = icon.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
icon.save(HERE / 'watch-icon.png')
pixels = icon.tobytes('raw', 'BGRA')
rows = [','.join(f'0x{value:02x}' for value in pixels[i:i + 32])
        for i in range(0, len(pixels), 32)]
source = '''// Original launcher artwork; regenerate with generate_icon.py.
#include "lvgl.h"
static const uint8_t pixels[] = {
'''
source += ''.join('    ' + row + ',\n' for row in rows)
source += '''};
const lv_image_dsc_t img_app_watch = {
    .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_ARGB8888,
               .w=112, .h=112, .stride=448},
    .data_size=sizeof(pixels), .data=pixels,
};
'''
(HERE / 'img_app_watch.c').write_text(source, encoding='utf-8', newline='\n')
