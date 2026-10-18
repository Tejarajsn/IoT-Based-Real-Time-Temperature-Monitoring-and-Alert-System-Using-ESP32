#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// =============== SENSOR AND BUZZER ===============
#define TEMP_SENSOR 17
#define BUZZER_PIN 27

// =============== OLED CONFIGURATION ===============
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// =============== WIFI CONFIGURATION ===============
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

// =============== ADAFRUIT IO CREDENTIALS ===============
#define IO_USERNAME "tejaraj_sn13"
#define IO_KEY  "key"

// =============== ADAFRUIT IO SERVER ===============
#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883

// Feed key shown as TEMP_VALUE on your dashboard
#define TEMP_FEED IO_USERNAME "/feeds/temperature"

// =============== TEMPERATURE SENSOR ===============
OneWire oneWire(TEMP_SENSOR);
DallasTemperature sensors(&oneWire);

// =============== OLED DISPLAY ===============
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =============== MQTT CLIENT ===============
WiFiClient client;

Adafruit_MQTT_Client mqtt(
  &client,
  AIO_SERVER,
  AIO_SERVERPORT,
  IO_USERNAME,
  IO_KEY
);

Adafruit_MQTT_Publish temperatureFeed =
  Adafruit_MQTT_Publish(&mqtt, TEMP_FEED);

// =============== TIMERS ===============
unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 10000;

// =============== WIFI CONNECTION ===============
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }
}

// =============== ADAFRUIT IO CONNECTION ===============
bool connectAdafruit() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (mqtt.connected()) {
    return true;
  }

  Serial.println("Connecting to Adafruit IO...");

  int8_t result = mqtt.connect();

  if (result == 0) {
    Serial.println("Adafruit IO connected!");
    return true;
  }

  Serial.print("Adafruit IO connection failed: ");
  Serial.println(mqtt.connectErrorString(result));

  mqtt.disconnect();

  return false;
}

// =============== SETUP ===============
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize DS18B20
  sensors.begin();

  Serial.print("DS18B20 sensors detected: ");
  Serial.println(sensors.getDeviceCount());

  // Initialize OLED: SDA = GPIO 21, SCL = GPIO 22
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed!");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Temperature Monitor");
    display.println("Connecting...");
    display.display();
  }

  connectWiFi();
}

// =============== MAIN LOOP ===============
void loop() {
  // Read the temperature
  sensors.requestTemperatures();

  float temperature = sensors.getTempCByIndex(0);

  bool sensorError = (temperature == DEVICE_DISCONNECTED_C);

  // Print temperature to Serial Monitor
  Serial.print("Temperature: ");

  if (sensorError) {
    Serial.println("Sensor disconnected!");
  } else {
    Serial.print(temperature, 2);
    Serial.println(" C");
  }

  // Update OLED
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TEMPERATURE MONITOR");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 22);

  if (sensorError) {
    display.println("Sensor Err");
  } else {
    display.print(temperature, 1);
    display.println(" C");
  }

  display.setTextSize(1);
  display.setCursor(0, 48);

  if (sensorError) {
    display.println("Check sensor");
  } else if (temperature > 60.0) {
    display.println("WARNING: HIGH TEMP!");
  } else {
    display.println("Temperature Normal");
  }

  display.display();

  // Activate buzzer above 60 C
  if (!sensorError && temperature > 60.0) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Publish only once every 10 seconds
  if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = millis();

    connectWiFi();

    if (connectAdafruit()) {
      bool success = false;

      if (!sensorError) {
        success = temperatureFeed.publish(temperature);
      }

      if (success) {
        Serial.println("Temperature uploaded to Adafruit IO!");
      } else if (sensorError) {
        Serial.println("Upload skipped: temperature sensor error.");
      } else {
        Serial.println("Upload failed. Check feed name and AIO key.");
      }

      mqtt.processPackets(100);
      mqtt.ping();
    } else {
      Serial.println("Adafruit IO unavailable. Will retry.");
    }
  }

  delay(500);
}