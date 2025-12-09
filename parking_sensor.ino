#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ——— WiFi CONFIGURATION ———
const char *AP_SSID = "ParkingSensor";
const char *AP_PASSWORD = "configure123";

// ——— LED CONFIGURATION ———
// This is tuned for 60 leds
#define LED_PIN 17                   // GPIO pin for LED strip data line
#define NUM_LEDS 60                  // Total number of LEDs on the strip
#define BRIGHTNESS 100               // LED brightness (0-255) - REDUCED for USB power
#define LEDS_PER_SIDE (NUM_LEDS / 2) // Number of LEDs per side (strip lit symmetrically from both ends)

// ——— DISTANCE SENSOR CONFIGURATION ———
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 12
#define SOUND_SPEED_CM_US 0.0343 // Speed of sound in cm/microsecond

// ——— PROXIMITY THRESHOLDS (defaults - can be changed via web interface) ———
int stopDistance = 20; // STOP position (cm) - all LEDs solid red
int maxDistance = 400; // Maximum detection range (cm) - beyond this LEDs are off
// yellowThreshold: yellow is last 15% before stop, green is remaining 85% of range

// ——— FILTERING ———
#define FILTER_SAMPLES 5        // Number of readings to average (simple moving average)
#define SENSOR_TIMEOUT_US 50000 // Ultrasonic sensor timeout in microseconds (50ms = ~8.5m max range)
float distanceReadings[FILTER_SAMPLES];
int readingIndex = 0;
float averageDistance = 0;

// ——— TIMING ———
#define LOOP_DELAY_MS 100
#define STARTUP_BLINK_MS 500
#define AUTO_SHUTOFF_MS 150000 // 2.5 minutes (150 seconds)

// ——— STATE TRACKING ———
unsigned long parkedAtStopTime = 0; // When car reached stop distance
bool ledsAutoOff = false;           // Track if LEDs are auto-shutoff

Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences preferences;

/**
 * Lights up a single LED symmetrically from both ends of the strip
 * @param ledIndex: Which LED position to control (0-29, from outside-in)
 * @param color: RGB color as uint32_t
 */
void setSymmetricalLED(int ledIndex, uint32_t color)
{
  // Light up LED from the beginning of strip
  ledStrip.setPixelColor(ledIndex, color);

  // Light up corresponding LED from the end of strip (mirrored)
  ledStrip.setPixelColor(NUM_LEDS - 1 - ledIndex, color);
}

/**
 * Display LEDs based on distance with zone-based behavior
 */
void displayDistanceGradient(float distance, bool forceOff)
{
  if (forceOff || distance > maxDistance)
  {
    // LEDs off (either forced off or beyond max distance)
    for (int i = 0; i < LEDS_PER_SIDE; i++)
    {
      setSymmetricalLED(i, ledStrip.Color(0, 0, 0));
    }
  }
  else if (distance <= stopDistance)
  {
    // At or below stop: all LEDs solid red
    for (int i = 0; i < LEDS_PER_SIDE; i++)
    {
      setSymmetricalLED(i, ledStrip.Color(255, 0, 0));
    }
  }
  else
  {
    // Calculate how many LEDs should be ON (turning off from outside-in as car approaches)
    float rangeSize = maxDistance - stopDistance;
    float progress = (distance - stopDistance) / rangeSize; // 1.0 at max, 0.0 at stop

    // Apply square root to make LED changes more consistent across the distance range
    // This makes LEDs turn off more evenly as you approach (rather than faster at far distances)
    progress = sqrt(progress);

    int ledsToLight = (int)(progress * LEDS_PER_SIDE);

    if (ledsToLight > LEDS_PER_SIDE)
      ledsToLight = LEDS_PER_SIDE;
    if (ledsToLight < 1)
      ledsToLight = 1;

    // Calculate yellow threshold: yellow is last 15% of range before stop
    float yellowThreshold = stopDistance + (rangeSize * 0.15);

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

    // Light up LEDs from OUTSIDE IN (higher indices stay on longer)
    for (int i = 0; i < LEDS_PER_SIDE; i++)
    {
      // Invert: outer LEDs (higher index) stay on longer
      if (i >= (LEDS_PER_SIDE - ledsToLight))
      {
        setSymmetricalLED(i, color);
      }
      else
      {
        // Turn off inner LEDs first as car approaches
        setSymmetricalLED(i, ledStrip.Color(0, 0, 0));
      }
    }
  }
}

/**
 * Measures distance using HC-SR04 ultrasonic sensor with moving average filter
 * @return: Distance in centimeters (filtered)
 */
