#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include "pca9685.h"

#define I2C_MASTER_SCL_IO           22      // GPIO for I2C SCL
#define I2C_MASTER_SDA_IO           21      // GPIO for I2C SDA
#define I2C_MASTER_FREQ_HZ          400000  // I2C master clock frequency
#define I2C_MASTER_NUM              I2C_NUM_0
#define NUM_SERVOS                  32      // Total number of servos to control

// Test patterns for servos
typedef enum {
    PATTERN_SEQUENTIAL,   // Move one servo at a time
    PATTERN_WAVE,         // Create a wave-like motion
    PATTERN_ALL_TOGETHER  // Move all servos together
} test_pattern_t;

void init_i2c(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

// Function to move a single servo
void move_servo(pca9685_dev_t *dev, uint8_t servo_num, uint16_t pulse_width) {
    if (servo_num < NUM_SERVOS) {
        ESP_ERROR_CHECK(pca9685_set_servo_pulse(dev, servo_num % 16, pulse_width));
    }
}

// Function to run a specific test pattern
void run_test_pattern(pca9685_dev_t *dev, test_pattern_t pattern) {
    const uint16_t positions[] = {500, 1500, 2500}; // 0°, 90°, 180°
    
    switch (pattern) {
        case PATTERN_SEQUENTIAL:
            printf("Running sequential pattern test...\n");
            for (int pos = 0; pos < 3; pos++) {
                for (int servo = 0; servo < NUM_SERVOS; servo++) {
                    printf("Moving servo %d to position %d\n", servo, pos);
                    move_servo(dev, servo, positions[pos]);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
            }
            break;
            
        case PATTERN_WAVE:
            printf("Running wave pattern test...\n");
            for (int cycle = 0; cycle < 3; cycle++) {
                for (int pos = 0; pos < 3; pos++) {
                    for (int servo = 0; servo < NUM_SERVOS; servo++) {
                        int wave_pos = (pos + servo) % 3;
                        move_servo(dev, servo, positions[wave_pos]);
                    }
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }
            break;
            
        case PATTERN_ALL_TOGETHER:
            printf("Running all-together pattern test...\n");
            for (int pos = 0; pos < 3; pos++) {
                printf("Moving all servos to position %d\n", pos);
                for (int servo = 0; servo < NUM_SERVOS; servo++) {
                    move_servo(dev, servo, positions[pos]);
                    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent power spike
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            break;
    }
}

void app_main(void) {
    // Initialize I2C
    init_i2c();

    // Initialize PCA9685
    pca9685_dev_t dev;
    ESP_ERROR_CHECK(pca9685_init(&dev, I2C_MASTER_NUM, PCA9685_DEFAULT_ADDRESS));
    
    // Set PWM frequency to 50Hz (standard for servos)
    ESP_ERROR_CHECK(pca9685_set_frequency(&dev, 50));

    printf("PCA9685 initialized. Starting servo test patterns...\n");

    while (1) {
        // Run each test pattern
        printf("\n=== Starting new test cycle ===\n");
        
        // Pattern 1: Sequential movement
        run_test_pattern(&dev, PATTERN_SEQUENTIAL);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Pattern 2: Wave pattern
        run_test_pattern(&dev, PATTERN_WAVE);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Pattern 3: All servos together
        run_test_pattern(&dev, PATTERN_ALL_TOGETHER);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        printf("Test cycle completed. Starting next cycle...\n\n");
    }
}
