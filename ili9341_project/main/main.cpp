#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// The main tag for logging
#define TAG "ILI9341_TOUCH"

// Display pins
#define PIN_MOSI       GPIO_NUM_23  // SDA
#define PIN_SCK        GPIO_NUM_18  // SCL
#define PIN_CS_LCD     GPIO_NUM_5   // CS for LCD
#define PIN_DC         GPIO_NUM_2   // DC (Data/Command)
#define PIN_RST        GPIO_NUM_4   // Reset
#define PIN_LED        GPIO_NUM_15  // Backlight

// Touch controller pins (XPT2046)
#define PIN_CS_TOUCH   GPIO_NUM_21  // CS for touch controller
#define PIN_IRQ_TOUCH  GPIO_NUM_22  // Touch interrupt pin

// ILI9341 Commands
#define ILI9341_SWRESET    0x01
#define ILI9341_SLPOUT     0x11
#define ILI9341_DISPON     0x29
#define ILI9341_CASET      0x2A
#define ILI9341_PASET      0x2B
#define ILI9341_RAMWR      0x2C
#define ILI9341_MADCTL     0x36
#define ILI9341_PIXFMT     0x3A
#define ILI9341_FRMCTR1    0xB1
#define ILI9341_DFUNCTR    0xB6
#define ILI9341_PWCTR1     0xC0
#define ILI9341_PWCTR2     0xC1
#define ILI9341_VMCTR1     0xC5
#define ILI9341_VMCTR2     0xC7
#define ILI9341_GMCTRP1    0xE0
#define ILI9341_GMCTRN1    0xE1

// ILI9341 MADCTL bits
#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

// Display dimensions
#define ILI9341_TFTWIDTH   240
#define ILI9341_TFTHEIGHT  320

// Colors (16-bit RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_DARKGRAY 0x4208

// Font definitions
#define FONT_WIDTH  6
#define FONT_HEIGHT 8

// XPT2046 Touch commands
#define XPT2046_CMD_X    0xD0
#define XPT2046_CMD_Y    0x90

// Touch calibration values (you may need to adjust these)
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3900

// Button structure
typedef struct {
    uint16_t x, y, w, h;
    uint16_t color;
    uint16_t text_color;
    const char* text;
    bool pressed;
} button_t;

// Touch point structure
typedef struct {
    uint16_t x, y;
    bool valid;
} touch_point_t;

static spi_device_handle_t spi_lcd;
static spi_device_handle_t spi_touch;

