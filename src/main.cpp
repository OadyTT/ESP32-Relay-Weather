#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"  // WiFi SSID/Password และ API Key (ไม่ถูก commit)
#define OWM_CITY      "Bangkok"
#define OWM_COUNTRY   "TH"
#define WEATHER_INTERVAL_MS  (2UL * 60UL * 1000UL)  // 2 นาที

// --- Relay (Active Low) ---
#define RELAY1 17
#define RELAY2 16
#define RELAY3 4

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// --- Switch (Active Low, External Pull-up) ---
#define SW1 34
#define SW2 35
#define SW3 32

#define DEBOUNCE_MS 20

struct SwitchState {
  uint8_t pin;
  bool lastRaw;
  bool stable;
  unsigned long lastChangeTime;
};

struct RelayState {
  uint8_t pin;
  bool on;
};

SwitchState sw[3] = {
  {SW1, HIGH, HIGH, 0},
  {SW2, HIGH, HIGH, 0},
  {SW3, HIGH, HIGH, 0},
};

RelayState relay[3] = {
  {RELAY1, false},
  {RELAY2, false},
  {RELAY3, false},
};

unsigned long lastWeatherFetch = 0;

String getTimeStr() {
  struct tm t;
  if (!getLocalTime(&t)) return "--:--:--";
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  return String(buf);
}

void toggleRelay(RelayState &r) {
  r.on = !r.on;
  digitalWrite(r.pin, r.on ? RELAY_ON : RELAY_OFF);
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Weather] WiFi ไม่ได้เชื่อมต่อ");
    return;
  }

  HTTPClient http;
  http.setTimeout(10000);
  JsonDocument doc;
  float lat = 0, lon = 0;

  // --- Current Weather ---
  String city = String(OWM_CITY);
  city.replace(" ", "%20");
  String weatherUrl = String("http://api.openweathermap.org/data/2.5/weather?q=") +
                      city + "," + OWM_COUNTRY +
                      "&appid=" + OWM_API_KEY + "&units=metric&lang=th";

  http.begin(weatherUrl);
  int code = http.GET();
  Serial.printf("[Weather] HTTP status: %d\n", code);

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();  // เก็บใน variable ก่อน parse
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[Weather] JSON error: %s\n", err.c_str());
    } else {
      float  temp      = doc["main"]["temp"];
      float  feels     = doc["main"]["feels_like"];
      int    humidity  = doc["main"]["humidity"];
      float  windSpeed = doc["wind"]["speed"];
      String desc      = doc["weather"][0]["description"].as<String>();  // copy เป็น String
      lat = doc["coord"]["lat"];
      lon = doc["coord"]["lon"];

      Serial.printf("========== สภาพอากาศ กรุงเทพมหานคร [%s] ==========\n", getTimeStr().c_str());
      Serial.printf("  อุณหภูมิ    : %.1f C (รู้สึกเหมือน %.1f C)\n", temp, feels);
      Serial.printf("  ความชื้น    : %d %%\n", humidity);
      Serial.printf("  ความเร็วลม  : %.1f m/s\n", windSpeed);
      Serial.printf("  สภาพ        : %s\n", desc.c_str());
    }
  } else {
    Serial.printf("[Weather] Error: %d\n", code);
  }
  http.end();

  if (lat == 0 && lon == 0) return;

  // --- Air Pollution (AQI) ---
  String aqiUrl = String("http://api.openweathermap.org/data/2.5/air_pollution?lat=") +
                  String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + OWM_API_KEY;

  http.begin(aqiUrl);
  code = http.GET();
  Serial.printf("[AQI] HTTP status: %d\n", code);

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[AQI] JSON error: %s\n", err.c_str());
    } else {
      int   aqi  = doc["list"][0]["main"]["aqi"];
      float pm25 = doc["list"][0]["components"]["pm2_5"];
      float pm10 = doc["list"][0]["components"]["pm10"];
      float co   = doc["list"][0]["components"]["co"];
      float no2  = doc["list"][0]["components"]["no2"];
      float o3   = doc["list"][0]["components"]["o3"];

      const char* aqiLabel[] = {"", "ดี", "พอใช้", "ปานกลาง", "แย่", "แย่มาก"};

      Serial.println("------------- คุณภาพอากาศ (AQI) ---------------");
      Serial.printf("  AQI         : %d (%s)\n", aqi, (aqi >= 1 && aqi <= 5) ? aqiLabel[aqi] : "?");
      Serial.printf("  PM2.5       : %.1f ug/m3\n", pm25);
      Serial.printf("  PM10        : %.1f ug/m3\n", pm10);
      Serial.printf("  CO          : %.1f ug/m3\n", co);
      Serial.printf("  NO2         : %.1f ug/m3\n", no2);
      Serial.printf("  O3          : %.1f ug/m3\n", o3);
      Serial.println("================================================");
    }
  } else {
    Serial.printf("[AQI] Error: %d\n", code);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // UTC+7 (Thailand)

  for (int i = 0; i < 3; i++) {
    pinMode(relay[i].pin, OUTPUT);
    digitalWrite(relay[i].pin, RELAY_OFF);
  }

  for (int i = 0; i < 3; i++) {
    pinMode(sw[i].pin, INPUT);
  }

  fetchWeather();  // ดึงข้อมูลทันทีเมื่อเริ่มต้น
  lastWeatherFetch = millis();
}

void loop() {
  unsigned long now = millis();

  // --- Switch debounce & relay toggle ---
  for (int i = 0; i < 3; i++) {
    bool raw = digitalRead(sw[i].pin);

    if (raw != sw[i].lastRaw) {
      sw[i].lastRaw = raw;
      sw[i].lastChangeTime = now;
    }

    if ((now - sw[i].lastChangeTime) >= DEBOUNCE_MS && raw != sw[i].stable) {
      sw[i].stable = raw;
      if (sw[i].stable == LOW) {
        toggleRelay(relay[i]);
        Serial.printf("SW%d กด -> Relay%d %s\n", i + 1, i + 1, relay[i].on ? "ON" : "OFF");
      }
    }
  }

  // --- Weather fetch ทุก 2 นาที ---
  if (now - lastWeatherFetch >= WEATHER_INTERVAL_MS) {
    lastWeatherFetch = now;
    fetchWeather();
  }
}