float measureDistanceCM()
{
  // Send ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Measure echo duration with timeout
  float pulseDuration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, SENSOR_TIMEOUT_US);

  // Convert duration to distance: (time * speed_of_sound) / 2
  float rawDistance = (pulseDuration * SOUND_SPEED_CM_US) / 2;

  // If sensor times out, use previous average
  if (rawDistance == 0 && averageDistance > 0)
  {
    rawDistance = averageDistance;
  }

  // Add to circular buffer
  distanceReadings[readingIndex] = rawDistance;
  readingIndex = (readingIndex + 1) % FILTER_SAMPLES;

  // Calculate simple moving average
  float sum = 0;
  for (int i = 0; i < FILTER_SAMPLES; i++)
  {
    sum += distanceReadings[i];
  }
  averageDistance = sum / FILTER_SAMPLES;

  return averageDistance;
}

/**
 * Startup indicator - blinks all LEDs red
 */
void startupBlink()
{
  for (int i = 0; i < LEDS_PER_SIDE; i++)
  {
    setSymmetricalLED(i, ledStrip.Color(255, 0, 0));
  }
  ledStrip.show();
  delay(STARTUP_BLINK_MS);
  for (int i = 0; i < LEDS_PER_SIDE; i++)
  {
    setSymmetricalLED(i, ledStrip.Color(0, 0, 0));
  }
  ledStrip.show();
}

// ——— WEB SERVER FUNCTIONS ———

void loadSettings()
{
  preferences.begin("parking", false);
  stopDistance = preferences.getInt("stopDist", stopDistance);
  maxDistance = preferences.getInt("maxDist", maxDistance);
  preferences.end();

  Serial.println("Settings loaded:");
  Serial.print("  Stop distance: ");
  Serial.println(stopDistance);
  Serial.print("  Max distance: ");
  Serial.println(maxDistance);
  float yellowThreshold = stopDistance + ((maxDistance - stopDistance) * 0.15);
  Serial.print("  Yellow threshold (calculated): ");
  Serial.println(yellowThreshold);
}

void saveSettings()
{
  preferences.begin("parking", false);
  preferences.putInt("stopDist", stopDistance);
  preferences.putInt("maxDist", maxDistance);
  preferences.end();
  Serial.println("Settings saved!");
}

void handleRoot()
{
  float currentDistance = measureDistanceCM();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
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
  html += "</div>";

  html += "<div class='box' style='background:#f8f9fa;border:2px dashed #ccc'>";
  html += "<div style='color:#666;font-size:14px'>";
  html += "<strong>Color Zones (Auto-calculated):</strong><br>";
  float range = maxDistance - stopDistance;
  float yellowCalc = stopDistance + (range * 0.15);
  html += "🟢 Green: " + String((int)yellowCalc) + "-" + String(maxDistance) + " cm (85%)<br>";
  html += "🟡 Yellow: " + String(stopDistance) + "-" + String((int)yellowCalc) + " cm (15%)<br>";
  html += "🔴 Red: ≤" + String(stopDistance) + " cm (STOP)";
  html += "</div></div>";

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

  saveSettings();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
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

  // Initialize distance readings array with first reading
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  float pulseDuration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, SENSOR_TIMEOUT_US);
  float initialDistance = (pulseDuration * SOUND_SPEED_CM_US) / 2;
  for (int i = 0; i < FILTER_SAMPLES; i++)
  {
    distanceReadings[i] = initialDistance;
  }

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

  // Track when car parks at stop distance
  if (currentDistance <= stopDistance)
  {
    if (parkedAtStopTime == 0)
    {
      // Just arrived at stop position
      parkedAtStopTime = millis();
      ledsAutoOff = false;
      Serial.println("Car parked - starting auto-shutoff timer");
    }
    else if (!ledsAutoOff && (millis() - parkedAtStopTime >= AUTO_SHUTOFF_MS))
    {
      // Timer expired - turn off LEDs
      ledsAutoOff = true;
      Serial.println("Auto-shutoff: LEDs turning off");
    }
  }
  else
  {
    // Car moved away from stop position - reset timer
    if (parkedAtStopTime != 0)
    {
      Serial.println("Car moved - resetting auto-shutoff timer");
    }
    parkedAtStopTime = 0;
    ledsAutoOff = false;
  }

  // Display gradient based on current distance
  displayDistanceGradient(currentDistance, ledsAutoOff);

  // Update the LED display
  ledStrip.show();
  delay(LOOP_DELAY_MS);
}
