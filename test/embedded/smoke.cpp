/*
 * Minimal test source for the ESP-IDF / PlatformIO / Arduino CI builds.
 */
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#include "md4c.h"
#include "md4c-html.h"

/* Parser never really needs to emit anything; we only verify the library
 * builds and links for the target. */
static void process_output(const MD_CHAR* chunk, MD_SIZE size, void* userdata)
{
    (void)chunk;
    (void)size;
    (void)userdata;
}

static void parse_smoke(void)
{
    const char* src = "# Hello md4c\n";
    int ret = md_html(src, (MD_SIZE)strlen(src), process_output, NULL,
                      MD_DIALECT_COMMONMARK, 0);

    if(ret < 0) {
        /* Never reached in CI; kept so the result of md_html() is used. */
        while(1) {
        }
    }
}

#if defined(ARDUINO)
void setup()
{
    parse_smoke();
}

void loop()
{
    delay(1000);
}
#else
extern "C" void app_main(void)
{
    parse_smoke();

    vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif