# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (NOT DEFINED PICO_EXTRAS_PATH)
    if (EXISTS "${USERHOME}/.pico-sdk/pico-extras")
        set(PICO_EXTRAS_PATH "${USERHOME}/.pico-sdk/pico-extras" CACHE PATH "Path to pico-extras")
    else()
        include(FetchContent)
        FetchContent_Declare(
            pico_extras
            GIT_REPOSITORY https://github.com/raspberrypi/pico-extras.git
            GIT_TAG master
        )
        FetchContent_MakeAvailable(pico_extras)
        set(PICO_EXTRAS_PATH "${pico_extras_SOURCE_DIR}" CACHE PATH "Path to pico-extras")
    endif()
endif()

include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

include(FetchContent)
FetchContent_Declare(
    rp2040_i2s_example
    GIT_REPOSITORY https://github.com/malacalypse/rp2040_i2s_example.git
    GIT_TAG master
)
FetchContent_GetProperties(rp2040_i2s_example)
if (NOT rp2040_i2s_example_POPULATED)
    FetchContent_Populate(rp2040_i2s_example)
endif()

FetchContent_Declare(
    pimoroni_pico
    GIT_REPOSITORY https://github.com/pimoroni/pimoroni-pico.git
    GIT_TAG 015d8f2f6ed80dc6528333cf3ba3a83cb8420b46
)
FetchContent_GetProperties(pimoroni_pico)
if (NOT pimoroni_pico_POPULATED)
    FetchContent_Populate(pimoroni_pico)
endif()

FetchContent_Declare(
    cmsis_dsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG v1.17.0
)

project(embeded C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

set(CMSISCORE "${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core")
set(DISABLEFLOAT16 ON CACHE BOOL "Disable CMSIS-DSP float16 kernels" FORCE)
set(LOOPUNROLL ON CACHE BOOL "Enable CMSIS-DSP loop unrolling" FORCE)
FetchContent_MakeAvailable(cmsis_dsp)

include(${pimoroni_pico_SOURCE_DIR}/common/pimoroni_bus.cmake)
include(${pimoroni_pico_SOURCE_DIR}/libraries/pico_graphics/pico_graphics.cmake)
include(${pimoroni_pico_SOURCE_DIR}/drivers/uc8151/uc8151.cmake)

include_directories(${pimoroni_pico_SOURCE_DIR})
target_include_directories(pico_graphics PUBLIC ${pimoroni_pico_SOURCE_DIR})
target_include_directories(uc8151 INTERFACE ${pimoroni_pico_SOURCE_DIR})

# Add executable. Default name is the project name, version 0.1

add_executable(embeded
                src/acoustic_runtime.c
                src/audio_calibration.c
                src/audio_calibration_runtime.c
                src/audio_spectrum.c
                src/console_helpers.c
                src/device_config.c
                src/device_clock.c
                src/device_status.c
                src/diagnostics_service.c
                src/health_runtime.c
                src/http_post_client.c
                src/network_runtime.c
                src/runtime_status.c
                src/wifi_service.c
                src/startup_helpers.c
                src/time_sync_service.c
                src/time_sync_runtime.c
                src/audio_stream_queue.c
                src/ina219_wattmeter.c
                src/main.c
                src/microphone.c
                src/sd_card_buffer.c
                src/sound_event_detector.c
                src/sound_event_uploader.c
                src/inky_status_display.cpp
                src/power_meter_service.c
                src/server_health.c
                ${rp2040_i2s_example_SOURCE_DIR}/i2s.c )

pico_generate_pio_header(embeded ${rp2040_i2s_example_SOURCE_DIR}/i2s.pio)

pico_set_program_name(embeded "embeded")
pico_set_program_version(embeded "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(embeded 0)
pico_enable_stdio_usb(embeded 1)

# Add the standard library to the build
target_link_libraries(embeded
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(embeded PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${rp2040_i2s_example_SOURCE_DIR}
    ${pimoroni_pico_SOURCE_DIR}
)

# Add any user requested libraries
target_link_libraries(embeded
        pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
    CMSISDSP
    hardware_dma
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_irq
    hardware_clocks
    uc8151
    pico_graphics
        )

target_compile_options(CMSISDSP PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-O3 -ffast-math>
)

pico_add_extra_outputs(embeded)

