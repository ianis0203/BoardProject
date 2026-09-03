#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// ===== CONFIGURATION =====
// --- Sensor Pins ---
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// --- LED and Buzzer Pins ---
#define LED_PIN 27
#define BUZ_PIN 26

// Alert Thresholds
float THRESHOLD = 26.0;
const unsigned long READ_PERIOD = 2000; // Read every 2s

// Global variables
unsigned long lastRead = 0;
bool alarmState = false; 

// --- Control LED + Buzzer ---
void setAlarm(bool state) {
  digitalWrite(LED_PIN, state ? HIGH : LOW);
  digitalWrite(BUZ_PIN, state ? HIGH : LOW);
  alarmState = state;
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZ_PIN, OUTPUT);
  setAlarm(false);

  // Blink LED to indicate startup
  for(int i=0; i<3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);
  }
}

// --- Main Loop ---
void loop() {
  unsigned long now = millis();

  // Read Sensor
  if (now - lastRead >= READ_PERIOD) {
    lastRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // Send JSON formatted data to Serial
      // Format: {"temperature": 25.5, "humidity": 60.2}
      Serial.print("{\"temperature\": ");
      Serial.print(t, 1);
      Serial.print(", \"humidity\": ");
      Serial.print(h, 1);
      Serial.println("}");

      // Check Threshold for Local Alarm
      if (t > THRESHOLD) {
        setAlarm(true); 
      } else {
        if (alarmState) {
          setAlarm(false); 
        }
      }
    } else {
      Serial.println("{\"error\": \"Sensor Read Failed\"}");
    }
  }
}