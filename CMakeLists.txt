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

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    eeprom
    st7735
    si5351
)

# Include MDO-1A configuration constants
include("${CMAKE_CURRENT_SOURCE_DIR}/lib/mdo-1a-config/mdo.config.cmake")

# Include firmware version and type definition file
include("${CMAKE_CURRENT_SOURCE_DIR}/version.cmake")

# Construct output filename
set(OUTPUT_FILE_NAME "${FW_MODEL}--${FW_VERSION}-${CMAKE_BUILD_TYPE}")

# Set the output filename for the executable target (.elf)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES 
    OUTPUT_NAME "${OUTPUT_FILE_NAME}"
)

# Set the base address for the application firmware after the bootloader
math(EXPR APP_BASE_ADDR "0x08000000 + ${BOOTLOADER_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Pass the model, FW version, and FW type to compiler
    BUILD_FW_VERSION="${FW_VERSION}"
    BUILD_FW_MODEL="${FW_MODEL}"
    BUILD_BOARD_HWID="${BOARD_HWID}"
    # Relocate the Vector Table Offset for bootloader compatibility
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=${BOOTLOADER_SIZE}U
)

# Pass memory layout symbols to C/C++
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    TOTAL_FLASH_SIZE=${TOTAL_FLASH_SIZE}U
    BOOTLOADER_SIZE=${BOOTLOADER_SIZE}U
    BOOTLOADER_DATA_SIZE=${BOOTLOADER_DATA_SIZE}U
    EEPROM_SIZE=${EEPROM_SIZE}U
    APP_METADATA_SIZE=${APP_METADATA_SIZE}U
)

# Pass memory layout symbols to LD
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,--defsym=__TOTAL_FLASH_SIZE__=${TOTAL_FLASH_SIZE}
    -Wl,--defsym=__BOOTLOADER_SIZE__=${BOOTLOADER_SIZE}
    -Wl,--defsym=__EEPROM_SIZE__=${EEPROM_SIZE}
    -Wl,--defsym=__APP_METADATA_SIZE__=${APP_METADATA_SIZE}
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Generate '.bin' from the compiled '.elf'
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            --gap-fill 0xFF
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.bin
    COMMENT "Generating BIN file: ${OUTPUT_FILE_NAME}.bin"
)

# Update CRC32 in '.bin' and '.elf' files after build
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/crc/update_crc.py
            ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.bin
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Calculating CRC32 and patching BIN and ELF"
)

# Generate '.uf2' from the compiled '.bin'
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/uf2/uf2conv.py
            ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.bin
            -b ${APP_BASE_ADDR}
            -f ${UF2_FAMILY}
            -o ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE_NAME}.uf2
            --convert
    COMMENT "Generating UF2 file: ${OUTPUT_FILE_NAME}.uf2"
)
