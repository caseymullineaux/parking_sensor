#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ——— WiFi CONFIGURATION ———
const char *AP_SSID = "ParkingSensor";
const char *AP_PASSWORD = "configure123";

// ——— LED CONFIGURATION ———
// This is tuned for 60 leds
#define LED_PIN 17         // GPIO pin for LED strip data line
#define NUM_LEDS 60        // Total number of LEDs on the strip
#define BRIGHTNESS 50      // LED brightness (0-255) - REDUCED for USB power
#define LEDS_PER_TRIPLET 3 // Number of LEDs grouped together
#define MAX_TRIPLETS 10    // Maximum number of triplets (30 LEDs per side)

// ——— DISTANCE SENSOR CONFIGURATION ———
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 12
#define SOUND_SPEED_CM_US 0.0343 // Speed of sound in cm/microsecond

// ——— PROXIMITY THRESHOLDS (defaults - can be changed via web interface) ———
int stopDistance = 20;     // STOP position - all LEDs solid red
int maxDistance = 200;     // Maximum detection range - beyond this LEDs are off
int yellowThreshold = 100; // Distance where LEDs switch from green to yellow

// ——— TIMING ———
#define LOOP_DELAY_MS 100
#define STARTUP_BLINK_MS 500

Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences preferences;

/**
 * Lights up a triplet of LEDs symmetrically from both ends of the strip
 * @param tripletIndex: Which triplet to control (0-9)
 * @param color: RGB color as uint32_t
 */
void setSymmetricalTriplet(int tripletIndex, uint32_t color)
{
  int startIndex = tripletIndex * LEDS_PER_TRIPLET;

  // Light up triplet from the beginning of strip
  ledStrip.setPixelColor(startIndex, color);
  ledStrip.setPixelColor(startIndex + 1, color);
  ledStrip.setPixelColor(startIndex + 2, color);

  // Light up corresponding triplet from the end of strip (mirrored)
  ledStrip.setPixelColor(NUM_LEDS - (startIndex + 1), color);
  ledStrip.setPixelColor(NUM_LEDS - (startIndex + 2), color);
  ledStrip.setPixelColor(NUM_LEDS - (startIndex + 3), color);
}

/**
 * Display LEDs based on distance with zone-based behavior
 */
void displayDistanceGradient(float distance)
{
  if (distance > maxDistance)
  {
    // Beyond max distance: all LEDs off
    for (int triplet = 0; triplet < MAX_TRIPLETS; triplet++)
    {
      setSymmetricalTriplet(triplet, ledStrip.Color(0, 0, 0));
    }
  }
  else if (distance <= stopDistance)
  {
    // At or below stop: all LEDs solid red
    for (int triplet = 0; triplet < MAX_TRIPLETS; triplet++)
    {
      setSymmetricalTriplet(triplet, ledStrip.Color(255, 0, 0));
    }
  }
  else
  {
    // Calculate how many LEDs should be ON (turning off from outside-in as car approaches)
    float rangeSize = maxDistance - stopDistance;
    float progress = (distance - stopDistance) / rangeSize; // 1.0 at max, 0.0 at stop
    int tripletsToLight = (int)(progress * MAX_TRIPLETS);

    if (tripletsToLight > MAX_TRIPLETS)
      tripletsToLight = MAX_TRIPLETS;
    if (tripletsToLight < 1)
      tripletsToLight = 1;

    // Determine color based on zone
    uint32_t color;
    if (distance >= yellowThreshold)
    {
      // Green zone (far away)
      color = ledStrip.Color(0, 255, 0);
    }
    else
    {
      // Yellow zone (getting closer)
      color = ledStrip.Color(255, 255, 0);
    }

    // Light up triplets from OUTSIDE IN (higher indices stay on longer)
    for (int triplet = 0; triplet < MAX_TRIPLETS; triplet++)
    {
      // Invert: outer triplets (higher index) stay on longer
      if (triplet >= (MAX_TRIPLETS - tripletsToLight))
      {
        setSymmetricalTriplet(triplet, color);
      }
      else
      {
        // Turn off inner triplets first as car approaches
        setSymmetricalTriplet(triplet, ledStrip.Color(0, 0, 0));
      }
    }
  }
}

/**
 * Measures distance using HC-SR04 ultrasonic sensor
 * @return: Distance in centimeters
 */
float measureDistanceCM()
{
  // Send ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Measure echo duration
  float pulseDuration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);

  // Convert duration to distance: (time * speed_of_sound) / 2
  return (pulseDuration * SOUND_SPEED_CM_US) / 2;
}

/**
 * Startup indicator - blinks all LEDs red
 */
void startupBlink()
{
  for (int triplet = 0; triplet < MAX_TRIPLETS; triplet++)
  {
    setSymmetricalTriplet(triplet, ledStrip.Color(255, 0, 0));
  }
  ledStrip.show();
  delay(STARTUP_BLINK_MS);
  for (int triplet = 0; triplet < MAX_TRIPLETS; triplet++)
  {
    setSymmetricalTriplet(triplet, ledStrip.Color(0, 0, 0));
  }
  ledStrip.show();
}

