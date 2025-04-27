#include "driver/i2c.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define I2C_PORT I2C_NUM_0
#define SDA_GPIO 21
#define SCL_GPIO 22
#define DS1307_ADDR 0x68

void scan_i2c_devices() {
    ESP_LOGI("I2C", "Scanning I2C bus...");
    int found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, 100 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI("I2C", "Found device at: 0x%02X", addr);
            found++;
            
            // Special check for DS1307
            if (addr == DS1307_ADDR) {
                ESP_LOGI("DS1307", "DS1307 detected at 0x%02X", addr);
            }
        }
    }
    
    if (found == 0) {
        ESP_LOGE("I2C", "No I2C devices found!");
    }
}

void check_signal_quality() {
    ESP_LOGI("SIGNAL", "Checking SDA/SCL line states...");
    
    // Check if lines are being pulled up properly
    gpio_set_direction(SDA_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(SCL_GPIO, GPIO_MODE_INPUT);
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    int sda_state = gpio_get_level(SDA_GPIO);
    int scl_state = gpio_get_level(SCL_GPIO);
    
    if (sda_state == 1 && scl_state == 1) {
        ESP_LOGI("SIGNAL", "Signal Quality: GOOD (Both lines high)");
    } else {
        ESP_LOGE("SIGNAL", "Signal Quality: POOR (SDA:%d, SCL:%d) - Check pull-up resistors", sda_state, scl_state);
    }
}

void app_main(void) {
    // I2C Configuration
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    
    ESP_LOGI("I2C", "Starting I2C Diagnostics");
    
    // 1. Initialize I2C
    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Config failed: %s", esp_err_to_name(err));
        return;
    }
    
    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Driver install failed: %s", esp_err_to_name(err));
        return;
    }
    
    // 2. Run diagnostics
    check_signal_quality();
    scan_i2c_devices();
    
    // 3. Cleanup
    i2c_driver_delete(I2C_PORT);
    ESP_LOGI("I2C", "Diagnostics complete");
}