// Simple 6x8 bitmap font for basic ASCII characters (32-126)
static const uint8_t font6x8[][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00, 0x00}, // ! (33)
    {0x00, 0x07, 0x00, 0x07, 0x00, 0x00}, // " (34)
    {0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00}, // # (35)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00}, // $ (36)
    {0x23, 0x13, 0x08, 0x64, 0x62, 0x00}, // % (37)
    {0x36, 0x49, 0x55, 0x22, 0x50, 0x00}, // & (38)
    {0x00, 0x05, 0x03, 0x00, 0x00, 0x00}, // ' (39)
    {0x00, 0x1C, 0x22, 0x41, 0x00, 0x00}, // ( (40)
    {0x00, 0x41, 0x22, 0x1C, 0x00, 0x00}, // ) (41)
    {0x08, 0x2A, 0x1C, 0x2A, 0x08, 0x00}, // * (42)
    {0x08, 0x08, 0x3E, 0x08, 0x08, 0x00}, // + (43)
    {0x00, 0x50, 0x30, 0x00, 0x00, 0x00}, // , (44)
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x00}, // - (45)
    {0x00, 0x60, 0x60, 0x00, 0x00, 0x00}, // . (46)
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x00}, // / (47)
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00}, // 0 (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00}, // 1 (49)
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00}, // 2 (50)
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00}, // 3 (51)
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00}, // 4 (52)
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00}, // 5 (53)
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00}, // 6 (54)
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00}, // 7 (55)
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00}, // 8 (56)
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00}, // 9 (57)
    {0x00, 0x36, 0x36, 0x00, 0x00, 0x00}, // : (58)
    {0x00, 0x56, 0x36, 0x00, 0x00, 0x00}, // ; (59)
    {0x00, 0x08, 0x14, 0x22, 0x41, 0x00}, // < (60)
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00}, // = (61)
    {0x00, 0x41, 0x22, 0x14, 0x08, 0x00}, // > (62)
    {0x02, 0x01, 0x51, 0x09, 0x06, 0x00}, // ? (63)
    {0x32, 0x49, 0x59, 0x51, 0x3E, 0x00}, // @ (64)
    {0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00}, // A (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00}, // B (66)
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00}, // C (67)
    {0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00}, // D (68)
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}, // E (69)
    {0x7F, 0x09, 0x09, 0x01, 0x01, 0x00}, // F (70)
    {0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00}, // G (71)
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00}, // H (72)
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00}, // I (73)
    {0x20, 0x40, 0x41, 0x3F, 0x01, 0x00}, // J (74)
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00}, // K (75)
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}, // L (76)
    {0x7F, 0x02, 0x04, 0x02, 0x7F, 0x00}, // M (77)
    {0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00}, // N (78)
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00}, // O (79)
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00}, // P (80)
    {0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00}, // Q (81)
    {0x7F, 0x09, 0x19, 0x29, 0x46, 0x00}, // R (82)
    {0x46, 0x49, 0x49, 0x49, 0x31, 0x00}, // S (83)
    {0x01, 0x01, 0x7F, 0x01, 0x01, 0x00}, // T (84)
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00}, // U (85)
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00}, // V (86)
    {0x7F, 0x20, 0x18, 0x20, 0x7F, 0x00}, // W (87)
    {0x63, 0x14, 0x08, 0x14, 0x63, 0x00}, // X (88)
    {0x03, 0x04, 0x78, 0x04, 0x03, 0x00}, // Y (89)
    {0x61, 0x51, 0x49, 0x45, 0x43, 0x00}, // Z (90)
    {0x00, 0x00, 0x7F, 0x41, 0x41, 0x00}, // [ (91)
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x00}, // \ (92)
    {0x00, 0x41, 0x41, 0x7F, 0x00, 0x00}, // ] (93)
    {0x04, 0x02, 0x01, 0x02, 0x04, 0x00}, // ^ (94)
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x00}, // _ (95)
    {0x00, 0x01, 0x02, 0x04, 0x00, 0x00}, // ` (96)
    {0x20, 0x54, 0x54, 0x54, 0x78, 0x00}, // a (97)
    {0x7F, 0x48, 0x44, 0x44, 0x38, 0x00}, // b (98)
    {0x38, 0x44, 0x44, 0x44, 0x20, 0x00}, // c (99)
    {0x38, 0x44, 0x44, 0x48, 0x7F, 0x00}, // d (100)
    {0x38, 0x54, 0x54, 0x54, 0x18, 0x00}, // e (101)
    {0x08, 0x7E, 0x09, 0x01, 0x02, 0x00}, // f (102)
    {0x08, 0x14, 0x54, 0x54, 0x3C, 0x00}, // g (103)
    {0x7F, 0x08, 0x04, 0x04, 0x78, 0x00}, // h (104)
    {0x00, 0x44, 0x7D, 0x40, 0x00, 0x00}, // i (105)
    {0x20, 0x40, 0x44, 0x3D, 0x00, 0x00}, // j (106)
    {0x00, 0x7F, 0x10, 0x28, 0x44, 0x00}, // k (107)
    {0x00, 0x41, 0x7F, 0x40, 0x00, 0x00}, // l (108)
    {0x7C, 0x04, 0x18, 0x04, 0x78, 0x00}, // m (109)
    {0x7C, 0x08, 0x04, 0x04, 0x78, 0x00}, // n (110)
    {0x38, 0x44, 0x44, 0x44, 0x38, 0x00}, // o (111)
    {0x7C, 0x14, 0x14, 0x14, 0x08, 0x00}, // p (112)
    {0x08, 0x14, 0x14, 0x18, 0x7C, 0x00}, // q (113)
    {0x7C, 0x08, 0x04, 0x04, 0x08, 0x00}, // r (114)
    {0x48, 0x54, 0x54, 0x54, 0x20, 0x00}, // s (115)
    {0x04, 0x3F, 0x44, 0x40, 0x20, 0x00}, // t (116)
    {0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00}, // u (117)
    {0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00}, // v (118)
    {0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00}, // w (119)
    {0x44, 0x28, 0x10, 0x28, 0x44, 0x00}, // x (120)
    {0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00}, // y (121)
    {0x44, 0x64, 0x54, 0x4C, 0x44, 0x00}, // z (122)
    {0x00, 0x08, 0x36, 0x41, 0x00, 0x00}, // { (123)
    {0x00, 0x00, 0x7F, 0x00, 0x00, 0x00}, // | (124)
    {0x00, 0x41, 0x36, 0x08, 0x00, 0x00}, // } (125)
    {0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00}  // ~ (126)
};

