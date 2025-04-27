#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "ds1307.h"
#include "pca9685.h"
#include "i2cdev.h"

// --- Configuration ---
#define I2C_PORT I2C_NUM_0
#define SDA_GPIO 21
#define SCL_GPIO 22
#define PCA9685_ADDR_H 0x40
#define PCA9685_ADDR_M 0x41
#define DS1307_ADDR 0x68 // For reference only, not used in init
#define SERVO_FREQ 50
#define MID_OFFSET 100

// I2C config struct
static const i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = SDA_GPIO,
    .scl_io_num = SCL_GPIO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000
};

// Segment positions for servos (ON/OFF pulse widths)
static const uint16_t segmentHOn[14]  = {385,375,385,375,382,375,354,367,375,385,375,368,371,375};
static const uint16_t segmentMOn[14]  = {382,395,378,315,375,340,345,380,385,365,290,365,315,365};
static const uint16_t segmentHOff[14] __attribute__((unused)) = {200,200,550,480,200,520,200,200,200,480,550,200,515,200};
static const uint16_t segmentMOff[14] __attribute__((unused)) = {200,200,550,440,200,480,200,200,200,550,450,200,430,200};
// Digit segment map: digits[d][s] = 1 if segment s is on for digit d
static const int digits[10][7] __attribute__((unused)) = {
    {1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0},
    {1,1,0,1,1,0,1},
    {1,1,1,1,0,0,1},
    {0,1,1,0,0,1,1},
    {1,0,1,1,0,1,1},
    {1,0,1,1,1,1,1},
    {1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1},
    {1,1,1,1,0,1,1}
};

// State variables
static int hourTens = 0, hourUnits = 0, minuteTens = 0, minuteUnits = 0;
static int prevHourTens = 8, prevHourUnits = 8, prevMinuteTens = 8, prevMinuteUnits = 8;

// PCA9685 device structs
static pca9685_dev_t pwmH, pwmM;
static i2c_dev_t ds1307_dev;

// --- Helper Functions ---
static void updateMid(void) __attribute__((unused));
static void updateDisplay(void);

void updateMid(void) {
    // ... (unchanged, your servo update logic)
}

void updateDisplay(void) {
    // ... (unchanged, your display update logic)
}

void app_main(void)
{
    esp_err_t err;

    // I2C bus init
    err = i2c_param_config(I2C_PORT, &conf);
    ESP_LOGI("I2C", "i2c_param_config: %s", esp_err_to_name(err));
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "i2c_param_config failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    ESP_LOGI("I2C", "i2c_driver_install: %s", esp_err_to_name(err));
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "i2c_driver_install failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }

    // --- DS1307 RTC init (matches test project) ---
    ESP_LOGI("DS1307", "Initializing DS1307: I2C_PORT=%d, SDA_GPIO=%d, SCL_GPIO=%d", I2C_PORT, SDA_GPIO, SCL_GPIO);
    err = ds1307_init_desc(&ds1307_dev, I2C_PORT, SDA_GPIO, SCL_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE("DS1307", "ds1307_init_desc failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    // Check if RTC is running
    bool running = false;
    err = ds1307_is_running(&ds1307_dev, &running);
    if (err != ESP_OK) {
        ESP_LOGE("DS1307", "ds1307_is_running failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    if (!running) {
        ESP_LOGI("DS1307", "RTC is not running, starting it...");
        err = ds1307_start(&ds1307_dev, true);
        if (err != ESP_OK) {
            ESP_LOGE("DS1307", "ds1307_start failed: %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(err);
        }
    } else {
        ESP_LOGI("DS1307", "RTC already running");
    }

    // --- PCA9685 Hour driver init ---
    err = pca9685_init(&pwmH, I2C_PORT, PCA9685_ADDR_H);
    if (err != ESP_OK) {
        ESP_LOGE("PCA9685-H", "pca9685_init failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    err = pca9685_set_frequency(&pwmH, SERVO_FREQ);
    if (err != ESP_OK) {
        ESP_LOGE("PCA9685-H", "set_frequency failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    // --- PCA9685 Minute driver init ---
    err = pca9685_init(&pwmM, I2C_PORT, PCA9685_ADDR_M);
    if (err != ESP_OK) {
        ESP_LOGE("PCA9685-M", "pca9685_init failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
    err = pca9685_set_frequency(&pwmM, SERVO_FREQ);
    if (err != ESP_OK) {
        ESP_LOGE("PCA9685-M", "set_frequency failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }

    // Set all servos to ON (display 88:88)
    for(int i=0; i<=13; i++) {
        pca9685_set_servo_pulse(&pwmH, i, segmentHOn[i]);
        vTaskDelay(pdMS_TO_TICKS(10));
        pca9685_set_servo_pulse(&pwmM, i, segmentMOn[i]);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Main loop
    struct tm rtc_time;
    static int prevSecond = -1;
    while (1) {
        err = ds1307_get_time(&ds1307_dev, &rtc_time);
        if (err != ESP_OK) {
            ESP_LOGE("DS1307", "get_time failed: %s", esp_err_to_name(err));
            continue;
        }
        int temp;
        temp = rtc_time.tm_hour;
        hourTens = temp / 10;
        hourUnits = temp % 10;
        temp = rtc_time.tm_min;
        minuteTens = temp / 10;
        minuteUnits = temp % 10;
        if (rtc_time.tm_sec != prevSecond) {
            updateDisplay();
            prevSecond = rtc_time.tm_sec;
        }
        prevHourTens = hourTens;
        prevHourUnits = hourUnits;
        prevMinuteTens = minuteTens;
        prevMinuteUnits = minuteUnits;
        vTaskDelay(pdMS_TO_TICKS(100)); // Faster refresh for better precision
    }
}