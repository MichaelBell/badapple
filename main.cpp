#include "libraries/pico_graphics/pico_graphics_dv.hpp"
#include "drivers/dv_display/dv_display.hpp"

#include "ff.h"

#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "pico/sync.h"

using namespace pimoroni;

auto_init_mutex(fs_mutex);

FATFS fs;
FIL fil;
FIL audio_file;
FRESULT fr;
DVDisplay display;

#define NUM_BUFFERS 16
#define BUFFER_LEN 256
#define BUFFER_BYTES (BUFFER_LEN*2)
uint16_t buf[BUFFER_LEN];
uint write_buf;
uint read_buf;
uint buf_idx;

#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 480

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

#define AUDIO_SAMPLES_PER_BUFFER 256

static struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
            .sample_freq = 22050,
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 4
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 8, AUDIO_SAMPLES_PER_BUFFER);
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    return producer_pool;
}

static void fill_audio_buffer(audio_buffer_pool *producer_pool) {
    audio_buffer_t *buffer;

    buffer = take_audio_buffer(producer_pool, false);
    if (!buffer) return;

    uint bytes_read;
    mutex_enter_blocking(&fs_mutex);
    fr = f_read(&audio_file, buffer->buffer->bytes, buffer->max_sample_count * 4, &bytes_read);
    mutex_exit(&fs_mutex);
    if (fr != FR_OK || bytes_read == 0) {
        printf("Audio read fail\n");
        return;
    }
    //memset(buffer->buffer, 0, bytes_read);

    buffer->sample_count = bytes_read >> 2;
    give_audio_buffer(producer_pool, buffer);
    printf(".");
}

static bool display_frame() {
    for (int y = 0; y < DISPLAY_HEIGHT; ++y)
    {
        int x = 0;
        while (x < DISPLAY_WIDTH) {
            const uint16_t span_len = buf[buf_idx] >> 6;
            const uint8_t colour = buf[buf_idx] & 0x3C;
            display.write_palette_pixel_span({x, y}, span_len, colour);
            x += span_len;

            if (++buf_idx == BUFFER_LEN) {
                uint bytes_read;
                mutex_enter_blocking(&fs_mutex);
                fr = f_read(&fil, buf, BUFFER_BYTES, &bytes_read);
                mutex_exit(&fs_mutex);
                if (fr != FR_OK) {
                    printf("Failed to read data, error: %d\n", fr);
                    return false;
                }

                buf_idx = 0;
            }
        }
    }

    return true;
}

volatile bool run_fs = false;

void core1_main() {
    audio_buffer_pool *ap = init_audio();
    bool ok = audio_i2s_connect(ap);
    assert(ok);
    audio_i2s_set_enabled(true);

    while (true) {
        multicore_fifo_pop_blocking();
        
        while (run_fs) {
            fill_audio_buffer(ap);
            //fill_video_buffer();
        }

        multicore_fifo_push_blocking(0);
    }
}

int main() {
    set_sys_clock_khz(200000, true);
    stdio_init_all();

    sleep_ms(5000);
    printf("Hello\n");

    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
      printf("Failed to mount SD card, error: %d\n", fr);
      return 0;
    }

    display.init(DISPLAY_WIDTH, DISPLAY_HEIGHT, DVDisplay::MODE_PALETTE, FRAME_WIDTH, FRAME_HEIGHT);
    PicoGraphics_PenDV_P5 graphics(FRAME_WIDTH, FRAME_HEIGHT, display);

    multicore_launch_core1(core1_main);
    printf("Init\n");

#if 0
    int black = graphics.create_pen(0, 0, 0);
    int white = graphics.create_pen(255, 255, 255);
    int grey = graphics.create_pen(135, 135, 135);
    int dark_grey = graphics.create_pen(72, 72, 72);
#else
    for (int i = 0; i < 8; ++i) {
        uint8_t col = (i << 1) | (i << 5) | (i >> 2);
        graphics.create_pen(col, col, col);
    }
#endif
    graphics.set_pen(0);
    graphics.clear();
    display.flip();

    while (true) {
        fr = f_open(&fil, "/badapple640x480-8m.bin", FA_READ);
        if (fr != FR_OK) {
            printf("Failed to open badapple video, error: %d\n", fr);
            return 0;
        }

        fr = f_open(&audio_file, "/badapple-22050.pcm", FA_READ);
        if (fr != FR_OK) {
            printf("Failed to open badapple audio, error: %d\n", fr);
            return 0;
        }

        uint bytes_read;
        fr = f_read(&fil, buf, BUFFER_BYTES, &bytes_read);
        if (fr != FR_OK) {
            printf("Failed to read data, error: %d\n", fr);
            return 0;
        }
        buf_idx = 0;

        printf("Audio start\n");
        run_fs = true;
        multicore_fifo_push_blocking(0);

        absolute_time_t start_time = get_absolute_time();
        for (int i = 0; i < 6950; ++i) {
            display_frame();
            
            absolute_time_t sleep_to_time = delayed_by_ms(start_time, 30);
            if (absolute_time_diff_us(get_absolute_time(), sleep_to_time) > 1000) {
                sleep_until(sleep_to_time);
            }
            else {
                printf("Frame %d time %lldms\n", i, absolute_time_diff_us(start_time, get_absolute_time()) / 1000);
            }
            display.flip();
            start_time = delayed_by_ms(start_time, (i % 3 == 2) ? 34 : 33);
        }
        run_fs = false;
        multicore_fifo_pop_blocking();

        f_close(&fil);
        f_close(&audio_file);
    }
}