// Initialize GPIO pins
static void gpio_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_LED);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Configure touch IRQ pin as input with pull-up
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_IRQ_TOUCH);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    
    // Turn on backlight
    gpio_set_level(PIN_LED, 1);
}

// Send command to display
static void ili9341_send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0); // Command mode
    
    spi_transaction_t trans = {};
    trans.length = 8;
    trans.tx_data[0] = cmd;
    trans.flags = SPI_TRANS_USE_TXDATA;
    
    spi_device_polling_transmit(spi_lcd, &trans);
}

// Send data to display
static void ili9341_send_data(uint8_t data)
{
    gpio_set_level(PIN_DC, 1); // Data mode
    
    spi_transaction_t trans = {};
    trans.length = 8;
    trans.tx_data[0] = data;
    trans.flags = SPI_TRANS_USE_TXDATA;
    
    spi_device_polling_transmit(spi_lcd, &trans);
}

// Send multiple data bytes
static void ili9341_send_data_multi(uint8_t *data, size_t len)
{
    gpio_set_level(PIN_DC, 1); // Data mode
    
    spi_transaction_t trans = {};
    trans.length = len * 8;
    trans.tx_buffer = data;
    
    spi_device_polling_transmit(spi_lcd, &trans);
}

// Hardware reset
static void ili9341_reset(void)
{
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Set address window for drawing
static void ili9341_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ili9341_send_cmd(ILI9341_CASET); // Column address set
    uint8_t col_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    ili9341_send_data_multi(col_data, 4);
    
    ili9341_send_cmd(ILI9341_PASET); // Page address set
    uint8_t page_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};
    ili9341_send_data_multi(page_data, 4);
    
    ili9341_send_cmd(ILI9341_RAMWR); // Memory write
}

