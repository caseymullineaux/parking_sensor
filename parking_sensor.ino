#define SONIC_TRIG_PIN  5
#define SONIC_ECHO_PIN  12

void setup() {  
  Serial.begin(115200);
  pinMode(SONIC_TRIG_PIN, OUTPUT);  
  pinMode(SONIC_ECHO_PIN, INPUT);  
}

void loop() {
  float distance, duration;
  digitalWrite(SONIC_TRIG_PIN, LOW);  
  delayMicroseconds(2);  
	
  digitalWrite(SONIC_TRIG_PIN, HIGH);  
  delayMicroseconds(10);  
	
  digitalWrite(SONIC_TRIG_PIN, LOW);  
  duration = pulseIn(SONIC_ECHO_PIN, HIGH);

  distance = (duration*.0343)/2;
	
  Serial.println(String(distance) + "cm");
  delay(100);
}
