#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <time.h>
#include <ArduinoJson.h>

// --- WiFi ---
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* serverUrl = "https://weerstationbram.nl/api/meteo_Arnhem";
const char* API_token = "API_TOKEN";
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
RTC_DATA_ATTR int lastUploadMinute = -1; // Retained during deep sleep

void syncTime() {
  configTime(0, 0, NTP_SERVER);
  struct tm timeinfo;
  int retries = 5;
  while (retries-- > 0) {
    if (getLocalTime(&timeinfo)) {
      Serial.println("Time synced.");
      Serial.printf("Year: %d Month: %d Day: %d Hour: %d Min: %d Sec: %d\n",
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return;
    }
    delay(1000); // wait 1 second before retry
  }
  Serial.println("Time sync failed after retries!");
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

// Voltage divider: R1 = 1M, R2 = 3M
// Vout = Vin * (R2 / (R1 + R2)) => Vin = Vout * ((R1 + R2) / R2)
// ADC range: 0-4095, reference voltage: 3.3V
float readChargerVoltage() {
  int adc = analogRead(CHARGER_VOLT_PIN);
  float vout = adc * (3.3f / 4095.0f);
  float vin = vout * (4.0f / 3.0f); // 1M + 3M divider
  return vin;
}

void setJsonFloatOrNull(JsonDocument& doc, const char* key, float value) {
  if (isnan(value)) {
    doc[key] = nullptr; // ArduinoJson treats nullptr as JSON null
  } else {
    doc[key] = value;
  }
}

void reportData() {
  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  bmp.begin(0x77);
  pinMode(CHARGER_VOLT_PIN, INPUT);

  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F; // hPa
  float humidity = dht.readHumidity();
  float chargerVoltage = round(readChargerVoltage() * 100000) / 100000; // round to 5 decimal places

  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo); // Get UTC time
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

  JsonDocument doc;
  doc["time"] = timestamp;
  setJsonFloatOrNull(doc, "temperature", temperature);
  setJsonFloatOrNull(doc, "pressure", pressure);
  setJsonFloatOrNull(doc, "humidity", humidity);
  setJsonFloatOrNull(doc, "charger_voltage", chargerVoltage);

  String payload;
  serializeJson(doc, payload);
  Serial.println(payload); 

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_token);
    int code = http.POST(payload);
    Serial.printf("HTTP Response: %d\n", code);
    http.end();
  }
}

void setup() {

  Serial.begin(115200);
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  connectWiFi();
  syncTime();

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int minute = timeinfo.tm_min;

  if (cause == ESP_SLEEP_WAKEUP_TIMER && minute % 5 == 0 && minute != lastUploadMinute) {
    lastUploadMinute = minute;
    reportData();
  }

  WiFi.disconnect(true);

  getLocalTime(&timeinfo);
  time_t now = mktime(&timeinfo); 

  minute = timeinfo.tm_min;
  int nextMinute = ((minute / 5) + 1) * 5;
  int nextHour = timeinfo.tm_hour;
  if (nextMinute >= 60) {
      nextMinute = 0;
      nextHour += 1;
  }

  struct tm nextTime = timeinfo;
  nextTime.tm_min = nextMinute;
  nextTime.tm_hour = nextHour;
  nextTime.tm_sec = 2;
  time_t target = mktime(&nextTime);
  int sleepSeconds = target - now;
  if (sleepSeconds <= 0) sleepSeconds = 1; // fallback

  Serial.printf("Sleeping for %d seconds until next 5-min mark...\n", sleepSeconds);
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}