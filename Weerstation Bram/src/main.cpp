#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <time.h>

// --- WiFi ---
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* serverUrl = "SERVERURL";
const char* NTP_SERVER = "pool.ntp.org";

// --- Sensors ---
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_BMP280 bmp;

#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define CHARGER_VOLT_PIN 34  // Analog pin for charger voltage

// --- Functions ---
void syncTimeOnce() {
  configTime(0, 0, NTP_SERVER);
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 20) {
    delay(500);
    retry++;
  }
  if (retry < 20) {
    Serial.println("Time synced.");
  } else {
    Serial.println("Time sync failed!");
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi connected!" : "\nWiFi failed!");
}

// Voltage divider: R1 = 10k, R2 = 15k
// Vout = Vin * (R2 / (R1 + R2)) => Vin = Vout * ((R1 + R2) / R2)
// ADC range: 0-4095, reference voltage: 3.3V
float readChargerVoltage() {
  int adc = analogRead(CHARGER_VOLT_PIN);
  float vout = adc * (3.3f / 4095.0f);
  float vin = vout * (25.0f / 15.0f); // 10k/15k divider
  return vin;
}

void reportData() {
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F; // hPa
  float humidity = dht.readHumidity();
  float chargerVoltage = readChargerVoltage();

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  Serial.printf("Report @ %s | T=%.2f C | P=%.1f hPa | H=%.1f %% | Charger=%.2f V\n",
                timestamp, temperature, pressure, humidity, chargerVoltage);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    String payload = String("{\"timestamp\":\"") + timestamp +
                     "\",\"temperature\":" + temperature +
                     ",\"pressure\":" + pressure +
                     ",\"humidity\":" + humidity +
                     ",\"charger_voltage\":" + chargerVoltage + "}";
    int code = http.POST(payload);
    Serial.printf("HTTP Response: %d\n", code);
    http.end();
  }
}

uint64_t secondsToNextBoundary() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  int secOfHour = timeinfo.tm_min * 60 + timeinfo.tm_sec;
  int nextBoundary = ((secOfHour / 300) + 1) * 300; // 300 = 5 min
  int sleepSeconds = nextBoundary - secOfHour;
  if (sleepSeconds <= 0) sleepSeconds = 300; // safety
  return (uint64_t)sleepSeconds;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  bmp.begin(0x77);
  pinMode(CHARGER_VOLT_PIN, INPUT);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // first boot
    connectWiFi();
    syncTimeOnce();
    WiFi.disconnect(true);
  }

  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    connectWiFi();
    reportData();
    WiFi.disconnect(true);
  }

  // Calculate sleep time until next boundary
  uint64_t sleepSeconds = secondsToNextBoundary();
  Serial.printf("Sleeping for %llu seconds...\n", sleepSeconds);

  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);

  esp_deep_sleep_start();
}

void loop() {}