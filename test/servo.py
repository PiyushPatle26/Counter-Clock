import time
import board
import busio
from adafruit_pca9685 import PCA9685

# Create I2C bus interface
i2c = busio.I2C(board.SCL, board.SDA)

# Create PCA9685 instance
pca = PCA9685(i2c)
pca.frequency = 50

# Define servo channels
SERVO_CHANNELS = [0, 1, 2, 3, 4, 5, 6]  # Modified to support 7 servos

# Function to set servo angle
def set_servo_angle(servo, angle):
    """
    Set the angle of a specific servo.
    :param servo: Servo index (0 to 6)
    :param angle: Desired angle (0 to 180)
    """
    if 0 <= servo < len(SERVO_CHANNELS) and 0 <= angle <= 180:
        pulse_width = int((angle / 180.0) * 4095)  # Convert angle to duty cycle
        pca.channels[SERVO_CHANNELS[servo]].duty_cycle = pulse_width
        print(f"Servo {servo} set to {angle} degrees.")
    else:
        print("Invalid servo index or angle.")

try:
    while True:
        cmd = input("Enter command (servo_index angle), or 'q' to quit: ")
        if cmd.lower() == 'q':
            break
        try:
            servo, angle = map(int, cmd.split())
            set_servo_angle(servo, angle)
        except ValueError:
            print("Invalid input. Use: servo_index angle (e.g., '0 90')")

except KeyboardInterrupt:
    print("\nExiting...")

finally:
    # Reset all servos
    for channel in SERVO_CHANNELS:
        pca.channels[channel].duty_cycle = 0
    pca.deinit()