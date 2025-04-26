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
#define SERVO_FREQ 50
#define MID_OFFSET 100

// Segment positions for servos (ON/OFF pulse widths)
static const uint16_t segmentHOn[14]  = {385,375,385,375,382,375,354,367,375,385,375,368,371,375};
static const uint16_t segmentMOn[14]  = {382,395,378,315,375,340,345,380,385,365,290,365,315,365};
static const uint16_t segmentHOff[14] = {200,200,550,480,200,520,200,200,200,480,550,200,515,200};
static const uint16_t segmentMOff[14] = {200,200,550,440,200,480,200,200,200,550,450,200,430,200};
// Digit segment map: digits[d][s] = 1 if segment s is on for digit d
static const int digits[10][7] = {
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
static void updateMid(void);
static void updateDisplay(void);

static void updateMid(void) {
    // Move middle and adjacent segments for all 4 digits
    // Minute Units
    if(digits[minuteUnits][6]!=digits[prevMinuteUnits][6]) {
        if(digits[prevMinuteUnits][1]==1)
            pca9685_set_servo_pulse(&pwmM, 1, segmentMOn[1]-MID_OFFSET);
        if(digits[prevMinuteUnits][6]==1)
            pca9685_set_servo_pulse(&pwmM, 5, segmentMOn[5]+MID_OFFSET);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if(digits[minuteUnits][6]==1)
        pca9685_set_servo_pulse(&pwmM, 6, segmentMOn[6]);
    else
        pca9685_set_servo_pulse(&pwmM, 6, segmentMOff[6]);

    // Minute Tens
    if(digits[minuteTens][6]!=digits[prevMinuteTens][6]) {
        if(digits[prevMinuteTens][1]==1)
            pca9685_set_servo_pulse(&pwmM, 8, segmentMOn[8]-MID_OFFSET);
        if(digits[prevMinuteTens][6]==1)
            pca9685_set_servo_pulse(&pwmM, 12, segmentMOn[12]+MID_OFFSET);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if(digits[minuteTens][6]==1)
        pca9685_set_servo_pulse(&pwmM, 13, segmentMOn[13]);
    else
        pca9685_set_servo_pulse(&pwmM, 13, segmentMOff[13]);

    // Hour Units
    if(digits[hourUnits][6]!=digits[prevHourUnits][6]) {
        if(digits[prevHourUnits][1]==1)
            pca9685_set_servo_pulse(&pwmH, 1, segmentHOn[1]-MID_OFFSET);
        if(digits[prevHourUnits][6]==1)
            pca9685_set_servo_pulse(&pwmH, 5, segmentHOn[5]+MID_OFFSET);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if(digits[hourUnits][6]==1)
        pca9685_set_servo_pulse(&pwmH, 6, segmentHOn[6]);
    else
        pca9685_set_servo_pulse(&pwmH, 6, segmentHOff[6]);

    // Hour Tens
    if(digits[hourTens][6]!=digits[prevHourTens][6]) {
        if(digits[prevHourTens][1]==1)
            pca9685_set_servo_pulse(&pwmH, 8, segmentHOn[8]-MID_OFFSET);
        if(digits[prevHourTens][6]==1)
            pca9685_set_servo_pulse(&pwmH, 12, segmentHOn[12]+MID_OFFSET);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if(digits[hourTens][6]==1)
        pca9685_set_servo_pulse(&pwmH, 13, segmentHOn[13]);
    else
        pca9685_set_servo_pulse(&pwmH, 13, segmentHOff[13]);
}

static void updateDisplay(void) {
    updateMid();
    for (int i=0; i<=5; i++) {
        // Hour tens
        if(digits[hourTens][i]==1)
            pca9685_set_servo_pulse(&pwmH, i+7, segmentHOn[i+7]);
        else
            pca9685_set_servo_pulse(&pwmH, i+7, segmentHOff[i+7]);
        vTaskDelay(pdMS_TO_TICKS(10));
        // Hour units
        if(digits[hourUnits][i]==1)
            pca9685_set_servo_pulse(&pwmH, i, segmentHOn[i]);
        else
            pca9685_set_servo_pulse(&pwmH, i, segmentHOff[i]);
        vTaskDelay(pdMS_TO_TICKS(10));
        // Minute tens
        if(digits[minuteTens][i]==1)
            pca9685_set_servo_pulse(&pwmM, i+7, segmentMOn[i+7]);
        else
            pca9685_set_servo_pulse(&pwmM, i+7, segmentMOff[i+7]);
        vTaskDelay(pdMS_TO_TICKS(10));
        // Minute units
        if(digits[minuteUnits][i]==1)
            pca9685_set_servo_pulse(&pwmM, i, segmentMOn[i]);
        else
            pca9685_set_servo_pulse(&pwmM, i, segmentMOff[i]);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // I2C bus init
    ESP_ERROR_CHECK(i2c_master_init(I2C_PORT, SDA_GPIO, SCL_GPIO));
    // DS1307 RTC init
    ESP_ERROR_CHECK(ds1307_init_desc(&ds1307_dev, I2C_PORT, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(ds1307_start(&ds1307_dev, true));
    // PCA9685 Hour driver init
    ESP_ERROR_CHECK(pca9685_init(&pwmH, I2C_PORT, PCA9685_ADDR_H));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pwmH, SERVO_FREQ));
    // PCA9685 Minute driver init
    ESP_ERROR_CHECK(pca9685_init(&pwmM, I2C_PORT, PCA9685_ADDR_M));
    ESP_ERROR_CHECK(pca9685_set_frequency(&pwmM, SERVO_FREQ));
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
    while (1) {
        ESP_ERROR_CHECK(ds1307_get_time(&ds1307_dev, &rtc_time));
        int temp;
        temp = rtc_time.tm_hour;
        hourTens = temp / 10;
        hourUnits = temp % 10;
        temp = rtc_time.tm_min;
        minuteTens = temp / 10;
        minuteUnits = temp % 10;
        if(minuteUnits != prevMinuteUnits) {
            updateDisplay();
        }
        prevHourTens = hourTens;
        prevHourUnits = hourUnits;
        prevMinuteTens = minuteTens;
        prevMinuteUnits = minuteUnits;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

