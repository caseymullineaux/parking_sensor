# Garage Parking Sensor

An ESP32-based garage parking sensor that uses visual LED feedback to help drivers park perfectly every time. The system uses an ultrasonic distance sensor and a 60-LED WS2812B strip to provide real-time distance feedback as you pull into your garage.

## Features

- **Symmetrical LED Display**: LEDs light up from both ends of the strip toward the center, providing clear visual feedback from either side of the vehicle
- **Automatic Color-Coded Distance Feedback**:
  - 🟢 **Green** (Far 2/3 of range): Safe distance, keep moving forward
  - 🟡 **Yellow** (Close 1/3 of range): Getting closer, proceed carefully
  - 🔴 **All Red** (At stop distance): **STOP!** Perfect parking position reached
- **Auto-Shutoff**: LEDs automatically turn off 2.5 minutes after parking to save power
- **Progressive Lighting**: Individual LEDs (not groups) turn off smoothly from outside-in as the vehicle approaches, providing intuitive distance awareness
- **WiFi Configuration Portal**: Adjust stop and max distance without reprogramming - color zones automatically calculated
- **Persistent Settings**: Configuration saved to EEPROM and retained after power cycle
- **Smooth Measurements**: 5-sample moving average filter with real sensor priming eliminates jumpy readings
- **Fast Compilation**: Uses lightweight Adafruit NeoPixel library for quick development cycles

## Hardware Requirements

- **ESP32 Development Board** (ESP32-WROOM-DA or compatible)
- **HC-SR04 Ultrasonic Distance Sensor**
- **WS2812B LED Strip** (60 LEDs, 5V)
- **Power Supply**: 5V DC, 2-3A recommended (brightness limited to 50/255 for USB power compatibility)
- Jumper wires and mounting hardware

## Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| LED Strip Data | GPIO 17 |
| Ultrasonic TRIG | GPIO 5 |
| Ultrasonic ECHO | GPIO 12 |
| LED Strip 5V | 5V |
| LED Strip GND | GND |
| Ultrasonic VCC | 5V |
| Ultrasonic GND | GND |

## Software Setup

### Dependencies
- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)
- WiFi (built-in ESP32)
- WebServer (built-in ESP32)
- Preferences (built-in ESP32)

### Installation
1. Install the Arduino IDE or use VS Code with the Arduino extension
2. Install the ESP32 board support in Arduino IDE
3. Install the Adafruit NeoPixel library:
   ```bash
   arduino-cli lib install "Adafruit NeoPixel"
   ```
4. Open `parking_sensor.ino` and upload to your ESP32

### WiFi Configuration

The sensor creates its own WiFi access point for easy configuration:

1. **Connect to WiFi**: 
   - SSID: `ParkingSensor`
   - Password: `configure123`

2. **Open Web Interface**: 
   - Navigate to `http://192.168.4.1` in your browser

3. **Configure Settings**:
   - **Stop Distance** (default 130cm): Perfect parking position - all LEDs turn solid red
   - **Max Distance** (default 4000cm): Beyond this, LEDs turn off
   - **Color Zones**: Automatically calculated - Green for far 2/3 of range, Yellow for close 1/3

4. Settings are automatically saved to EEPROM and persist after reboot

### Default Configuration

```cpp
int stopDistance = 13;         // STOP position - all LEDs solid red (in cm)
int maxDistance = 400;         // Maximum detection range (in cm)
// Yellow threshold auto-calculated: stopDistance + (range × 2/3)
#define BRIGHTNESS 50          // LED brightness (0-255)
#define FILTER_SAMPLES 5       // Moving average filter samples
#define AUTO_SHUTOFF_MS 150000 // Auto-shutoff timer: 2.5 minutes (150 seconds)
```

## How It Works

1. **Distance Measurement**: The HC-SR04 sensor continuously measures distance in centimeters, with readings smoothed by a 5-sample moving average filter initialized with real sensor readings at startup
2. **LED Calculation**: Based on distance, the system calculates how many individual LEDs should be illuminated (not triplets)
3. **Symmetrical Display**: Individual LEDs light up from both ends of the strip, turning off progressively from outside-in as the vehicle approaches
4. **Automatic Color Zones**:
   - Green zone: Far 2/3 of the distance range (calculated as: stopDistance + range × 2/3 to maxDistance)
   - Yellow zone: Close 1/3 of the distance range (stopDistance to yellow threshold)
   - Red: All LEDs solid red when at or below stopDistance
5. **Auto-Shutoff**: When vehicle parks at stop distance, LEDs remain red for 2.5 minutes then automatically turn off to save power. Timer resets if vehicle moves away
6. **Web Configuration**: Real-time distance display and simple threshold adjustment via WiFi portal - only configure stop and max distance, color zones calculate automatically

## Installation in Garage

1. Mount the LED strip horizontally at driver's eye level (typically wall-mounted)
2. Mount the ultrasonic sensor at the parking target position, pointing toward the approaching vehicle
3. Position the sensor at bumper height for accurate distance measurement
4. Connect to 5V power supply (2-3A recommended for higher brightness; current brightness setting works with USB power)
5. Connect to `ParkingSensor` WiFi and adjust stop/max distances at `http://192.168.4.1` - color zones automatically adjust based on your settings

## Power Considerations

- LED brightness is set to 50/255 to work within USB power constraints
- For full brightness operation, use a dedicated 5V 3A power supply
- Current draw: approximately 60mA per LED at full brightness (60 LEDs × 60mA = 3.6A max)

## Troubleshooting

- **Jumpy measurements**: Increase `FILTER_SAMPLES` for more smoothing (trades responsiveness for stability)
- **LEDs don't light up**: Check power supply capacity and wiring connections
- **Can't connect to WiFi**: Ensure you're connecting to `ParkingSensor` network, not your home WiFi
- **Web interface not loading**: Try `http://192.168.4.1` - IP address is always the same

## Inspired By

[Build a parking sensor with ESP32 board, LEDs and ultrasonic distance sensor](https://www.poeticoding.com/build-a-parking-sensor-with-esp32-board-leds-and-ultrasonic-distance-sensor/)
