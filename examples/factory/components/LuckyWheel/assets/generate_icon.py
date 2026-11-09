"""Regenerate the original Lucky Wheel launcher icon (requires Pillow)."""
from pathlib import Path
import math
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SCALE = 4
SIZE = 112
CREAM = '#fbf5e9'
GREEN = '#334d43'
COLORS = ['#f6a79a', '#f1ca86', '#c9dda0', '#96d3bc',
          '#9ccce7', '#bab6e5', '#e4bad2', '#edbc9f']


def box(bounds):
    return tuple(round(v * SCALE) for v in bounds)


image = Image.new('RGBA', (SIZE * SCALE, SIZE * SCALE))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle(box((1, 1, 111, 111)), radius=26 * SCALE, fill=CREAM)
draw.ellipse(box((9, 9, 103, 103)), outline='#e7dfd1', width=SCALE)
for index, color in enumerate(COLORS):
    start = -112.5 + index * 45
    draw.pieslice(box((12, 12, 100, 100)), start, start + 45, fill=color)
    angle = math.radians(start)
    end = (56 + 44 * math.cos(angle), 56 + 44 * math.sin(angle))
    draw.line(box((56, 56, *end)), fill=CREAM, width=2 * SCALE)
draw.ellipse(box((38, 38, 74, 74)), fill=CREAM)
draw.ellipse(box((41, 41, 71, 71)), fill=GREEN)
draw.ellipse(box((53, 53, 59, 59)), fill=CREAM)
draw.polygon([box(p) for p in ((49, 13), (63, 13), (56, 29))], fill=GREEN)
draw.polygon([box(p) for p in ((52, 15), (60, 15), (56, 24))], fill=CREAM)
image = image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
image.save(HERE / 'lucky-wheel-icon.png')
pixels = image.tobytes('raw', 'BGRA')
rows = [','.join(f'0x{value:02x}' for value in pixels[i:i + 32])
        for i in range(0, len(pixels), 32)]
source = '''// Original launcher artwork; regenerate with generate_icon.py.
#include "lvgl.h"
static const uint8_t pixels[] = {
'''
source += ''.join('    ' + row + ',\n' for row in rows)
source += '''};
const lv_image_dsc_t img_app_lucky_wheel = {
    .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_ARGB8888,
               .w=112, .h=112, .stride=448},
    .data_size=sizeof(pixels), .data=pixels,
};
'''
(HERE / 'img_app_lucky_wheel.c').write_text(source, encoding='utf-8', newline='\n')
