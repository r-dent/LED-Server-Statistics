#include <time.h>
#include <FastLED.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiClient.h>

 
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 OLED(OLED_RESET);

#define NUM_LEDS 30
#define LED_DATA_PIN D4
CRGB leds[NUM_LEDS];

bool blink = 0;

const int buttonPin = D3;
int buttonState = 0;
time_t lastSwitchTime = 0;

const int logMaxLines = 4;
const int logLineLength = 21;
int logBufferLines = 0;
char logBuffer[logMaxLines][logLineLength];

void displayLog(const char *text) {
  OLED.clearDisplay();
  OLED.setCursor(0, 0);

  if (logBufferLines < logMaxLines) {
    strcpy(logBuffer[logBufferLines], text);
    logBufferLines = logBufferLines + 1;
  }
  else {
    // Shift buffer content up.
    for (int i = 1; i < logMaxLines; i++) {
      strcpy(logBuffer[i -1], logBuffer[i]);
    }
    // Insert new text as last line.
    strcpy(logBuffer[logMaxLines - 1], text);
  }

  for (int i = 0; i < logBufferLines; i++) {
    OLED.println(logBuffer[i]);
  }

  OLED.display();
}
 
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

} 
 
void loop() {
  time_t currTime = millis();

  buttonState = digitalRead(buttonPin);
  if (currTime - lastSwitchTime > 300 && buttonState == LOW) {
    blink = !blink;
    lastSwitchTime = currTime;

    for(int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(HUE_ORANGE, 255, 255 * (blink ? 0.5 : 0));
    }
    FastLED.show(); 

    displayLog(blink ? "Switched ON" : "Switched OFF");
  }

  delay(10);
}