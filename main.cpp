#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7701/st7701.hpp"

#include "ff.h"

#include "pico/multicore.h"
#include "pico/sync.h"

#include "hardware/dma.h"

using namespace pimoroni;

FATFS fs;
FIL fil;
FIL audio_file;
FRESULT fr;

#define FRAME_WIDTH 480
#define FRAME_HEIGHT 480
static const uint BACKLIGHT = 45;
static const uint LCD_CLK = 26;
static const uint LCD_CS = 28;
static const uint LCD_DAT = 27;
static const uint LCD_DC = -1;
static const uint LCD_D0 = 1;

uint16_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT];

ST7701* presto;
PicoGraphics_PenRGB565* display;

#define NUM_BUFFERS 16
#define BUFFER_LEN 256
#define BUFFER_BYTES (BUFFER_LEN*2)
uint16_t buf[NUM_BUFFERS][BUFFER_LEN];
volatile uint write_buf;
volatile uint read_buf;
uint buf_idx;

static void fill_video_buffer() {
    uint next_buf_idx = (write_buf + 1) & 0xF;
    if (next_buf_idx == read_buf) return;

    uint bytes_read;
    fr = f_read(&fil, buf[write_buf], BUFFER_BYTES, &bytes_read);
    if (fr != FR_OK) {
        printf("Failed to read data, error: %d\n", fr);
        return;
    }

    write_buf = next_buf_idx;
}

static bool display_frame() {
    for (int y = 0; y < FRAME_HEIGHT; ++y)
    {
        int x = 0;
        while (x < FRAME_WIDTH) {
            const uint16_t span_len = buf[read_buf][buf_idx];
            const uint16_t colour = buf[read_buf][buf_idx+1];
            display->set_pen(colour);
            display->set_pixel_span({x, y}, span_len);

            x += span_len;

            buf_idx += 2;
            if (buf_idx == BUFFER_LEN) {
                uint next_buf_idx = (read_buf + 1) & 0xF;
                while (next_buf_idx == write_buf);
                read_buf = next_buf_idx;

                buf_idx = 0;
            }
        }
    }

    return true;
}

volatile bool run_fs = false;

void core1_main() {
    while (true) {
        multicore_fifo_pop_blocking();
        
        while (run_fs) {
            fill_video_buffer();
        }

        multicore_fifo_push_blocking(0);
    }
}

int main() {
    set_sys_clock_khz(240000, true);
    stdio_init_all();

    gpio_init(LCD_CS);
    gpio_put(LCD_CS, 1);
    gpio_set_dir(LCD_CS, 1);

    sleep_ms(5000);
    printf("Hello\n");

    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
      printf("Failed to mount SD card, error: %d\n", fr);
      return 0;
    }

    presto = new ST7701(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, frame_buffer);
    display = new PicoGraphics_PenRGB565(FRAME_WIDTH, FRAME_HEIGHT, frame_buffer);

    presto->init();

    multicore_launch_core1(core1_main);
    printf("Init\n");

    display->set_pen(0);
    display->clear();
    //presto->update(display);

    while (true) {
        fr = f_open(&fil, "/badapple480x480-565.bin", FA_READ);
        if (fr != FR_OK) {
            printf("Failed to open badapple video, error: %d\n", fr);
            return 0;
        }

        uint bytes_read;
        fr = f_read(&fil, buf[0], BUFFER_BYTES, &bytes_read);
        if (fr != FR_OK) {
            printf("Failed to read data, error: %d\n", fr);
            return 0;
        }

        write_buf = 1;
        read_buf = 0;
        buf_idx = 0;

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
            //presto->update(display);
            start_time = delayed_by_ms(start_time, (i % 3 == 2) ? 34 : 33);
        }
        run_fs = false;
        multicore_fifo_pop_blocking();

        f_close(&fil);
        f_close(&audio_file);
    }
}
