#include "HD44780.h"
#include "freertos/portmacro.h"
#include "sdkconfig.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#define LCD_ADDR 0x27
#define SDA_PIN 21
#define SCL_PIN 22
#define LCD_COLS 16
#define LCD_ROWS 2

static char tag[] = "LCD test";
void LCD_DemoTask(void *param);

void app_main(void) {
  ESP_LOGI(tag, "Starting up application");
  LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);
  vTaskDelay(pdMS_TO_TICKS(100));
  xTaskCreate(&LCD_DemoTask, "Demo Task", 2048, NULL, 5, NULL);
}

void LCD_DemoTask(void *param) {
  char txtBuf[8];
  while (true) {
    int row = 0, col = 0;
    LCD_home();
    LCD_clearScreen();
    LCD_writeStr("16x2 LCD Demo");
    LCD_setCursor(0, 1);
    LCD_writeStr("ESP32 Test");
    LCD_setCursor(10, 1);
    LCD_writeStr("Time: ");
    for (int i = 10; i >= 0; i--) {
      LCD_setCursor(15, 1);
      sprintf(txtBuf, "%02d", i);
      printf("%s\n", txtBuf);
      LCD_writeStr(txtBuf);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    for (int i = 0; i < 32; i++) {
      LCD_clearScreen();
      LCD_setCursor(col, row);
      LCD_writeChar('*');

      if (i >= 15) {
        row = (i + 1) / 16;
      }
      if (col++ >= 15) {
        col = 0;
      }

      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}
