#!/usr/bin/env python3

import struct
from PIL import Image

out_file = open("badapple480x480-565.bin", "wb")
data_len = 0

colour_shift_changes = {
    1: 5,
    1526: 4,
    1873: 5,
    1913: 4,
    2716: 5,
    2737: 4,
    2916: 5,
    2957: 2,
    3305: 3,
    3320: 4
}

colour_shift = 5

def convert_span_colour(span_colour):
    if colour_shift == 2:
        span_colour = ((span_colour << 10) & 0xF800) | (span_colour << 5) | (span_colour >> 1)
    else:
        if colour_shift == 5:
            span_colour = (span_colour << 2) | (span_colour >> 1)
        elif colour_shift == 4:
            span_colour = (span_colour << 1) | (span_colour >> 3)
        span_colour = (span_colour << 11) | (span_colour << 6) | span_colour
    span_colour = (span_colour & 0xFF) << 8 | ((span_colour >> 8) & 0xFF)  # Stupid byte swap
    return span_colour

for i in range(1,6957):
    img = Image.open("png_frames/badapple%04d.png" % (i,)).resize((480,480))

    data = img.load()

    if i in colour_shift_changes:
        colour_shift = colour_shift_changes[i]

    for y in range(0,480):
        span_len = 0
        span_colour = -255
        for x in range(480):
            colour = data[x, y][0] >> colour_shift
            if colour_shift == 2 and colour > 0x10: colour &= 0x3E
            if colour != span_colour:
                if span_len > 0:
                    span_colour = convert_span_colour(span_colour)
                    out_file.write(struct.pack('<HH', span_len, span_colour))
                    data_len += 4
                span_len = 1
                span_colour = colour
            else:
                span_len += 1
        span_colour = convert_span_colour(span_colour)
        out_file.write(struct.pack('<HH', span_len, span_colour))
        data_len += 4
    print("Frame %d, len %.2fMB" % (i, data_len / (1024 * 1024)))
