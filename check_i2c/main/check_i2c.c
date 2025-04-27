#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_PORT I2C_NUM_0
#define SDA_GPIO 21
#define SCL_GPIO 22

void app_main(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    esp_err_t err;

    err = i2c_param_config(I2C_PORT, &conf);
    ESP_LOGI("I2C", "i2c_param_config: %s", esp_err_to_name(err));
    if (err != ESP_OK) return;

    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    ESP_LOGI("I2C", "i2c_driver_install: %s", esp_err_to_name(err));
    if (err == ESP_OK) {
        ESP_LOGI("I2C", "I2C driver installed successfully!");
    } else {
        ESP_LOGE("I2C", "I2C driver install failed!");
    }
}
