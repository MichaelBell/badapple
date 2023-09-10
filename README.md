# Bad Apple on PicoVision

To run this first you'll need to acquire the Bad Apple video from somewhere.  I used the [official video](https://www.youtube.com/watch?v=i41KoE0iMYU) from YouTube.  If you use a different version you may need to adjust some frame numbers.

To extract the frames and audio from the video:

    ffmpeg -i badapple.mp4 "frames/badapple%04d.png"
    ffmpeg -i badapple.mp4 -ar 22050 -filter:a "volume=0.5" -acodec pcm_s16le -f s16le badapple-22050.pcm

To build the run length encoded video from the extracted frames (this will take a few minutes):

    ./bit_dump32.py

Copy the resulting `badapple640x480-32m.bin` and `badapple-22050.pcm` to the root directory of an SD card.

Build the project in the normal way, flash to the PicoVision, plug a decent speaker in to the line out, and enjoy!