#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include "ds1307.h"

#define SDA_GPIO 21
#define SCL_GPIO 22
#define I2C_PORT 0

static const char *TAG = "ds1307_test";
static i2c_dev_t dev;

void app_main(void)
{
    // Initialize I2C
    ESP_ERROR_CHECK(ds1307_init_desc(&dev, I2C_PORT, SDA_GPIO, SCL_GPIO));

    // Check if RTC is running
    bool running;
    ESP_ERROR_CHECK(ds1307_is_running(&dev, &running));
    if (!running) {
        ESP_LOGI(TAG, "RTC is not running, starting it...");
        ESP_ERROR_CHECK(ds1307_start(&dev, true));
    }

    // Set initial time to current time: April 27, 2025 01:02:28
    struct tm time = {
        .tm_sec = 00,
        .tm_min = 12,
        .tm_hour = 19,
        .tm_mday = 27,
        .tm_mon = 3,   // 0-based, so 3 = April
        .tm_year = 137, // years since 1900 (2025 - 1900)
        .tm_wday = 0    // 0 = Sunday
    };
    ESP_ERROR_CHECK(ds1307_set_time(&dev, &time));
    ESP_LOGI(TAG, "Time set successfully");

    // Read and display time every second
    while (1) {
        struct tm time;
        ESP_ERROR_CHECK(ds1307_get_time(&dev, &time));
        ESP_LOGI(TAG, "Date: %04d-%02d-%02d (%s)",
            time.tm_year , time.tm_mon + 1, time.tm_mday,
            (time.tm_wday == 0) ? "Sunday" :
            (time.tm_wday == 1) ? "Monday" :
            (time.tm_wday == 2) ? "Tuesday" :
            (time.tm_wday == 3) ? "Wednesday" :
            (time.tm_wday == 4) ? "Thursday" :
            (time.tm_wday == 5) ? "Friday" : "Saturday");
        ESP_LOGI(TAG, "Time: %02d:%02d:%02d",
            time.tm_hour, time.tm_min, time.tm_sec);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
