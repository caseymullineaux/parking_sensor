# Garage Parking Sensor

An ESP32-based garage parking sensor that uses visual LED feedback to help drivers park perfectly every time. The system uses an ultrasonic distance sensor and a 60-LED WS2812B strip to provide real-time distance feedback as you pull into your garage.

## Features

- **Symmetrical LED Display**: LEDs light up from both ends of the strip toward the center, providing clear visual feedback from either side of the vehicle
- **Color-Coded Distance Feedback**:
  - 🟢 **Green** (>200cm): Safe distance, keep moving forward
  - ⚪ **White** (60-200cm): Moderate distance, slow down
  - 🔴 **Red** (20-60cm): Getting close, proceed carefully
  - 🔴 **All Red** (≤20cm): **STOP!** Perfect parking position reached
- **Progressive Lighting**: More LEDs illuminate as the vehicle gets closer, providing intuitive distance awareness
- **Fast Compilation**: Uses lightweight Adafruit NeoPixel library for quick development cycles

## Hardware Requirements

- **ESP32 Development Board** (ESP32-WROOM-DA or compatible)
- **HC-SR04 Ultrasonic Distance Sensor**
- **WS2812B LED Strip** (60 LEDs, 5V)
- **Power Supply**: 5V DC (adequate for LED strip - typically 3A recommended)
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

### Installation
1. Install the Arduino IDE or use VS Code with the Arduino extension
2. Install the ESP32 board support in Arduino IDE
3. Install the Adafruit NeoPixel library:
   ```bash
   arduino-cli lib install "Adafruit NeoPixel"
   ```
4. Open `parking_sensor.ino` and upload to your ESP32

### Configuration

You can adjust these parameters in the code to match your garage setup:

```cpp
#define DANGER_DISTANCE_CM 20    // Distance for "STOP" (all red)
#define MAX_DETECTION_CM 200     // Maximum detection range
#define DISTANCE_PER_LEVEL 20    // Distance per LED level
#define COLOR_CHANGE_LEVEL 3     // When color changes from red to white
```

## How It Works

1. The HC-SR04 sensor continuously measures the distance to the approaching vehicle
2. Based on the measured distance, the system calculates how many LED "triplets" (groups of 3 LEDs) to illuminate
3. LEDs light up symmetrically from both ends of the strip, converging toward the center
4. Color changes automatically based on proximity thresholds
5. When the vehicle reaches the target distance (20cm), all LEDs turn red to signal "STOP"

## Installation in Garage

1. Mount the LED strip horizontally at driver's eye level (typically wall-mounted)
2. Mount the ultrasonic sensor at the parking target position, pointing toward the approaching vehicle
3. Position the sensor at bumper height for accurate distance measurement
4. Connect power supply and ESP32
5. Adjust `DANGER_DISTANCE_CM` to match your ideal parking position

## Inspired By

[Build a parking sensor with ESP32 board, LEDs and ultrasonic distance sensor](https://www.poeticoding.com/build-a-parking-sensor-with-esp32-board-leds-and-ultrasonic-distance-sensor/)
