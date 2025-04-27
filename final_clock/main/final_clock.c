
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <time.h>
#include <string.h>
#include "ds1307.h"
#include "pca9685.h"
#include "HD44780.h"
#include "freertos/portmacro.h"
#include "sdkconfig.h"
#include <driver/i2c.h>

// I2C Configuration
#define SDA_GPIO 21
#define SCL_GPIO 22
#define I2C_PORT I2C_NUM_0

// LCD Configuration
#define LCD_ADDR 0x27  // Common I2C address for PCF8574
#define LCD_COLS 16
#define LCD_ROWS 2

// PCA9685 Addresses
#define PCA1_ADDR 0x40  // First controller (digits 1-2)
#define PCA2_ADDR 0x41  // Second controller (digits 3-4)

// Segment to servo mapping (7 servos per digit)
#define DIGIT1_SEGMENTS {0, 1, 2, 3, 4, 5, 6}    // First digit (PCA1)
#define DIGIT2_SEGMENTS {7, 8, 9, 10, 11, 12, 13} // Second digit (PCA1)
#define DIGIT3_SEGMENTS {0, 1, 2, 3, 4, 5, 6}    // Third digit (PCA2)
#define DIGIT4_SEGMENTS {7, 8, 9, 10, 11, 12, 13} // Fourth digit (PCA2)

static const char *TAG = "final_clock";
static i2c_dev_t dev;

// Day names
const char *day_names[7] = {
    "Sunday", "Monday", "Tuesday", 
    "Wednesday", "Thursday", "Friday", "Saturday"
};

// 7-segment patterns (common cathode)
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

void set_digit(pca9685_dev_t *pca, const uint8_t segments[7], uint8_t digit, 
               uint16_t pulse_0deg, uint16_t pulse_90deg) {
    uint8_t pattern = digit_patterns[digit % 10];
    for (int seg = 0; seg < 7; seg++) {
        uint16_t pulse = (pattern & (1 << seg)) ? pulse_90deg : pulse_0deg;
        pca9685_set_servo_pulse(pca, segments[seg], pulse);
    }
}

void clock_display_task(void *param) {
    char date_str[16];
    char day_str[10];
    
    while (1) {
        struct tm time;
        if (ds1307_get_time(&dev, &time) == ESP_OK) {
            // Format date and day
            strftime(date_str, sizeof(date_str), "Date: %d/%m", &time);
            strncpy(day_str, day_names[time.tm_wday], sizeof(day_str)-1);
            day_str[sizeof(day_str)-1] = '\0';
            
            // Update LCD display
            LCD_clearScreen();
            LCD_setCursor(0, 0);
            LCD_writeStr(date_str);
            LCD_setCursor(0, 1);
            LCD_writeStr(day_str);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C bus initialized");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize LCD
    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_init(LCD_ADDR, SDA_GPIO, SCL_GPIO, LCD_COLS, LCD_ROWS);
    LCD_clearScreen();
    LCD_writeStr("Clock Starting...");
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Initialize DS1307 RTC
    ESP_LOGI(TAG, "Initializing DS1307...");
    esp_err_t rtc_err = ds1307_init_desc(&dev, I2C_PORT, SDA_GPIO, SCL_GPIO);
    if (rtc_err != ESP_OK) {
        ESP_LOGE(TAG, "DS1307 init failed: %s", esp_err_to_name(rtc_err));
    } else {
        ESP_LOGI(TAG, "DS1307 initialized successfully");
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize PCA9685 controllers
    ESP_LOGI(TAG, "Initializing PCA9685 controllers...");
    pca9685_dev_t pca1, pca2;

    // First PCA (digits 1-2)
    ESP_ERROR_CHECK(pca9685_init(&pca1, I2C_PORT, PCA1_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca1, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized (digits 1-2)", PCA1_ADDR);
    
    // Second PCA (digits 3-4)
    ESP_ERROR_CHECK(pca9685_init(&pca2, I2C_PORT, PCA2_ADDR));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pca2, 50));
    ESP_LOGI(TAG, "PCA9685 @ 0x%02X initialized (digits 3-4)", PCA2_ADDR);
    
    vTaskDelay(500 / portTICK_PERIOD_MS);

    const uint16_t pulse_0deg = 500;  // 0 degrees position
    const uint16_t pulse_90deg = 1500; // 90 degrees position
    
    // Segment mappings for each digit
    const uint8_t digit1_segments[7] = DIGIT1_SEGMENTS;
    const uint8_t digit2_segments[7] = DIGIT2_SEGMENTS;
    const uint8_t digit3_segments[7] = DIGIT3_SEGMENTS;
    const uint8_t digit4_segments[7] = DIGIT4_SEGMENTS;

    // Create clock display task
    xTaskCreate(&clock_display_task, "Clock Display", 2048, NULL, 5, NULL);

    while (1) {
        struct tm time;
        if (ds1307_get_time(&dev, &time) == ESP_OK) {
            // Extract digits from time
            uint8_t hour_tens = time.tm_hour / 10;
            uint8_t hour_units = time.tm_hour % 10;
            uint8_t min_tens = time.tm_min / 10;
            uint8_t min_units = time.tm_min % 10;

            ESP_LOGI(TAG, "Time: %02d:%02d", time.tm_hour, time.tm_min);
            
            // Update all 4 digits
            set_digit(&pca1, digit1_segments, hour_tens, pulse_0deg, pulse_90deg);
            set_digit(&pca1, digit2_segments, hour_units, pulse_0deg, pulse_90deg);
            set_digit(&pca2, digit3_segments, min_tens, pulse_0deg, pulse_90deg);
            set_digit(&pca2, digit4_segments, min_units, pulse_0deg, pulse_90deg);
        } else {
            ESP_LOGE(TAG, "Failed to read time from DS1307");
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}