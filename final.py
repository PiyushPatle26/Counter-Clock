import time
from test.ds1307 import DS1307
from test.lcd import lcd_init, lcd_clear, lcd_set_cursor, lcd_print, get_greeting
from test.servo import set_servo_angle

# Seven segment display configurations for each digit (0-9)
# Each tuple represents the required servo angles (0 or 180 degrees) for segments a,b,c,d,e,f,g
DIGIT_PATTERNS = {
    0: (180, 180, 180, 180, 180, 180, 0),    # 0
    1: (0, 180, 180, 0, 0, 0, 0),            # 1
    2: (180, 180, 0, 180, 180, 0, 180),      # 2
    3: (180, 180, 180, 180, 0, 0, 180),      # 3
    4: (0, 180, 180, 0, 0, 180, 180),        # 4
    5: (180, 0, 180, 180, 0, 180, 180),      # 5
    6: (180, 0, 180, 180, 180, 180, 180),    # 6
    7: (180, 180, 180, 0, 0, 0, 0),          # 7
    8: (180, 180, 180, 180, 180, 180, 180),  # 8
    9: (180, 180, 180, 180, 0, 180, 180)     # 9
}

class SevenSegmentDisplay:
    def __init__(self, start_servo):
        """Initialize a 7-segment display with 7 servos starting from start_servo"""
        self.start_servo = start_servo

    def display_digit(self, digit):
        """Display a digit (0-9) using servo positions"""
        if digit in DIGIT_PATTERNS:
            pattern = DIGIT_PATTERNS[digit]
            for i, angle in enumerate(pattern):
                set_servo_angle(self.start_servo + i, angle)

def setup():
    """Initialize all components"""
    # Initialize LCD
    lcd_init()
    lcd_clear()

    # Initialize RTC
    rtc = DS1307()

    # Create 4 seven-segment displays (28 servos total, 7 for each digit)
    displays = [
        SevenSegmentDisplay(0),   # Hours tens
        SevenSegmentDisplay(7),   # Hours ones
        SevenSegmentDisplay(14),  # Minutes tens
        SevenSegmentDisplay(21)   # Minutes ones
    ]

    return rtc, displays

def update_time_display(displays, hours, minutes):
    """Update all displays with current time"""
    # Split hours and minutes into digits
    hours_tens = hours // 10
    hours_ones = hours % 10
    minutes_tens = minutes // 10
    minutes_ones = minutes % 10

    # Update each display
    displays[0].display_digit(hours_tens)
    displays[1].display_digit(hours_ones)
    displays[2].display_digit(minutes_tens)
    displays[3].display_digit(minutes_ones)

def main():
    rtc, displays = setup()
    last_hours = -1  # Force initial update

    try:
        while True:
            # Get current time
            current_time = rtc.read_time()
            hours = current_time.hour
            minutes = current_time.minute

            # Update displays
            update_time_display(displays, hours, minutes)

            # Update LCD greeting if hour has changed
            if hours != last_hours:
                lcd_clear()
                greeting = get_greeting(hours)
                lcd_set_cursor(0, 0)
                lcd_print(greeting)
                last_hours = hours

            # Wait before next update
            time.sleep(1)

    except KeyboardInterrupt:
        print("\nProgram terminated")
        lcd_clear()

if __name__ == "__main__":
    main()
