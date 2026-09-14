
#include <stdint.h>

typedef struct __attribute__((packed)) {
    char     hardware_id[10];
    char     app_version[10];
    uint32_t crc32;
} app_metadata_t;

__attribute__((section(".metadata"), used)) const app_metadata_t g_app_metadata = {
    .hardware_id = BUILD_FW_HWID,
    .app_version = BUILD_FW_VERSION,
    .crc32       = 0xCCCCCCCC, // Placeholder for CRC32, to be filled in by the build system
};