// Initialize the display
static void ili9341_init(void)
{
    ESP_LOGI(TAG, "Initializing ILI9341 display");
    
    ili9341_reset();
    
    ili9341_send_cmd(ILI9341_SWRESET); vTaskDelay(pdMS_TO_TICKS(150));
    ili9341_send_cmd(ILI9341_SLPOUT); vTaskDelay(pdMS_TO_TICKS(120));
    ili9341_send_cmd(0xCB); ili9341_send_data(0x39); ili9341_send_data(0x2C); ili9341_send_data(0x00); ili9341_send_data(0x34); ili9341_send_data(0x02);
    ili9341_send_cmd(0xCF); ili9341_send_data(0x00); ili9341_send_data(0xC1); ili9341_send_data(0x30);
    ili9341_send_cmd(0xE8); ili9341_send_data(0x85); ili9341_send_data(0x00); ili9341_send_data(0x78);
    ili9341_send_cmd(0xEA); ili9341_send_data(0x00); ili9341_send_data(0x00);
    ili9341_send_cmd(0xED); ili9341_send_data(0x64); ili9341_send_data(0x03); ili9341_send_data(0x12); ili9341_send_data(0x81);
    ili9341_send_cmd(0xF7); ili9341_send_data(0x20);
    ili9341_send_cmd(ILI9341_PWCTR1); ili9341_send_data(0x23);
    ili9341_send_cmd(ILI9341_PWCTR2); ili9341_send_data(0x10);
    ili9341_send_cmd(ILI9341_VMCTR1); ili9341_send_data(0x3e); ili9341_send_data(0x28);
    ili9341_send_cmd(ILI9341_VMCTR2); ili9341_send_data(0x86);
    ili9341_send_cmd(ILI9341_MADCTL); ili9341_send_data(ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR);
    ili9341_send_cmd(ILI9341_PIXFMT); ili9341_send_data(0x55);
    ili9341_send_cmd(ILI9341_FRMCTR1); ili9341_send_data(0x00); ili9341_send_data(0x18);
    ili9341_send_cmd(ILI9341_DFUNCTR); ili9341_send_data(0x08); ili9341_send_data(0x82); ili9341_send_data(0x27);
    ili9341_send_cmd(0xF2); ili9341_send_data(0x00);
    ili9341_send_cmd(0x26); ili9341_send_data(0x01);
    ili9341_send_cmd(ILI9341_GMCTRP1);
    uint8_t gamma_p[15] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
    ili9341_send_data_multi(gamma_p, 15);
    ili9341_send_cmd(ILI9341_GMCTRN1);
    uint8_t gamma_n[15] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
    ili9341_send_data_multi(gamma_n, 15);
    ili9341_send_cmd(ILI9341_DISPON); vTaskDelay(pdMS_TO_TICKS(120));
    
    ESP_LOGI(TAG, "ILI9341 initialization complete");
}

// Fill screen with solid color
static void ili9341_fill_screen(uint16_t color)
{
    ili9341_set_addr_window(0, 0, ILI9341_TFTWIDTH - 1, ILI9341_TFTHEIGHT - 1);
    
    gpio_set_level(PIN_DC, 1); // Data mode
    
    const size_t buffer_size_bytes = 1024;
    uint16_t *buffer = (uint16_t*)malloc(buffer_size_bytes);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate color buffer");
        return;
    }
    
    uint16_t swapped_color = (color >> 8) | (color << 8); // Swap bytes for SPI
    for (int i = 0; i < buffer_size_bytes / 2; i++) {
        buffer[i] = swapped_color;
    }
    
    uint32_t total_pixels = ILI9341_TFTWIDTH * ILI9341_TFTHEIGHT;
    uint32_t pixels_in_buffer = buffer_size_bytes / 2;
    uint32_t remaining_pixels = total_pixels;
    
    while (remaining_pixels > 0) {
        uint32_t pixels_to_send = (remaining_pixels > pixels_in_buffer) ? pixels_in_buffer : remaining_pixels;
        
        spi_transaction_t trans = {};
        trans.length = pixels_to_send * 16; // 16 bits per pixel
        trans.tx_buffer = buffer;
        
        spi_device_polling_transmit(spi_lcd, &trans);
        remaining_pixels -= pixels_to_send;
    }
    
    free(buffer);
}

// Draw a simple rectangle
static void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= ILI9341_TFTWIDTH || y >= ILI9341_TFTHEIGHT) return;
    if (x + w > ILI9341_TFTWIDTH) w = ILI9341_TFTWIDTH - x;
    if (y + h > ILI9341_TFTHEIGHT) h = ILI9341_TFTHEIGHT - y;
    
    ili9341_set_addr_window(x, y, x + w - 1, y + h - 1);
    gpio_set_level(PIN_DC, 1); // Data mode
    
    uint16_t swapped_color = (color >> 8) | (color << 8);
    for (uint32_t i = 0; i < w * h; i++) {
        ili9341_send_data(swapped_color >> 8);
        ili9341_send_data(swapped_color & 0xFF);
    }
}

