"""Regenerate the product-shaped Photo Badge launcher icon (requires Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
SIZE = 112
SCALE = 8


def bounds(values):
    return tuple(round(value * SCALE) for value in values)


icon = Image.new('RGBA', (SIZE * SCALE, SIZE * SCALE))
mask = Image.new('L', icon.size)
ImageDraw.Draw(mask).rounded_rectangle(bounds((1, 1, 111, 111)), radius=26*SCALE, fill=255)
surface = Image.new('RGBA', icon.size)
draw = ImageDraw.Draw(surface)
for y in range(surface.height):
    t = y / (surface.height - 1)
    color = tuple(round(a + (b-a)*t) for a, b in zip((218, 231, 219), (177, 203, 187)))
    draw.line((0, y, surface.width, y), fill=(*color, 255))
icon.paste(surface, (0, 0), mask)
draw = ImageDraw.Draw(icon)
draw.rounded_rectangle(bounds((1.7, 1.7, 110.3, 110.3)), radius=25.3*SCALE,
                       outline='#e5eee4', width=round(.65*SCALE))

shadow = Image.new('RGBA', icon.size)
ImageDraw.Draw(shadow).ellipse(bounds((10, 14, 104, 108)), fill=(28, 55, 42, 65))
icon = Image.alpha_composite(icon, shadow.filter(ImageFilter.GaussianBlur(2*SCALE)))

# Keep the actual product's silver shell and black bezel, using the supplied
# Waveshare product photo. Only the circular display content is illustrated.
shell = Image.open(HERE / 'badge-device-shell.png').convert('RGBA')
shell = shell.resize((96*SCALE, 96*SCALE), Image.Resampling.LANCZOS)
icon.alpha_composite(shell, (8*SCALE, 8*SCALE))

# A large landscape makes the photo-display function legible at launcher size.
photo = Image.new('RGBA', (72*SCALE, 72*SCALE), '#c6e1df')
p = ImageDraw.Draw(photo)
p.ellipse(bounds((44, 11, 58, 25)), fill='#edb979')
p.polygon([bounds(point) for point in ((0, 49), (22, 25), (42, 46), (59, 33),
                                     (72, 45), (72, 72), (0, 72))], fill='#91b6a1')
p.polygon([bounds(point) for point in ((0, 58), (23, 44), (40, 53), (58, 44),
                                     (72, 54), (72, 72), (0, 72))], fill='#416f61')
photo_mask = Image.new('L', photo.size)
ImageDraw.Draw(photo_mask).ellipse((0, 0, photo.width-1, photo.height-1), fill=255)
icon.paste(photo, (20*SCALE, 20*SCALE), photo_mask)

icon = icon.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
icon.save(HERE / 'badge-icon.png')
pixels = icon.tobytes('raw', 'BGRA')
rows = [','.join(f'0x{value:02x}' for value in pixels[i:i+32])
        for i in range(0, len(pixels), 32)]
source = '''// Photo Badge launcher artwork; regenerate with generate_icon.py.
#include "lvgl.h"
static const uint8_t pixels[] = {
'''
source += ''.join('    ' + row + ',\n' for row in rows)
source += '''};
const lv_image_dsc_t img_app_badge = {
    .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_ARGB8888,
               .w=112, .h=112, .stride=448},
    .data_size=sizeof(pixels), .data=pixels,
};
'''
(HERE / 'img_app_badge.c').write_text(source, encoding='utf-8', newline='\n')