// ——— WEB SERVER FUNCTIONS ———

void loadSettings()
{
  preferences.begin("parking", false);
  stopDistance = preferences.getInt("stopDist", 20);
  maxDistance = preferences.getInt("maxDist", 200);
  yellowThreshold = preferences.getInt("yellowThresh", 100);
  preferences.end();

  Serial.println("Settings loaded:");
  Serial.print("  Stop distance: ");
  Serial.println(stopDistance);
  Serial.print("  Max distance: ");
  Serial.println(maxDistance);
  Serial.print("  Yellow threshold: ");
  Serial.println(yellowThreshold);
}

void saveSettings()
{
  preferences.begin("parking", false);
  preferences.putInt("stopDist", stopDistance);
  preferences.putInt("maxDist", maxDistance);
  preferences.putInt("yellowThresh", yellowThreshold);
  preferences.end();
  Serial.println("Settings saved!");
}

void handleRoot()
{
  float currentDistance = measureDistanceCM();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;max-width:500px;margin:20px auto;padding:20px;background:#f0f0f0}";
  html += "h1{color:#333;text-align:center}";
  html += ".box{background:white;padding:20px;border-radius:10px;margin:10px 0;box-shadow:0 2px 5px rgba(0,0,0,0.1)}";
  html += ".current{font-size:24px;color:#007bff;text-align:center;padding:15px}";
  html += "label{display:block;margin:10px 0 5px;font-weight:bold;color:#555}";
  html += "input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}";
  html += "button{width:100%;padding:15px;background:#28a745;color:white;border:none;border-radius:5px;font-size:18px;cursor:pointer;margin-top:15px}";
  html += "button:hover{background:#218838}";
  html += ".info{font-size:12px;color:#666;margin-top:5px}";
  html += "</style></head><body>";
  html += "<h1>🚗 Parking Sensor Config</h1>";

  html += "<div class='box'><div class='current'>Current Distance: <b>" + String(currentDistance, 1) + " cm</b></div></div>";

  html += "<form method='POST' action='/save'>";
  html += "<div class='box'>";
  html += "<label>🔴 Stop Distance (cm)</label>";
  html += "<input type='number' name='stop' value='" + String(stopDistance) + "' min='5' max='100'>";
  html += "<div class='info'>Perfect parking position - all LEDs solid RED</div>";

  html += "<label>⚪ Maximum Detection (cm)</label>";
  html += "<input type='number' name='max' value='" + String(maxDistance) + "' min='50' max='500'>";
  html += "<div class='info'>Beyond this distance, LEDs turn off</div>";

  html += "<label>🟡 Yellow Threshold (cm)</label>";
  html += "<input type='number' name='yellow' value='" + String(yellowThreshold) + "' min='30' max='300'>";
  html += "<div class='info'>Below this: YELLOW, Above this: GREEN</div>";
  html += "</div>";

  html += "<button type='submit'>💾 Save Settings</button>";
  html += "</form>";
  html += "<div style='text-align:center;margin-top:20px;color:#888;font-size:12px'>Refresh page to see current distance</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave()
{
  if (server.hasArg("stop"))
  {
    stopDistance = server.arg("stop").toInt();
  }
  if (server.hasArg("max"))
  {
    maxDistance = server.arg("max").toInt();
  }
  if (server.hasArg("yellow"))
  {
    yellowThreshold = server.arg("yellow").toInt();
  }

  saveSettings();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2;url=/'>";
  html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f0f0}";
  html += "h1{color:#28a745}</style></head><body>";
  html += "<h1>✓ Settings Saved!</h1>";
  html += "<p>Redirecting...</p></body></html>";

  server.send(200, "text/html", html);
}

void setup()
{
  Serial.begin(115200);

  // Load saved settings from EEPROM
  loadSettings();

  // Initialize LED strip
  ledStrip.begin();
  ledStrip.setBrightness(BRIGHTNESS);
  ledStrip.show(); // Initialize all pixels to 'off'

  delay(500);

  // Initialize ultrasonic sensor pins
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  // Signal successful startup
  startupBlink();

  // Start WiFi Access Point
  Serial.println("Starting WiFi Access Point...");
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("Connect to WiFi: " + String(AP_SSID));
  Serial.println("Password: " + String(AP_PASSWORD));
  Serial.println("Then open browser to: http://" + IP.toString());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Web server started!");
}

void loop()
{
  // Handle web server requests
  server.handleClient();

  float currentDistance = measureDistanceCM();

  // Print distance to serial console
  Serial.print("Distance: ");
  Serial.print(currentDistance);
  Serial.println(" cm");

  // Display gradient based on current distance
  displayDistanceGradient(currentDistance);

  // Update the LED display
  ledStrip.show();
  delay(LOOP_DELAY_MS);
}