// Draw a single character
static void ili9341_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color)
{
    if (x >= ILI9341_TFTWIDTH || y >= ILI9341_TFTHEIGHT) return;
    if (c < 32 || c > 126) c = ' '; // Replace unsupported chars with space
    
    const uint8_t *font_data = font6x8[c - 32];
    
    ili9341_set_addr_window(x, y, x + FONT_WIDTH - 1, y + FONT_HEIGHT - 1);
    gpio_set_level(PIN_DC, 1); // Data mode
    
    for (int row = 0; row < FONT_HEIGHT; row++) {
        for (int col = 0; col < FONT_WIDTH; col++) {
            uint16_t pixel_color;
            if (font_data[col] & (1 << row)) {
                pixel_color = color;
            } else {
                pixel_color = bg_color;
            }
            
            // Send pixel color (byte swapped for SPI)
            uint16_t swapped_color = (pixel_color >> 8) | (pixel_color << 8);
            ili9341_send_data(swapped_color >> 8);
            ili9341_send_data(swapped_color & 0xFF);
        }
    }
}

// Draw a string of text
static void ili9341_draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t bg_color)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    
    while (*text) {
        if (*text == '\n') {
            cursor_x = x;
            cursor_y += FONT_HEIGHT + 2; // Add 2 pixels spacing between lines
            if (cursor_y >= ILI9341_TFTHEIGHT) break;
        } else {
            if (cursor_x + FONT_WIDTH > ILI9341_TFTWIDTH) {
                cursor_x = x;
                cursor_y += FONT_HEIGHT + 2;
                if (cursor_y >= ILI9341_TFTHEIGHT) break;
            }
            
            ili9341_draw_char(cursor_x, cursor_y, *text, color, bg_color);
            cursor_x += FONT_WIDTH + 1; // Add 1 pixel spacing between characters
        }
        text++;
    }
}

// Draw text centered in a rectangle
static void ili9341_draw_text_centered(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *text, uint16_t color, uint16_t bg_color)
{
    int text_len = strlen(text);
    int text_width = text_len * (FONT_WIDTH + 1) - 1; // -1 for last character spacing
    int text_height = FONT_HEIGHT;
    
    int start_x = x + (w - text_width) / 2;
    int start_y = y + (h - text_height) / 2;
    
    ili9341_draw_text(start_x, start_y, text, color, bg_color);
}

// Read touch coordinates from XPT2046
static uint16_t xpt2046_read_data(uint8_t cmd)
{
    spi_transaction_t trans = {};
    uint8_t tx_data[3] = {cmd, 0x00, 0x00};
    uint8_t rx_data[3];
    
    trans.length = 24; // 3 bytes
    trans.tx_buffer = tx_data;
    trans.rx_buffer = rx_data;
    
    spi_device_polling_transmit(spi_touch, &trans);
    
    // XPT2046 returns 12-bit data in bits 14-3 of the response
    uint16_t result = ((rx_data[1] << 8) | rx_data[2]) >> 3;
    return result & 0x0FFF;
}

// Get touch point coordinates
static touch_point_t get_touch_point(void)
{
    touch_point_t point = {0, 0, false};
    
    // Check if touch is detected (IRQ pin goes low)
    if (gpio_get_level(PIN_IRQ_TOUCH) == 0) {
        // Read multiple samples and average them for better accuracy
        const int samples = 8;
        uint32_t x_sum = 0, y_sum = 0;
        int valid_samples = 0;
        
        for (int i = 0; i < samples; i++) {
            uint16_t x_raw = xpt2046_read_data(XPT2046_CMD_X);
            uint16_t y_raw = xpt2046_read_data(XPT2046_CMD_Y);
            
            // Filter out obviously invalid readings
            if (x_raw > TOUCH_X_MIN && x_raw < TOUCH_X_MAX && 
                y_raw > TOUCH_Y_MIN && y_raw < TOUCH_Y_MAX) {
                x_sum += x_raw;
                y_sum += y_raw;
                valid_samples++;
            }
            
            vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between samples
        }
        
        if (valid_samples > samples/2) { // At least half the samples should be valid
            uint16_t x_avg = x_sum / valid_samples;
            uint16_t y_avg = y_sum / valid_samples;
            
            // Map touch coordinates to screen coordinates
            point.x = (x_avg - TOUCH_X_MIN) * ILI9341_TFTWIDTH / (TOUCH_X_MAX - TOUCH_X_MIN);
            point.y = (y_avg - TOUCH_Y_MIN) * ILI9341_TFTHEIGHT / (TOUCH_Y_MAX - TOUCH_Y_MIN);
            
            // Clamp to screen boundaries
            if (point.x >= ILI9341_TFTWIDTH) point.x = ILI9341_TFTWIDTH - 1;
            if (point.y >= ILI9341_TFTHEIGHT) point.y = ILI9341_TFTHEIGHT - 1;
            
            point.valid = true;
        }
    }
    
    return point;
}

