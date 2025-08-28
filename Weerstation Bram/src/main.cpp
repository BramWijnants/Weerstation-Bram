#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <time.h>

// --- WiFi ---
const char* ssid = "SSID";
const char* password = "PASSWORD!";
const char* serverUrl = "serverUrl";
const char* NTP_SERVER = "pool.ntp.org";

// --- Sensors ---
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_BMP280 bmp;

#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// --- Tipping bucket ---
#define BUCKET_PIN 33   // RTC-capable GPIO
RTC_DATA_ATTR uint32_t tipCount = 0;
const float MM_PER_TIP = 0.2;

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

void reportData() {
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F; // hPa
  float humidity = dht.readHumidity();
  float rainfall = tipCount * MM_PER_TIP;

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  Serial.printf("Report @ %s | T=%.2f C | P=%.1f hPa | H=%.1f %% | Rain=%.2f mm (%u tips)\n",
                timestamp, temperature, pressure, humidity, rainfall, tipCount);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    String payload = String("{\"timestamp\":\"") + timestamp +
                     "\",\"temperature\":" + temperature +
                     ",\"pressure\":" + pressure +
                     ",\"humidity\":" + humidity +
                     ",\"rain_tips\":" + tipCount +
                     ",\"rain_mm\":" + rainfall + "}";
    int code = http.POST(payload);
    Serial.printf("HTTP Response: %d\n", code);
    http.end();
  }

  // Reset counter after reporting
  tipCount = 0;
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

  pinMode(BUCKET_PIN, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // first boot
    connectWiFi();
    syncTimeOnce();
    WiFi.disconnect(true);
  }

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    tipCount++;
    Serial.printf("Rain tip! Total tips=%u\n", tipCount);
  }

  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    connectWiFi();
    reportData();
    WiFi.disconnect(true);
  }

  // Calculate sleep time until next boundary
  uint64_t sleepSeconds = secondsToNextBoundary();
  Serial.printf("Sleeping for %llu seconds...\n", sleepSeconds);

  // Enable wake on tip or timer
  esp_sleep_enable_ext1_wakeup((1ULL << BUCKET_PIN), ESP_EXT1_WAKEUP_ALL_LOW);
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);

  esp_deep_sleep_start();
}

void loop() {}
