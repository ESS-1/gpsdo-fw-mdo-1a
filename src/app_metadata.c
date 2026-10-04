#include "app_metadata_defs.h"

__attribute__((section(".app_metadata"), used)) const app_metadata_t g_app_metadata = {
    .hardware_id = BUILD_BOARD_HWID,
    .app_version = BUILD_FW_VERSION,
};
