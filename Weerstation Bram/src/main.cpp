#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <time.h>

// --- WiFi ---
const char* ssid = "ssid";
const char* password = "password";
const char* serverUrl = "serverurl";
const char* API_token = "API_token";
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
  bool synced = false;
  while (!(synced = getLocalTime(&timeinfo)) && retry < 40) { // increase retries
    delay(1000);
    retry++;
  }
  if (synced) {
    Serial.println("Time synced.");
    Serial.printf("Year: %d Month: %d Day: %d Hour: %d Min: %d Sec: %d\n",
      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
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
  float vin = vout * (4.0f / 3.0f); // 1M + 3M divider
  return vin;
}

void reportData() {
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F; // hPa
  float humidity = dht.readHumidity();
  float chargerVoltage = readChargerVoltage();

  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo); // Get UTC time
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

  Serial.printf("Report @ %s | T=%.2f C | P=%.1f hPa | H=%.1f %% | Charger=%.2f V\n",
                timestamp, temperature, pressure, humidity, chargerVoltage);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", API_token); 
    String payload = String("{\"time\":\"") + timestamp +
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
    syncTimeOnce();
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