// Check if a point is inside a button
static bool is_point_in_button(touch_point_t point, button_t *button)
{
    if (!point.valid) return false;
    
    return (point.x >= button->x && point.x < button->x + button->w &&
            point.y >= button->y && point.y < button->y + button->h);
}

// Draw a button
static void draw_button(button_t *button)
{
    uint16_t border_color = button->pressed ? COLOR_WHITE : COLOR_DARKGRAY;
    uint16_t fill_color = button->pressed ? COLOR_DARKGRAY : button->color;
    
    // Draw button background
    ili9341_draw_rect(button->x, button->y, button->w, button->h, fill_color);
    
    // Draw button border
    ili9341_draw_rect(button->x, button->y, button->w, 2, border_color); // Top
    ili9341_draw_rect(button->x, button->y + button->h - 2, button->w, 2, border_color); // Bottom
    ili9341_draw_rect(button->x, button->y, 2, button->h, border_color); // Left
    ili9341_draw_rect(button->x + button->w - 2, button->y, 2, button->h, border_color); // Right
    
    // Draw button text
    ili9341_draw_text_centered(button->x, button->y, button->w, button->h, 
                              button->text, button->text_color, fill_color);
}

// Display question screen
static void display_question_screen(void)
{
    ili9341_fill_screen(COLOR_BLACK);
    
    // Title
    ili9341_draw_text_centered(0, 30, ILI9341_TFTWIDTH, 20, 
                              "Welcome!", COLOR_WHITE, COLOR_BLACK);
    
    // Question
    ili9341_draw_text_centered(0, 80, ILI9341_TFTWIDTH, 40, 
                              "Do you want to\nlearn robotics?", COLOR_CYAN, COLOR_BLACK);
    
    // Instructions
    ili9341_draw_text_centered(0, 200, ILI9341_TFTWIDTH, 20, 
                              "Touch your answer below:", COLOR_YELLOW, COLOR_BLACK);
}

// Display welcome screen
static void display_welcome_screen(void)
{
    ili9341_fill_screen(COLOR_GREEN);
    
    ili9341_draw_text_centered(0, 80, ILI9341_TFTWIDTH, 40, 
                              "Welcome to SRA!", COLOR_WHITE, COLOR_GREEN);
    
    ili9341_draw_text_centered(0, 140, ILI9341_TFTWIDTH, 40, 
                              "Let's start your\nrobotics journey!", COLOR_BLACK, COLOR_GREEN);
    
    ili9341_draw_text_centered(0, 220, ILI9341_TFTWIDTH, 20, 
                              "Touch screen to continue", COLOR_WHITE, COLOR_GREEN);
}

// Display alternative screen
static void display_alternative_screen(void)
{
    ili9341_fill_screen(COLOR_BLUE);
    
    ili9341_draw_text_centered(0, 80, ILI9341_TFTWIDTH, 40, 
                              "No problem!", COLOR_WHITE, COLOR_BLUE);
    
    ili9341_draw_text_centered(0, 140, ILI9341_TFTWIDTH, 40, 
                              "Do what you like\nand enjoy!", COLOR_YELLOW, COLOR_BLUE);
    
    ili9341_draw_text_centered(0, 220, ILI9341_TFTWIDTH, 20, 
                              "Touch screen to restart", COLOR_WHITE, COLOR_BLUE);
}

