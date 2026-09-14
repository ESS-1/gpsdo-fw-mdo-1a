cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME gpsdo)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(lib)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/app_metadata.c
    src/main.c
    src/eeprom.c
    src/frequency.c
    src/gps.c
    src/int.c
    src/bootlog.c
    src/cdcio.c
    src/pll.c
    src/pll_presets.c
    src/icons.c
    src/ui_core.c
    src/ui_helpers.c
    src/ui_msgbox.c
    src/ui.c
    src/encoder.c
    src/timer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    eeprom
    st7735
    si5351
)

# Include firmware version and type definition file
include("${CMAKE_CURRENT_SOURCE_DIR}/version.cmake")

# Construct output filename
set(OUTPUT_FILE_NAME "${FW_MODEL}--${FW_VERSION}-${FW_TYPE}-${CMAKE_BUILD_TYPE}")

# Set the output filename for the executable target (.elf)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES 
    OUTPUT_NAME "${OUTPUT_FILE_NAME}"
)


# ==============================================================================
# Flash Memory Layout Configuration
# ==============================================================================
# Sizes defined in Kilobytes (KB)
set(TOTAL_FLASH_KB      128)        # Total Flash memory
set(BOOTLOADER_KB       16)         # Reserved bootloader space
set(EEPROM_KB           1)          # Reserved space for EEPROM emulation
set(METADATA_BYTES      24)         # Reserved space for metadata (HW ID, CRC)


# Convert to byte offsets and addresses
math(EXPR BOOT_OFFSET_BYTES "${BOOTLOADER_KB} * 1024")
math(EXPR BOOT_OFFSET_HEX   "${BOOT_OFFSET_BYTES}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_BASE_ADDR     "0x08000000 + ${BOOT_OFFSET_BYTES}" OUTPUT_FORMAT HEXADECIMAL)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Pass the model, FW version, and FW type to compiler
    BUILD_FW_VERSION="${FW_VERSION}"
    BUILD_FW_TYPE="${FW_TYPE}"
    BUILD_FW_MODEL="${FW_MODEL}"
    BUILD_FW_HWID="${FW_HWID}"
    # Relocate the Vector Table Offset for bootloader compatibility
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=${BOOT_OFFSET_HEX}U
)

# Pass memory layout symbols to LD
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,--defsym=__TOTAL_FLASH__=${TOTAL_FLASH_KB}K
    -Wl,--defsym=__BOOT_SIZE__=${BOOTLOADER_KB}K
    -Wl,--defsym=__EEPROM_SIZE__=${EEPROM_KB}K
    -Wl,--defsym=__METADATA_SIZE__=${METADATA_BYTES}
)

# Generate '.bin' from the compiled '.elf'
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.bin
    COMMENT "Generating BIN file: ${OUTPUT_FILE_NAME}.bin"
)

# Generate '.uf2' from the compiled '.bin'
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/uf2/uf2conv.py
            ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.bin
            -b ${APP_BASE_ADDR}
            -f ${UF2_FAMILY_ID}
            -o ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.uf2
            --convert
    COMMENT "Generating UF2 file: ${OUTPUT_FILE_NAME}.uf2"
)
