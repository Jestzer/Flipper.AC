#include <furi.h>
#include <furi_hal_infrared.h>
#include <ac_app_icons.h>

int32_t ac_app_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("TEST", "Hello world");
    FURI_LOG_I("TEST", "I'm ac_app!");

    return 0;
}
