#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <time.h>
#include "ds1307.h"
#include "pca9685.h"

// Single I2C bus configuration
#define SDA_GPIO 21
#define SCL_GPIO 22
#define I2C_PORT I2C_NUM_0

// PCA9685 addresses
#define PCA1_ADDR 0x40  // Default address (A0-A5 all low)
#define PCA2_ADDR 0x41  // A0 high, others low

static const char *TAG = "7_segment_display";
static i2c_dev_t dev;

// 7-segment display patterns (common cathode)
const uint8_t digit_patterns[10] = {
    0x3F, // 0 - ABCDEF
    0x06, // 1 - BC
    0x5B, // 2 - ABGED
    0x4F, // 3 - ABGCD
    0x66, // 4 - FBGC
    0x6D, // 5 - AFGCD
    0x7D, // 6 - AFGECD
    0x07, // 7 - ABC
    0x7F, // 8 - ABCDEFG
    0x6F  // 9 - ABCDFG
};

void set_digit(pca9685_dev_t *pca, uint8_t digit, uint16_t pulse_0deg, uint16_t pulse_90deg) {
    uint8_t pattern = digit_patterns[digit % 10];
    for (int seg = 0; seg < 7; seg++) {
        uint16_t pulse = (pattern & (1 << seg)) ? pulse_90deg : pulse_0deg;
        pca9685_set_servo_pulse(pca, seg, pulse);
    }
}

void app_main(void) {   
    // Configure single I2C bus
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C bus initialized");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize DS1307
    ESP_LOGI(TAG, "Initializing DS1307...");
    esp_err_t rtc_err = ds1307_init_desc(&dev, I2C_PORT, SDA_GPIO, SCL_GPIO);
    if (rtc_err != ESP_OK) {
        ESP_LOGE(TAG, "DS1307 init failed: %s", esp_err_to_name(rtc_err));
    } else {
        ESP_LOGI(TAG, "DS1307 initialized successfully");
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Initializing PCA9685 controllers...");
    pca9685_dev_t pca1, pca2;

    // Initialize PCA1 (0x40)
    ESP_ERROR_CHECK(pca9685_init(&pca1, I2C_PORT, PCA1_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca1, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized", PCA1_ADDR);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize PCA2 (0x41)
    ESP_ERROR_CHECK(pca9685_init(&pca2, I2C_PORT, PCA2_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca2, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized", PCA2_ADDR);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    const uint16_t pulse_0deg = 500;  // 0 degrees position
    const uint16_t pulse_90deg = 1500; // 90 degrees position

    while (1) {
        // Count from 0 to 9 on both displays
        for (int digit = 0; digit < 10; digit++) {
            ESP_LOGI(TAG, "Displaying digit %d", digit);
            
            // Set digit on first display (PCA1)
            set_digit(&pca1, digit, pulse_0deg, pulse_90deg);
            
            // Set digit on second display (PCA2)
            set_digit(&pca2, digit, pulse_0deg, pulse_90deg);
            
            // Wait 10 seconds before showing next digit
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}
