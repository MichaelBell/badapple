cmake_minimum_required(VERSION 3.12)

set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pimoroni_pico_plus2_w_rp2350)

# Change your executable name to something creative!
set(NAME badapple) # <-- Name your project/executable here!

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

if (NOT PIMORONI_PRESTO_PATH)
    set(PIMORONI_PRESTO_PATH ../../presto/)
endif()
if(NOT IS_ABSOLUTE ${PIMORONI_PRESTO_PATH})
    get_filename_component(
        PIMORONI_PRESTO_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/${PIMORONI_PRESTO_PATH}"
        ABSOLUTE)
endif()
message("PIMORONI_PRESTO_PATH is ${PIMORONI_PRESTO_PATH}")
set(PIMORONI_PRESTO_PATH ${PIMORONI_PRESTO_PATH} CACHE PATH "Path to the Presto libraries" FORCE)
include_directories(${PIMORONI_PRESTO_PATH})
list(APPEND CMAKE_MODULE_PATH ${PIMORONI_PRESTO_PATH})
include(drivers/st7701/st7701_presto)

include_directories(${CMAKE_CURRENT_LIST_DIR}/uzlib/src)

# Add your source files
add_executable(${NAME}
    main.cpp # <-- Add source files here!
    a2dp_source_demo.c
    picow_bt_example_common.c
    sfe_psram.c

    uzlib/src/tinflate.c
    uzlib/src/tinfgzip.c
    uzlib/src/tinfzlib.c
    uzlib/src/adler32.c
    uzlib/src/crc32.c
    uzlib/src/defl_static.c
    uzlib/src/genlz77.c
)

target_include_directories(${NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    )

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(drivers/fatfs/fatfs)
include(drivers/sdcard/sdcard)
include(libraries/pico_graphics/pico_graphics)

# Don't forget to link the libraries you need!
target_link_libraries(${NAME}
    st7701_presto
    pico_stdlib
    pico_multicore
    pimoroni_i2c
    sdcard
    fatfs
    hardware_interp
    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_cyw43
    pico_cyw43_arch_threadsafe_background
    pico_btstack_sbc_encoder    
)

target_compile_definitions(${NAME} PRIVATE
  SDCARD_SPI_BUS=spi0
  SDCARD_PIN_SPI0_CS=39
  SDCARD_PIN_SPI0_SCK=34
  SDCARD_PIN_SPI0_MOSI=35
  SDCARD_PIN_SPI0_MISO=36
  SDCARD_CLK_MHZ=33
  PICO_CLOCK_AJDUST_PERI_CLOCK_WITH_SYS_CLOCK=1
  CYW43_LWIP=0
  CYW43_PIO_CLOCK_DIV_INT=3
  )


# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

pico_enable_stdio_uart(${NAME} 0)
pico_enable_stdio_usb(${NAME} 1)

# Set up files for the release packages
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
