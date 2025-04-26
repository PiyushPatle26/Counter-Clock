# NEO-6M GPS Test for ESP32

This is a simple ESP-IDF project that demonstrates how to read location data from a NEO-6M GPS module using UART.

## Hardware Requirements
- ESP32 development board
- NEO-6M GPS module with antenna
- Jumper wires

## Wiring Instructions
Connect the NEO-6M GPS module to your ESP32 as follows:

| NEO-6M Pin | ESP32 Pin |
|------------|-----------|
| VCC        | 3.3V     |
| GND        | GND      |
| TX         | GPIO16   |
| RX         | GPIO17   |

## Building and Running
1. Make sure you have ESP-IDF installed and properly set up
2. Navigate to the project directory
3. Build the project:
   ```
   idf.py build
   ```
4. Flash the project:
   ```
   idf.py -p (PORT) flash
   ```
5. Monitor the output:
   ```
   idf.py -p (PORT) monitor
   ```

## Expected Output
Once the GPS has a fix, you'll see output like this in the monitor:
```
I (1234) gps_test: Latitude: 12.345678
I (1234) gps_test: Longitude: 98.765432
```

## Notes
- The GPS module uses UART2 at 9600 baud rate
- First GPS fix may take several minutes, especially on cold start
- Make sure the GPS module has a clear view of the sky
- The antenna must be properly connected to the module

## Troubleshooting
- Verify all connections are secure
- Check that TX/RX pins are correctly connected
- Ensure the GPS module is powered properly
- Make sure the antenna is properly attached
- If no data is received, try moving to an area with better sky visibility
