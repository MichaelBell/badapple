#!/usr/bin/env python3

import struct
from PIL import Image

out_file = open("badapple480x480-565.bin", "wb")
data_len = 0

for i in range(1,6957):
    img = Image.open("png_frames/badapple%04d.png" % (i,)).resize((480,480))

    data = img.load()

    for y in range(0,480):
        span_len = 0
        span_colour = -255
        for x in range(480):
            colour = data[x, y][0] >> 5
            if colour != span_colour:
                if span_len > 0:
                    span_colour = (span_colour << 2) | (span_colour >> 1)
                    span_colour = (span_colour << 11) | (span_colour << 6) | span_colour
                    span_colour = (span_colour & 0xFF) << 8 | ((span_colour >> 8) & 0xFF)  # Stupid byte swap
                    out_file.write(struct.pack('<HH', span_len, span_colour))
                    data_len += 4
                span_len = 1
                span_colour = colour
            else:
                span_len += 1
        span_colour = (span_colour << 2) | (span_colour >> 1)
        span_colour = (span_colour << 11) | (span_colour << 6) | span_colour
        span_colour = (span_colour & 0xFF) << 8 | ((span_colour >> 8) & 0xFF)  # Stupid byte swap
        out_file.write(struct.pack('<HH', span_len, span_colour))
        data_len += 4
    print("Frame %d, len %.2fMB" % (i, data_len / (1024 * 1024)))
