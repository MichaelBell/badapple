#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7701/st7701.hpp"

#include "ff.h"
#include "uzlib.h"

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
#define BUFFER_LEN 1024
#define BUFFER_BYTES (BUFFER_LEN*2)
uint16_t buf[NUM_BUFFERS * BUFFER_LEN];
volatile uint write_buf;
volatile uint read_buf;
uint buf_idx;
bool data_starve;

uint8_t deflate_buf[BUFFER_BYTES];
struct uzlib_uncomp deflater;

static int deflate_read_cb(struct uzlib_uncomp *uncomp) {
    uint bytes_read;
    fr = f_read(&fil, deflate_buf, BUFFER_BYTES, &bytes_read);
    if (fr != FR_OK) {
        printf("Failed to read data, error: %d\n", fr);
        return -1;
    }
    if (bytes_read <= 0) {
        printf("EOF\n");
        return -1;
    }

    //printf("%d\n", bytes_read);

    uncomp->source_limit = deflate_buf + bytes_read;
    uncomp->source = &deflate_buf[1];
    return deflate_buf[0];
}

static void setup_video_decompression() {
    fr = f_open(&fil, "/badapple480x480-565.bin.gz", FA_READ);
    if (fr != FR_OK) {
        printf("Failed to open badapple video, error: %d\n", fr);
        exit(1);
    }

    uint bytes_read;
    fr = f_read(&fil, deflate_buf, BUFFER_BYTES, &bytes_read);
    if (fr != FR_OK) {
        printf("Failed to read data, error: %d\n", fr);
        exit(1);
    }

    uzlib_uncompress_init(&deflater);
    deflater.source = deflate_buf;
    deflater.source_limit = deflate_buf + BUFFER_BYTES;
    deflater.source_read_cb = &deflate_read_cb;

    int res = uzlib_gzip_parse_header(&deflater);
    if (res != TINF_OK) {
        printf("Error parsing header: %d\n", res);
        exit(1);
    }

    deflater.dest_start = deflater.dest = (uint8_t*)&buf[0];
    deflater.dest_limit = deflater.dest_start + BUFFER_BYTES;
    deflater.dest_ring_start = (uint8_t*)&buf[0];
    deflater.dest_ring_end = (uint8_t*)&buf[NUM_BUFFERS * BUFFER_LEN];

    res = uzlib_uncompress(&deflater);
    if (res != TINF_OK) {
        printf("Error decompressing first block: %d\n", res);
        exit(1);
    }

    printf("Here %04x %04x %04x %04x\n", buf[0], buf[1], buf[2], buf[3]);

    write_buf = 0;
    read_buf = 0;
}

static void fill_video_buffer() {
    uint next_buf_idx = (write_buf + 1) & 0xF;
    if (next_buf_idx == read_buf) return;

    //printf("Decomp: %p %p %p %p %p\n", deflater.source, deflater.source_limit, deflater.dest, deflater.dest_ring_end, (uint8_t*)&buf[next_buf_idx * BUFFER_LEN]);

    deflater.dest_start = deflater.dest = (uint8_t*)&buf[next_buf_idx * BUFFER_LEN];
    deflater.dest_limit = deflater.dest_start + BUFFER_BYTES;

    int res = uzlib_uncompress(&deflater);
    if (res != TINF_OK && res != TINF_DONE) {
        printf("Error decompressing block: %d\n", res);
    }

    write_buf = next_buf_idx;
}

static bool display_frame() {
    uint16_t* ptr = frame_buffer;
    for (int y = 0; y < FRAME_HEIGHT; ++y)
    {
        int x = 0;
        while (x < FRAME_WIDTH) {
            uint16_t span_len = buf[read_buf * BUFFER_LEN + buf_idx];
            const uint16_t colour = buf[read_buf * BUFFER_LEN + buf_idx+1];
            
            x += span_len;

            if (x > FRAME_WIDTH) {
                printf("Span error\n");
                span_len -= x - FRAME_WIDTH;
            }

            while (span_len--) *ptr++ = colour;

            buf_idx += 2;
            if (buf_idx == BUFFER_LEN) {
                uint next_buf_idx = (read_buf + 1) & 0xF;
                while (next_buf_idx == write_buf) data_starve = true;
                read_buf = next_buf_idx;

                buf_idx = 0;
            }
        }
    }

    return true;
}

volatile bool run_fs = false;

void core1_main() {

    uzlib_init();

    presto->init();

    while (true) {
        multicore_fifo_pop_blocking();
        setup_video_decompression();
        fill_video_buffer();
        multicore_fifo_push_blocking(0);

        while (run_fs) {
            fill_video_buffer();
        }

        f_close(&fil);

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

    multicore_launch_core1(core1_main);
    printf("Init\n");

    display->set_pen(0);
    display->clear();
    //presto->update(display);

    while (true) {
        write_buf = 1;
        read_buf = 0;
        buf_idx = 0;

        run_fs = true;
        multicore_fifo_push_blocking(0);
        multicore_fifo_pop_blocking();

        absolute_time_t start_time = get_absolute_time();
        for (int i = 0; i < 6950; ++i) {
            display_frame();
            
            absolute_time_t sleep_to_time = delayed_by_ms(start_time, 30);
            if (absolute_time_diff_us(get_absolute_time(), sleep_to_time) > 1000) {
                sleep_until(sleep_to_time);
            }
            else {
                printf("Frame %d time %lldms%s\n", i, absolute_time_diff_us(start_time, get_absolute_time()) / 1000, data_starve ? " (data)" : "");
            }
            data_starve = false;
            //presto->update(display);
            presto->wait_for_vsync();
            start_time = delayed_by_ms(start_time, (i % 3 == 2) ? 34 : 33);
        }
        run_fs = false;
        multicore_fifo_pop_blocking();
    }
}
