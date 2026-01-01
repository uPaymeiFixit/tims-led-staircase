/**
 * LED Staircase - Cascading Fade Animation
 *
 * Motion-activated LED animation that:
 * 1. Detects motion via PIR sensor
 * 2. Turns on each step one at a time (1 second between steps)
 * 3. Each step stays on for 2 seconds, then fades off over 3 seconds
 * 4. Animation cascades - new steps turn on while earlier steps fade
 *
 * Hardware:
 * - ESP32-WROOM-32
 * - WS2815 LED strip (980 LEDs across 14 steps)
 * - PIR motion sensor
 */

#include <FastLED.h>

// ============================================================================
// Hardware Configuration
// ============================================================================

#define LED_PIN 25       // GPIO pin for LED data signal
#define PIR_PIN 4        // GPIO pin for PIR motion sensor
#define NUM_LEDS 980     // Total LEDs in the strip
#define LED_TYPE WS2812B // WS2815 uses WS2812B protocol
#define COLOR_ORDER GRB  // Color order for WS2815

// ============================================================================
// Animation Timing
// ============================================================================

#define STEP_ON_INTERVAL_MS 650 // 1 second between each step turning on
#define STEP_HOLD_MS 1500       // 2 seconds on before fading
#define STEP_FADE_MS 1000       // 3 seconds to fade off

// ============================================================================
// Staircase Layout Constants
// ============================================================================

const int STEP_COUNT = 14;
const int MAX_LEDS_PER_STEP = 71; // Maximum LEDs on any step (steps 3-14)

// Number of LEDs on each step (steps 0-1 have 64, steps 2-13 have 71)
const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};

// Starting index in the LED array for each step
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

// ============================================================================
// Color Configuration
// ============================================================================

// Warm white color (similar to incandescent bulb, ~2700K)
const CRGB WARM_WHITE = CRGB(155, 64, 10);

// ============================================================================
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS]; // LED color buffer

// Step state tracking
struct StepState
{
  unsigned long turnOnTime; // When step turned on (0 = off)
  bool isActive;            // Whether step is part of animation
};

StepState stepStates[STEP_COUNT];

// ============================================================================
// Coordinate Mapping Function
// ============================================================================

/**
 * Convert 2D grid coordinates to 1D LED strip index.
 *
 * @param step  Step number (0-13, where 0 is top step)
 * @param pos   Horizontal position (0-70, where 0 is leftmost)
 * @return      LED index in the strip, or NUM_LEDS for virtual positions
 */
int xyToIndex(int step, int pos)
{
  // Bounds checking
  if (step < 0 || step >= STEP_COUNT || pos < 0 || pos >= MAX_LEDS_PER_STEP)
  {
    return NUM_LEDS;
  }

  // Steps 0-1 have 7 fewer LEDs on the left side
  if (step < 2 && pos < 7)
  {
    return NUM_LEDS;
  }

  int adjustedPos = (step < 2) ? pos - 7 : pos;
  int start = STEP_START[step];

  // Handle zigzag wiring: even steps go left-to-right, odd steps go right-to-left
  return (step & 1) ? start + (LEDS_PER_STEP[step] - 1 - adjustedPos) : start + adjustedPos;
}

/**
 * Safely set an LED color, handling virtual positions.
 */
void setLedSafe(int index, CRGB color)
{
  if (index < NUM_LEDS)
  {
    leds[index] = color;
  }
}

// ============================================================================
// Animation Functions
// ============================================================================

/**
 * Calculate brightness for a step based on its state.
 *
 * @param step      Step number (0-13)
 * @param currentMs Current time in milliseconds
 * @return          Brightness value (0-255)
 */
uint8_t calculateStepBrightness(int step, unsigned long currentMs)
{
  if (!stepStates[step].isActive)
  {
    return 0;
  }

  unsigned long elapsed = currentMs - stepStates[step].turnOnTime;

  // Phase 1: Full brightness during hold period (0-2000ms)
  if (elapsed < STEP_HOLD_MS)
  {
    return 255;
  }

  // Phase 2: Fade out over 3 seconds (2000-5000ms)
  unsigned long fadeElapsed = elapsed - STEP_HOLD_MS;
  if (fadeElapsed < STEP_FADE_MS)
  {
    // Linear fade from 255 to 0
    return 255 - (fadeElapsed * 255 / STEP_FADE_MS);
  }

  // Phase 3: Completely off
  return 0;
}

/**
 * Set all LEDs on a step to a specific color with brightness scaling.
 *
 * @param step       Step number (0-13)
 * @param color      Base color
 * @param brightness Brightness (0-255)
 */
void setStepColor(int step, CRGB color, uint8_t brightness)
{
  CRGB scaledColor = color;
  scaledColor.nscale8(brightness);

  for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++)
  {
    int index = xyToIndex(step, pos);
    setLedSafe(index, scaledColor);
  }
}

/**
 * Run the cascading fade animation sequence.
 */
void runCascadingAnimation()
{
  // Initialize all step states
  for (int i = 0; i < STEP_COUNT; i++)
  {
    stepStates[i].isActive = false;
    stepStates[i].turnOnTime = 0;
  }

  // Calculate total animation duration
  // Last step turns on at: 13 * 1000ms = 13000ms
  // Last step completes at: 13000ms + 2000ms (hold) + 3000ms (fade) = 18000ms
  unsigned long animationStart = millis();
  unsigned long animationDuration = (STEP_COUNT - 1) * STEP_ON_INTERVAL_MS +
                                    STEP_HOLD_MS + STEP_FADE_MS;

  int nextStepToActivate = 0;
  bool animationComplete = false;

  while (!animationComplete)
  {
    unsigned long currentMs = millis();
    unsigned long elapsed = currentMs - animationStart;

    // Activate next step if it's time
    if (nextStepToActivate < STEP_COUNT)
    {
      if (elapsed >= nextStepToActivate * STEP_ON_INTERVAL_MS)
      {
        stepStates[nextStepToActivate].isActive = true;
        stepStates[nextStepToActivate].turnOnTime = currentMs;
        nextStepToActivate++;
      }
    }

    // Update all steps based on their state
    bool anyStepActive = false;
    for (int step = 0; step < STEP_COUNT; step++)
    {
      uint8_t brightness = calculateStepBrightness(step, currentMs);
      setStepColor(step, WARM_WHITE, brightness);

      if (brightness > 0)
      {
        anyStepActive = true;
      }
    }

    FastLED.show();

    // Check if animation is complete
    if (elapsed >= animationDuration || !anyStepActive)
    {
      animationComplete = true;
    }

    delay(20); // Update at ~50 FPS for smooth fading
  }

  // Ensure all LEDs are off
  FastLED.clear();
  FastLED.show();
}

// ============================================================================
// Setup and Main Loop
// ============================================================================

void setup()
{
  pinMode(PIR_PIN, INPUT);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(128); // 50% brightness

  FastLED.clear();
  FastLED.show();
}

void loop()
{
  if (digitalRead(PIR_PIN) == HIGH)
  {
    runCascadingAnimation();
  }
  delay(10);
}
