#include <time.h>
#include <FastLED.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
 
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 OLED(OLED_RESET);

#define NUM_LEDS 30
#define LED_DATA_PIN D4
CRGB leds[NUM_LEDS];

int lightness = 255 / 5;

const int buttonPin = D3;
int buttonState = 0;
time_t lastSwitchTime = 0;

const int logMaxLines = 4;
const int logLineLength = 21;
int logBufferLines = 0;
String logBuffer[logMaxLines];

void displayLog(String text) {
  OLED.clearDisplay();
  OLED.setCursor(0, 0);

  if (logBufferLines < logMaxLines) {
    logBuffer[logBufferLines] = text;
    logBufferLines = logBufferLines + 1;
  }
  else {
    // Shift buffer content up.
    for (int i = 1; i < logMaxLines; i++) {
      logBuffer[i -1] = logBuffer[i];
    }
    // Insert new text as last line.
    logBuffer[logMaxLines - 1] = text;
  }

  for (int i = 0; i < logBufferLines; i++) {
    OLED.println(logBuffer[i]);
  }

  OLED.display();
}

int hueForLanguage(const char* lang) {
  if (strcmp("en", lang) == 0) { return HUE_GREEN; }
  if (strcmp("es", lang) == 0) { return HUE_ORANGE; }
  if (strcmp("de", lang) == 0) { return HUE_YELLOW; }
  if (strcmp("fr", lang) == 0) { return HUE_BLUE; }
  if (strcmp("it", lang) == 0) { return HUE_RED; }
  if (strcmp("pt", lang) == 0) { return HUE_PINK; }
  if (strcmp("nl", lang) == 0) { return HUE_AQUA; }
  if (strcmp("ru", lang) == 0) { return HUE_PURPLE; }
}

// WiFi related.

WiFiManager wifiManager;

void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  displayLog("Entered config mode");
}

void wifiSaveConfigCallback () {
  displayLog("Sucessfully connected");
}

// WebSocket related.

#define HEARTBEAT_INTERVAL 25000
#define SOCKET_SERVER "rapscript.net"
#define SOCKET_PORT 8143

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

WebSocketsClient webSocket;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      isConnected = false;
      break;

    case WStype_CONNECTED: {
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      isConnected = true;
      displayLog(String("ws:") + SOCKET_SERVER + ":" + SOCKET_PORT);
      // Send message to server when Connected. Socket.io upgrade confirmation message (required).
      webSocket.sendTXT("5");
      // Subscribe to statistics message.
      webSocket.sendTXT("42[\"subscribe stats\",{}]");
    }
    break;

    case WStype_TEXT: {
      Serial.printf("[WSc] get text: %s\n", payload);

      String payloadString = String((char*)payload);
      // Assume that a bracket indicated a JSON array.
      int firstBracket = payloadString.indexOf("[");
      // Found JSON content.
      if (firstBracket > -1) {
        // Ignore everything before the bracket.
        String messageString = payloadString.substring(firstBracket);
        // Parse JSON. Assume that wa have a JSON array with [messageType, content].
        const size_t bufferSize = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(8) + 80;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonArray& message = jsonBuffer.parseArray(messageString);

        if (strcmp(message[0], "statistics") == 0) {
          JsonObject& stats = message[1];
          int ledPos = 0;
          String summary = "";
          // Iterate through all returned languages.
          for (const auto& kv : stats) {
            int userCount = kv.value;
            if (userCount > 0) {
              const char* lang = kv.key;
              int hue = hueForLanguage(lang);
              // Set LEDs to language specific color.
              for (int i = ledPos; i < min(ledPos + userCount, NUM_LEDS); i++) {
                leds[i] = CHSV(hue, 255, lightness);
              }
              ledPos += userCount;
              summary += String(lang) + ":" + String(userCount) + " ";
            }
          }
          // Set the rest of the LEDs black.
          for (int i = ledPos; i < NUM_LEDS; i++) {
            leds[i] = BLACK;
            ledPos++;
          }
          FastLED.show();
          displayLog(summary);
        }
      }
      // send message to server
      // webSocket.sendTXT("message here");
    }
    break;

    default:
      Serial.print("Received WS message.\n");
      break;
  }
}

/*
 * Setup 
 */

void setup()   {
  Serial.begin(115200);

  OLED.begin();
  OLED.setTextWrap(false);
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  // Set LEDs to color.
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(HUE_ORANGE, 255, 0);
  }

  displayLog("Welcome!");

  wifiManager.setAPCallback(wifiConfigModeCallback);
  wifiManager.setSaveConfigCallback(wifiSaveConfigCallback);
  wifiManager.autoConnect();

  if (WiFi.status() == WL_CONNECTED) {
    displayLog(WiFi.SSID());
    displayLog(WiFi.localIP().toString());
  }
  else {
    displayLog("No Wifi connection.");
  }

  webSocket.beginSocketIOSSL(SOCKET_SERVER, SOCKET_PORT);
  webSocket.onEvent(webSocketEvent);
} 

/*
 * Loop 
 */
void loop() {
  time_t currTime = millis();

  buttonState = digitalRead(buttonPin);
  if (currTime - lastSwitchTime > 300 && buttonState == LOW) {
    lightness = (lightness + (255 / 5)) % 255;
    lastSwitchTime = currTime;
    // Re-subscribe to trigger stats again.
    webSocket.sendTXT("42[\"subscribe stats\",{}]");
  }

  // WebSocket Server.

  webSocket.loop();

  if (isConnected) {
    uint64_t now = millis();

    if ((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
      heartbeatTimestamp = now;
      // socket.io heartbeat message
      webSocket.sendTXT("2");
    }
  }
}