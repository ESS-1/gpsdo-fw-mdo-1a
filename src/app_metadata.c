#include <stdint.h>

typedef struct __attribute__((packed)) {
    char     hardware_id[10] __attribute__((nonstring));
    char     app_version[10] __attribute__((nonstring));
} app_metadata_t;

__attribute__((section(".app_metadata"), used)) const app_metadata_t g_app_metadata = {
    .hardware_id = BUILD_FW_HWID,
    .app_version = BUILD_FW_VERSION,
};
