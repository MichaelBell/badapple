#!/usr/bin/env python3

import struct
from PIL import Image

out_file = open("badapple240x240.bin", "wb")
data_len = 0

colour_shift_changes = {
    1: 6,
    1506: 7,
    1743: 6,
    1873: 7,
    1913: 6,
    2716: 6,
    2737: 5,
    2836: 7,
    2918: 6,
    2957: 4,
    3320: 6
}

colour_shift = 5

def convert_span_colour(span_colour):
    if colour_shift == 7:
        span_colour = 0b11111 if span_colour else 0
    elif colour_shift == 6:
        span_colour = (span_colour << 3) | (span_colour << 1) | (span_colour >> 1)
    elif colour_shift == 5:
        span_colour = (span_colour << 2) | (span_colour >> 1)
    elif colour_shift == 4:
        span_colour = (span_colour << 1) | (span_colour >> 3)
    return span_colour

for i in range(1,6957):
    if i in colour_shift_changes:
        colour_shift = colour_shift_changes[i]

    if (i & 1) != 1: continue

    img = Image.open("png_frames/badapple%04d.png" % (i,)).resize((240,240))

    data = img.load()

    for y in range(0,240):
        span_len = 0
        span_colour = -255
        for x in range(240):
            colour = data[x, y][0] >> colour_shift
            if colour_shift == 2 and colour > 0x10: colour &= 0x3E
            if colour != span_colour:
                if span_len > 0:
                    span_colour = convert_span_colour(span_colour)
                    out_file.write(struct.pack('<BB', span_len, span_colour))
                    data_len += 2
                span_len = 1
                span_colour = colour
            else:
                span_len += 1
        span_colour = convert_span_colour(span_colour)
        out_file.write(struct.pack('<BB', span_len, span_colour))
        data_len += 2
    print("Frame %d, len %.2fMB" % (i, data_len / (1024 * 1024)))
