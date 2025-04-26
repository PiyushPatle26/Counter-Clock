#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

#define PCA9685_DEFAULT_ADDRESS 0x40

/**
 * @brief Enumeration of PCA9685 PWM output channels.
 *
 * Defines the 16 available PWM channels on the PCA9685, numbered from 0 to 15.
 */
typedef enum {
    PCA9685_CHANNEL_0 = 0,  /**< PWM Channel 0 */
    PCA9685_CHANNEL_1,      /**< PWM Channel 1 */
    PCA9685_CHANNEL_2,      /**< PWM Channel 2 */
    PCA9685_CHANNEL_3,      /**< PWM Channel 3 */
    PCA9685_CHANNEL_4,      /**< PWM Channel 4 */
    PCA9685_CHANNEL_5,      /**< PWM Channel 5 */
    PCA9685_CHANNEL_6,      /**< PWM Channel 6 */
    PCA9685_CHANNEL_7,      /**< PWM Channel 7 */
    PCA9685_CHANNEL_8,      /**< PWM Channel 8 */
    PCA9685_CHANNEL_9,      /**< PWM Channel 9 */
    PCA9685_CHANNEL_10,     /**< PWM Channel 10 */
    PCA9685_CHANNEL_11,     /**< PWM Channel 11 */
    PCA9685_CHANNEL_12,     /**< PWM Channel 12 */
    PCA9685_CHANNEL_13,     /**< PWM Channel 13 */
    PCA9685_CHANNEL_14,     /**< PWM Channel 14 */
    PCA9685_CHANNEL_15,     /**< PWM Channel 15 */
    PCA9685_CHANNEL_COUNT   /**< Total number of channels (16) */
} pca9685_channel_t;

/**
 * @brief PCA9685 device configuration structure.
 *
 * Holds the configuration parameters for a PCA9685 device, including the I2C port and address.
 */
typedef struct {
    i2c_port_t i2c_port;    /**< I2C port number (e.g., I2C_NUM_0 or I2C_NUM_1) */
    uint8_t i2c_addr;       /**< I2C address of the PCA9685 (e.g., 0x40 to 0x7F) */
    float pwm_freq_hz;      /**< Current PWM frequency in Hz, set by pca9685_set_frequency */
} pca9685_dev_t;

/**
 * @brief Initialize a PCA9685 device.
 *
 * Configures the PCA9685 for operation on the specified I2C port and address.
 * Assumes the I2C driver is already initialized externally (e.g., by the application).
 * Puts the device into sleep mode briefly, enables auto-increment, and restarts the oscillator.
 * All PWM channels are set to 0% duty cycle on initialization.
 *
 * @param dev Pointer to the PCA9685 device configuration structure.
 * @param port I2C port number (e.g., I2C_NUM_0 or I2C_NUM_1).
 * @param addr I2C address of the PCA9685 (typically 0x40 to 0x7F).
 * @return
 *     - ESP_OK on success.
 *     - ESP_ERR_INVALID_ARG if dev is NULL.
 *     - ESP_FAIL or other errors if I2C communication fails.
 */
esp_err_t pca9685_init(pca9685_dev_t *dev, i2c_port_t port, uint8_t addr);

/**
 * @brief Set the PWM frequency for all channels.
 *
 * Configures the PCA9685's oscillator to produce the specified PWM frequency.
 * Valid frequencies range from 24 Hz to 1526 Hz. The device is briefly put into
 * sleep mode to update the prescaler, then restarted.
 *
 * @param dev Pointer to the initialized PCA9685 device configuration structure.
 * @param freq_hz Desired PWM frequency in Hz (24 to 1526).
 * @return
 *     - ESP_OK on success.
 *     - ESP_ERR_INVALID_ARG if dev is NULL or freq_hz is out of range.
 *     - ESP_FAIL or other errors if I2C communication fails.
 */
esp_err_t pca9685_set_frequency(pca9685_dev_t *dev, float freq_hz);

/**
 * @brief Set the PWM duty cycle for a specific channel.
 *
 * Sets the duty cycle for the specified PWM channel, from 0 to 4095 (12-bit resolution),
 * corresponding to 0% to 100% duty cycle. The PWM signal uses the frequency set by
 * pca9685_set_frequency.
 *
 * @param dev Pointer to the initialized PCA9685 device configuration structure.
 * @param channel PWM channel to configure (PCA9685_CHANNEL_0 to PCA9685_CHANNEL_15).
 * @param duty Duty cycle value (0 to 4095).
 * @return
 *     - ESP_OK on success.
 *     - ESP_ERR_INVALID_ARG if dev is NULL, channel is invalid, or duty > 4095.
 *     - ESP_FAIL or other errors if I2C communication fails.
 */
esp_err_t pca9685_set_duty(pca9685_dev_t *dev, pca9685_channel_t channel, uint16_t duty);

/**
 * @brief Set the PWM pulse width for a servo on a specific channel.
 *
 * Configures the pulse width for a servo connected to the specified channel, in microseconds.
 * Typical servo pulse widths range from 500 µs to 2500 µs. The pulse width is converted
 * to a duty cycle based on the current PWM frequency set by pca9685_set_frequency.
 *
 * @param dev Pointer to the initialized PCA9685 device configuration structure.
 * @param channel PWM channel connected to the servo (PCA9685_CHANNEL_0 to PCA9685_CHANNEL_15).
 * @param pulse_us Pulse width in microseconds (500 to 2500).
 * @return
 *     - ESP_OK on success.
 *     - ESP_ERR_INVALID_ARG if dev is NULL, channel is invalid, or pulse_us is out of range.
 *     - ESP_ERR_INVALID_STATE if PWM frequency is not set.
 *     - ESP_FAIL or other errors if I2C communication fails.
 */
esp_err_t pca9685_set_servo_pulse(pca9685_dev_t *dev, pca9685_channel_t channel, uint16_t pulse_us);

#endif // PCA9685_H