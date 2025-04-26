from smbus2 import SMBus
import time
from datetime import datetime

# DS1307 I2C address
DS1307_ADDRESS = 0x68

# Register addresses
SECONDS_REG = 0x00
MINUTES_REG = 0x01
HOURS_REG = 0x02
DAY_REG = 0x03
DATE_REG = 0x04
MONTH_REG = 0x05
YEAR_REG = 0x06
CONTROL_REG = 0x07

class DS1307:
    def __init__(self, bus_number=1):
        self.bus = SMBus(bus_number)
        
    def _bcd_to_int(self, bcd):
        """Convert BCD format to integer"""
        return (bcd & 0x0F) + ((bcd >> 4) * 10)
    
    def _int_to_bcd(self, value):
        """Convert integer to BCD format"""
        return ((value // 10) << 4) + (value % 10)
    
    def read_time(self):
        """Read current time from DS1307"""
        data = self.bus.read_i2c_block_data(DS1307_ADDRESS, 0x00, 7)
        
        seconds = self._bcd_to_int(data[0] & 0x7F)
        minutes = self._bcd_to_int(data[1])
        hours = self._bcd_to_int(data[2] & 0x3F)  # 24 hour mode
        day = self._bcd_to_int(data[3])
        date = self._bcd_to_int(data[4])
        month = self._bcd_to_int(data[5])
        year = 2000 + self._bcd_to_int(data[6])
        
        return datetime(year, month, date, hours, minutes, seconds)
    
    def set_time(self, dt):
        """Set time on DS1307"""
        data = [
            self._int_to_bcd(dt.second),
            self._int_to_bcd(dt.minute),
            self._int_to_bcd(dt.hour),
            self._int_to_bcd(dt.isoweekday()),
            self._int_to_bcd(dt.day),
            self._int_to_bcd(dt.month),
            self._int_to_bcd(dt.year - 2000)
        ]
        self.bus.write_i2c_block_data(DS1307_ADDRESS, 0x00, data)

def continuous_time_display():
    """Continuously read and display time from DS1307"""
    rtc = DS1307()

    # Sync RTC time with Raspberry Pi system time once
    print("Setting RTC to system time...")
    rtc.set_time(datetime.now())
    
    try:
        while True:
            current_time = rtc.read_time()
            print(f"Current Time: {current_time.strftime('%Y-%m-%d %H:%M:%S')}")
            time.sleep(1)  # Update every second
            
    except KeyboardInterrupt:
        print("\nStopping time display")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    continuous_time_display()