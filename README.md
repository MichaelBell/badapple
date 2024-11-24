# Bad Apple on Presto

To run this first you'll need to acquire the Bad Apple video from somewhere.  I used the [official video](https://www.youtube.com/watch?v=i41KoE0iMYU) from YouTube.  If you use a different version you may need to adjust some frame numbers.

To extract the frames and audio from the video:

    ffmpeg -i badapple.mp4 "frames/badapple%04d.png"
    ffmpeg -i badapple.mp4 -ar 20000 -filter:a "volume=0.5" -acodec pcm_u8 -f u8 badapple-20000-mono.pcm

To build the run length encoded video from the extracted frames (this will take a few minutes):

    ./bit_dump32.py

Gzip the resulting `badapple480x480-565.bin`, and copy `badapple480x480-565.bin.gz` to the root directory of an SD card.

Build the project in the normal way, flash to the Presto, and enjoy!