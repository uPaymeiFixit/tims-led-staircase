/**
 * Power stress test - set all LEDs to white at full brightness
 */

#include <FastLED.h>

// Hardware configuration
#define LED_PIN 25
#define NUM_LEDS 980

// LED array
CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
}

void loop() {
  // Set all LEDs to white
  fill_solid(leds, NUM_LEDS, CRGB(255, 255, 255));
  FastLED.show();

  // Wait 2 seconds
  delay(2000);
}
