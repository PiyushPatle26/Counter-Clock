#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define GPS_UART UART_NUM_2
#define GPS_TXD (GPIO_NUM_17)
#define GPS_RXD (GPIO_NUM_16)
#define BUF_SIZE (1024)

static const char *TAG = "gps_test";

// GPS NMEA sentence buffer
static char nmea_buffer[BUF_SIZE];
static int buffer_index = 0;

void parse_gps_data(const char* sentence) {
    ESP_LOGI(TAG, "Parsing NMEA sentence: %s", sentence);  // Debugging NMEA sentence
    if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        // GPRMC: Recommended Minimum Specific GPS/Transit Data
        char temp[256];
        strncpy(temp, sentence, sizeof(temp) - 1);
        temp[sizeof(temp)-1] = 0;
        char *token;
        char *saveptr;
        int field_index = 0;
        float lat = 0, lon = 0, speed = 0;
        char lat_dir = 'N', lon_dir = 'E';
        int valid = 0;

        token = strtok_r(temp, ",", &saveptr);
        while (token != NULL) {
            switch(field_index) {
                case 2: // Status A=active, V=void
                    if (token[0] == 'A') valid = 1;
                    break;
                case 3: // Latitude
                    if (strlen(token) > 0) lat = atof(token);
                    break;
                case 4: // N/S
                    if (strlen(token) > 0) lat_dir = token[0];
                    break;
                case 5: // Longitude
                    if (strlen(token) > 0) lon = atof(token);
                    break;
                case 6: // E/W
                    if (strlen(token) > 0) lon_dir = token[0];
                    break;
                case 7: // Speed in knots
                    if (strlen(token) > 0) speed = atof(token) * 1.852; // convert to km/h
                    break;
            }
            token = strtok_r(NULL, ",", &saveptr);
            field_index++;
        }

        if (valid) {
            float lat_deg = (int)(lat / 100);
            float lat_min = lat - (lat_deg * 100);
            float latitude = lat_deg + (lat_min / 60.0);
            if (lat_dir == 'S') latitude = -latitude;
            float lon_deg = (int)(lon / 100);
            float lon_min = lon - (lon_deg * 100);
            float longitude = lon_deg + (lon_min / 60.0);
            if (lon_dir == 'W') longitude = -longitude;

            ESP_LOGI(TAG, "Latitude: %.6f", latitude);
            ESP_LOGI(TAG, "Longitude: %.6f", longitude);
            ESP_LOGI(TAG, "Speed: %.2f km/h", speed);
        } else {
            ESP_LOGI(TAG, "No valid GPRMC data");
        }
    } else if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        // GGA: Global Positioning System Fix Data
        char temp[256];
        strncpy(temp, sentence, sizeof(temp) - 1);
        temp[sizeof(temp)-1] = 0;
        char *token;
        char *saveptr;
        int field_index = 0;
        int satellites = 0;
        float altitude = 0;
        int valid = 0;

        token = strtok_r(temp, ",", &saveptr);
        while (token != NULL) {
            switch(field_index) {
                case 6: // Fix quality: 0 = invalid, 1 = GPS fix, 2 = DGPS fix
                    if (strlen(token) > 0 && token[0] > '0') valid = 1;
                    break;
                case 7: // Number of satellites
                    if (strlen(token) > 0) satellites = atoi(token);
                    break;
                case 9: // Altitude (meters)
                    if (strlen(token) > 0) altitude = atof(token);
                    break;
            }
            token = strtok_r(NULL, ",", &saveptr);
            field_index++;
        }

        if (valid) {
            ESP_LOGI(TAG, "Satellites: %d", satellites);
            ESP_LOGI(TAG, "Altitude: %.2f m", altitude);
        } else {
            ESP_LOGI(TAG, "No valid GGA data");
        }
    }
}

// Optimized GPS task with better debugging and periodic summaries
void gps_task(void *arg) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_LOGI(TAG, "Configuring UART...");
    esp_err_t err = uart_driver_install(GPS_UART, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    err = uart_param_config(GPS_UART, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    err = uart_set_pin(GPS_UART, GPS_TXD, GPS_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate UART buffer");
        vTaskDelete(NULL);
    }

    TickType_t last_data_tick = xTaskGetTickCount();
    TickType_t last_summary_tick = xTaskGetTickCount();
    int nmea_counter = 0;

    // Last valid fix info
    float last_lat = 0, last_lon = 0, last_speed = 0, last_alt = 0;
    int last_sats = 0;
    int have_fix = 0;

    while (1) {
        int len = uart_read_bytes(GPS_UART, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Read %d bytes from UART", len);  // Debugging UART read length

        if (len > 0) {
            for(int i = 0; i < len; i++) {
                char current = data[i];

                if (current == '\n' || current == '\r') {
                    if (buffer_index > 0) {
                        nmea_buffer[buffer_index] = '\0';
                        nmea_counter++;

                        ESP_LOGI(TAG, "[%d] NMEA: %s", nmea_counter, nmea_buffer); // Debugging NMEA sentence

                        parse_gps_data(nmea_buffer);  // Parse and update last valid fix

                        buffer_index = 0;
                        last_data_tick = xTaskGetTickCount();
                    }
                } else if (buffer_index < BUF_SIZE - 1) {
                    nmea_buffer[buffer_index++] = current;
                } else {
                    ESP_LOGW(TAG, "Buffer overflow detected, skipping byte"); // Debugging buffer overflow
                }
            }
        } else {
            if ((xTaskGetTickCount() - last_data_tick) > pdMS_TO_TICKS(5000)) {
                ESP_LOGW(TAG, "No GPS data received from UART for 5 seconds");
                last_data_tick = xTaskGetTickCount();  // Reset the tick counter
            }
        }

        // Print summary every 10 seconds
        if ((xTaskGetTickCount() - last_summary_tick) > pdMS_TO_TICKS(10000)) {
            if (have_fix) {
                ESP_LOGI(TAG, "--- Last Valid Fix ---");
                ESP_LOGI(TAG, "Lat: %.6f, Lon: %.6f, Speed: %.2f km/h, Sats: %d, Alt: %.2f m", last_lat, last_lon, last_speed, last_sats, last_alt);
            } else {
                ESP_LOGI(TAG, "--- No valid GPS fix yet ---");
            }
            last_summary_tick = xTaskGetTickCount();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    free(data);
}

void app_main(void) {
    // Start the GPS task
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 10, NULL);
}
