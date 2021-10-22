#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>

#include "config.h"

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const char *ssid = WIFI_SSID;    // your network SSID (name)
const char *password = WIFI_PASSWORD;    // your network password (use for WPA, or use as key for WEP)
const char *mqtt_server = MQTT_BROKER;

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define DATA_PIN    5
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    200
#define BRIGHTNESS   255
#define FRAMES_PER_SECOND  120

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate
//    flashed in the WiFi module.

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE    (50)
char msg[MSG_BUFFER_SIZE];
CRGB leds[NUM_LEDS];
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
unsigned long resetTime = 0;
StaticJsonDocument<200> doc;
long playerNumber = 0;

FASTLED_USING_NAMESPACE

void rainbow() {
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void greenPulse() {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 40; j++) {
            if (j <= gHue % 20 || j >= 39 - (gHue % 20)) {
                leds[i * 40 + j] = CRGB::Green;
            } else {
                leds[i * 40 + j] = CRGB::Black;
            }
        }
    }
}

void redRotate() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < 60; i++) {
        leds[(gHue * 20 + i) % NUM_LEDS] = CRGB::Red;
    }
}

void playerColor() {
    switch (playerNumber) {
        case 0:
            fill_solid(leds, NUM_LEDS, CRGB::Red);
            break;
        case 1:
            fill_solid(leds, NUM_LEDS, CRGB::Green);
            break;
        case 2:
            fill_solid(leds, NUM_LEDS, CRGB::Blue);
            break;
        case 3:
            fill_solid(leds, NUM_LEDS, CRGB::Yellow);
            break;
    }
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();

SimplePatternList gPatterns = {rainbow, greenPulse, redRotate, playerColor};

void nextPattern() {
    // add one to the current pattern number, and wrap around at the end
    gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();

    DeserializationError error = deserializeJson(doc, payload);
    // Test if parsing succeeds.
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    const char* name = doc["name"];
    playerNumber = doc["player"];

    Serial.println(name);
    Serial.println(playerNumber);

    if (strncmp(name, "correct", 10) == 0) {
        gCurrentPatternNumber = 1;
        gHue = 0;
        resetTime = millis() + 5000;
    }
    if (strncmp(name, "wrong", 10) == 0) {
        gCurrentPatternNumber = 2;
        gHue = 0;
        resetTime = millis() + 5000;
    }
    if (strncmp(name, "respond", 10) == 0) {
        gCurrentPatternNumber = 3;
        resetTime = millis() + 60 * 60 * 1000;
    }
}

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            // ... and resubscribe
            client.subscribe("cluequiz");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup() {
    pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }

    if (millis() > resetTime) {
        gCurrentPatternNumber = 0;
    }

    client.loop();
    gPatterns[gCurrentPatternNumber]();
    FastLED.show();
    EVERY_N_MILLISECONDS(100)
    { gHue++; } // slowly cycle the "base color" through the rainbow
}
