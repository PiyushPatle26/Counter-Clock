#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include "pca9685.h"

#define SDA_GPIO 21
#define SCL_GPIO 22
#define I2C_PORT I2C_NUM_0
#define PCA1_ADDR 0x40
#define PCA2_ADDR 0x41
#define SERVOS_PER_PCA 16
#define TOTAL_SERVOS 32  // 16 per PCA * 2 controllers

static const char *TAG = "servo_reset";

void app_main(void)
{
    // Configure I2C
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

    // Initialize PCA9685 controllers
    pca9685_dev_t pca1, pca2;
    
    // First PCA at 0x40
    ESP_ERROR_CHECK(pca9685_init(&pca1, I2C_PORT, PCA1_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca1, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized", PCA1_ADDR);
    
    // Second PCA at 0x41
    ESP_ERROR_CHECK(pca9685_init(&pca2, I2C_PORT, PCA2_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca2, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized", PCA2_ADDR);
    
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Set all servos to 0 degrees (500µs pulse)
    const uint16_t pulse_0deg = 500;
    const uint16_t pulse_90deg = 1500;

    ESP_LOGI(TAG, "Resetting all 32 servos to 0 degrees");
    
    // Reset first PCA (channels 0-15)
    for (int ch = 0; ch < SERVOS_PER_PCA; ch++) {
        pca9685_set_servo_pulse(&pca1, ch, pulse_0deg);
    }
    
    // Reset second PCA (channels 16-31)
    for (int ch = 0; ch < SERVOS_PER_PCA; ch++) {
        pca9685_set_servo_pulse(&pca2, ch, pulse_0deg);
    }

    ESP_LOGI(TAG, "All 32 servos reset to 0 degrees position");
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}