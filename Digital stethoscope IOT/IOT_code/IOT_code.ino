#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

// Replace with your Wi-Fi credentials
const char* ssid = "abiabi";
const char* password = "87654321";

// ThingSpeak settings
const char* server = "api.thingspeak.com";
String writeAPIKey = "C8BQJ0THF8PFD1Z4";

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MAX4466 Mic input
#define MIC_PIN A0
#define SAMPLES 10
int samples[SAMPLES];
int sampleIndex = 0;

// Heartbeat detection
unsigned long lastBeatTime = 0;
float bpm = 0;
bool beatDetected = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastThingSpeakUpdate = 0;

WiFiClient client;

// Smooth analog read
int smoothAnalogRead() {
  samples[sampleIndex] = analogRead(MIC_PIN);
  sampleIndex = (sampleIndex + 1) % SAMPLES;
  int sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += samples[i];
  return sum / SAMPLES;
}

void setup() {
  Serial.begin(115200);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Smart Stethoscope");
  display.display();
  delay(1000);

  // WiFi connect
  WiFi.begin(ssid, password);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
}

void loop() {
  int micValue = smoothAnalogRead();
  static int threshold = 950;

  if (micValue > threshold && !beatDetected) {
    unsigned long currentTime = millis();
    unsigned long timeDiff = currentTime - lastBeatTime;

    if (timeDiff > 600) {
      bpm = 60000.0 / timeDiff;
      lastBeatTime = currentTime;
      beatDetected = true;

      Serial.print("Beat detected! BPM: ");
      Serial.println(bpm);
    }
  } else if (micValue < threshold) {
    beatDetected = false;
  }

  // OLED update every 1s
  if (millis() - lastDisplayUpdate > 1000) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Smart Stethoscope");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("BPM: ");
    display.println((int)bpm);

    display.setTextSize(1);
    display.setCursor(0, 50);
    if (bpm < 60) display.println("Bradycardia");
    else if (bpm > 100) display.println("Tachycardia");
    else display.println("Normal");

    display.display();
    lastDisplayUpdate = millis();
  }

  // ThingSpeak update every 20s
  if (millis() - lastThingSpeakUpdate > 10000 && bpm > 0) {
    if (client.connect(server, 80)) {
      String postStr = "api_key=" + writeAPIKey + "&field1=" + String((int)bpm);
      client.println("POST /update HTTP/1.1");
      client.println("Host: api.thingspeak.com");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(postStr.length());
      client.println();
      client.println(postStr);

      Serial.println("BPM sent to ThingSpeak.");
    } else {
      Serial.println("ThingSpeak connection failed.");
    }

    client.stop();
    lastThingSpeakUpdate = millis();
  }
}