// Main application logic
static void touch_app_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting touch application");
    
    // Define buttons
    button_t yes_button = {
        .x = 20, .y = 240, .w = 80, .h = 50,
        .color = COLOR_GREEN, .text_color = COLOR_WHITE,
        .text = "YES", .pressed = false
    };
    
    button_t no_button = {
        .x = 140, .y = 240, .w = 80, .h = 50,
        .color = COLOR_RED, .text_color = COLOR_WHITE,
        .text = "NO", .pressed = false
    };
    
    enum app_state {
        STATE_QUESTION,
        STATE_WELCOME,
        STATE_ALTERNATIVE
    };
    
    enum app_state current_state = STATE_QUESTION;
    bool screen_needs_update = true;
    uint32_t last_touch_time = 0;
    bool was_touched = false;
    
    while (1) {
        touch_point_t touch = get_touch_point();
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Handle state transitions and screen updates
        switch (current_state) {
            case STATE_QUESTION:
                if (screen_needs_update) {
                    display_question_screen();
                    draw_button(&yes_button);
                    draw_button(&no_button);
                    screen_needs_update = false;
                }
                
                // Handle button presses
                if (touch.valid && (current_time - last_touch_time > 200)) { // Debounce
                    if (is_point_in_button(touch, &yes_button)) {
                        ESP_LOGI(TAG, "YES button pressed");
                        yes_button.pressed = true;
                        draw_button(&yes_button);
                        vTaskDelay(pdMS_TO_TICKS(200)); // Visual feedback
                        yes_button.pressed = false;
                        current_state = STATE_WELCOME;
                        screen_needs_update = true;
                        last_touch_time = current_time;
                    } else if (is_point_in_button(touch, &no_button)) {
                        ESP_LOGI(TAG, "NO button pressed");
                        no_button.pressed = true;
                        draw_button(&no_button);
                        vTaskDelay(pdMS_TO_TICKS(200)); // Visual feedback
                        no_button.pressed = false;
                        current_state = STATE_ALTERNATIVE;
                        screen_needs_update = true;
                        last_touch_time = current_time;
                    }
                }
                break;
                
            case STATE_WELCOME:
                if (screen_needs_update) {
                    display_welcome_screen();
                    screen_needs_update = false;
                }
                
                // Touch to go back to question
                if (touch.valid && !was_touched && (current_time - last_touch_time > 1000)) {
                    current_state = STATE_QUESTION;
                    screen_needs_update = true;
                    last_touch_time = current_time;
                }
                break;
                
            case STATE_ALTERNATIVE:
                if (screen_needs_update) {
                    display_alternative_screen();
                    screen_needs_update = false;
                }
                
                // Touch to go back to question
                if (touch.valid && !was_touched && (current_time - last_touch_time > 1000)) {
                    current_state = STATE_QUESTION;
                    screen_needs_update = true;
                    last_touch_time = current_time;
                }
                break;
        }
        
        was_touched = touch.valid;
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting ILI9341 TFT with touch driver");
    
    gpio_init();
    
    // Initialize SPI bus
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = PIN_MOSI;
    bus_config.miso_io_num = GPIO_NUM_19; // MISO for touch controller
    bus_config.sclk_io_num = PIN_SCK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 4096;
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }
    
    // Initialize LCD SPI device
    spi_device_interface_config_t lcd_config = {};
    lcd_config.clock_speed_hz = 26 * 1000 * 1000; // 26 MHz
    lcd_config.mode = 0;
    lcd_config.spics_io_num = PIN_CS_LCD;
    lcd_config.queue_size = 7;
    lcd_config.flags = SPI_DEVICE_HALFDUPLEX;
    
    ret = spi_bus_add_device(SPI2_HOST, &lcd_config, &spi_lcd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add LCD SPI device");
        return;
    }
    
    // Initialize Touch SPI device
    spi_device_interface_config_t touch_config = {};
    touch_config.clock_speed_hz = 2 * 1000 * 1000; // 2 MHz for touch
    touch_config.mode = 0;
    touch_config.spics_io_num = PIN_CS_TOUCH;
    touch_config.queue_size = 1;
    
    ret = spi_bus_add_device(SPI2_HOST, &touch_config, &spi_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add Touch SPI device");
        return;
    }
    
    ili9341_init();
    
    ESP_LOGI(TAG, "Display and touch initialized successfully");
    
    // Create touch application task
    xTaskCreate(touch_app_task, "touch_app", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Touch application started");
}