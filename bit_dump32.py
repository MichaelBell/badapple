#!/usr/bin/env python3

import struct
from PIL import Image

out_file = open("badapple640x480-32m.bin", "wb")

def repeat_n_times(list, n):
    return [item for item in list for i in range(n)]

TWO  = repeat_n_times((0, 0x3C), 32)
FOUR = repeat_n_times((0, 0x14, 0x28, 0x3C), 16)
EIGHT= repeat_n_times((0, 0x08, 0x10, 0x18, 0x24, 0x2C, 0x34, 0x3C), 8)
SIXT = repeat_n_times((0, 0x4, 0x8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x34, 0x3C), 4)
THIRT= [0, 0x40, 0x44, 0x48, 0x4, 0x4C, 0x50, 0x54, 0x8, 0x58, 0x5C, 0x60, 0xC, 0x64, 0x68, 0x6C, 0x10, 0x70, 0x74, 0x78, 0x14, 0x14, 0x14, 0x14]
THIRT.extend(SIXT[24:])

print(len(SIXT))
print(len(THIRT))

colour_shift_changes = {
    1: SIXT,
    1509: FOUR,
    1526: SIXT,
    1873: TWO,
    1913: SIXT,
    2716: EIGHT,
    2737: SIXT,
    2916: EIGHT,
    2957: THIRT,
    3320: SIXT
}

for i in range(1,6957):
    img = Image.open("png_frames/badapple%04d.png" % (i,)).resize((640,480))

    data = img.load()

    if i in colour_shift_changes.keys():
        colour_shift = colour_shift_changes[i]

    for y in range(0,480):
        span_len = 0
        span_colour = 0
        for x in range(640):
            colour = colour_shift[data[x, y][0] >> 2]
            if colour != span_colour:
                #if span_len < 4 and span_colour != 0 and span_colour != 0x1C:
                #    span_colour = colour
                #    span_len += 1
                #else:
                if span_len > 0:
                    out_file.write(struct.pack('<H', (span_len << 7) + span_colour))
                span_len = 1
                span_colour = colour
            elif span_len == 511:
                out_file.write(struct.pack('<H', (span_len << 7) + span_colour))
                span_len = 1
            else:
                span_len += 1
        out_file.write(struct.pack('<H', (span_len << 7) + span_colour))
    print("Frame %d" % (i,))