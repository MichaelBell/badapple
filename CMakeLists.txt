cmake_minimum_required(VERSION 3.12)

# Change your executable name to something creative!
set(NAME badapple) # <-- Name your project/executable here!

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()
include(picovision_import.cmake)

# Add your source files
add_executable(${NAME}
    main.cpp # <-- Add source files here!
)

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(common/pimoroni_i2c)
include(drivers/fatfs/fatfs)
include(drivers/sdcard/sdcard)

# Don't forget to link the libraries you need!
target_link_libraries(${NAME}
    picovision
    pico_stdlib
    pico_audio_i2s
    pico_multicore
    pimoroni_i2c
    sdcard
    fatfs
)

target_compile_definitions(${NAME} PRIVATE
  SDCARD_SPI_BUS=spi1
  SDCARD_PIN_SPI0_CS=15
  SDCARD_PIN_SPI0_SCK=10
  SDCARD_PIN_SPI0_MOSI=11
  SDCARD_PIN_SPI0_MISO=12
  PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
  PICO_AUDIO_I2S_DATA_PIN=26
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
