/**
 * LED Staircase - Matrix Raindrop Effect
 *
 * Motion-activated LED animation that creates the iconic Matrix "digital rain"
 * effect with green cascading streams falling down the staircase. Multiple
 * raindrops fall simultaneously at varying speeds, with bright "heads" and
 * trailing tails that fade out.
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
// Animation Configuration
// ============================================================================

#define ANIMATION_DURATION_MS 20000 // 20 seconds total animation
#define NUM_RAINDROPS 15            // Number of simultaneous falling streams
#define FRAME_DELAY_MS 50           // Update at ~20 FPS
#define TAIL_LENGTH 7               // Length of fading trail behind each drop

// Matrix color palette (various shades of green)
const CRGB COLOR_HEAD = CRGB(200, 255, 200);   // Bright white-green head
const CRGB COLOR_BRIGHT = CRGB(0, 255, 0);     // Bright green
const CRGB COLOR_MEDIUM = CRGB(0, 200, 0);     // Medium green
const CRGB COLOR_DIM = CRGB(0, 120, 0);        // Dim green

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
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS]; // LED color buffer

// Raindrop state
struct Raindrop
{
  int column;           // Horizontal position (0-70)
  float step;           // Current vertical position (can be fractional)
  float speed;          // Steps per frame (0.2 - 0.8)
  bool active;          // Whether this drop is currently falling
  unsigned long respawnTime; // Time when this drop should respawn
};

Raindrop raindrops[NUM_RAINDROPS];

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
// Raindrop Functions
// ============================================================================

/**
 * Initialize a raindrop with random position and speed.
 *
 * @param drop       Pointer to raindrop to initialize
 * @param startNow   Whether to start falling immediately or wait
 */
void initRaindrop(Raindrop* drop, bool startNow)
{
  drop->column = random(0, MAX_LEDS_PER_STEP);
  drop->step = startNow ? 0 : -random(1, 10); // Start above screen if delayed
  drop->speed = random(20, 80) / 100.0;       // 0.2 to 0.8 steps per frame
  drop->active = startNow;
  drop->respawnTime = 0;
}

/**
 * Draw a single raindrop with its trailing tail.
 *
 * @param drop  Pointer to raindrop to draw
 */
void drawRaindrop(Raindrop* drop)
{
  if (!drop->active)
  {
    return;
  }

  int headStep = (int)drop->step;

  // Draw the bright head
  if (headStep >= 0 && headStep < STEP_COUNT)
  {
    int index = xyToIndex(headStep, drop->column);
    setLedSafe(index, COLOR_HEAD);
  }

  // Draw the fading tail
  for (int i = 1; i <= TAIL_LENGTH; i++)
  {
    int tailStep = headStep - i;
    if (tailStep >= 0 && tailStep < STEP_COUNT)
    {
      // Calculate fade based on distance from head
      uint8_t brightness = 255 - (i * 255 / (TAIL_LENGTH + 1));

      CRGB tailColor;
      if (i == 1)
      {
        tailColor = COLOR_BRIGHT;
      }
      else if (i <= 3)
      {
        tailColor = COLOR_MEDIUM;
      }
      else
      {
        tailColor = COLOR_DIM;
      }

      tailColor.nscale8(brightness);

      int index = xyToIndex(tailStep, drop->column);

      // Blend with existing color (in case multiple drops overlap)
      if (index < NUM_LEDS)
      {
        leds[index] += tailColor;
      }
    }
  }
}

/**
 * Update a raindrop's position and state.
 *
 * @param drop       Pointer to raindrop to update
 * @param currentMs  Current time in milliseconds
 */
void updateRaindrop(Raindrop* drop, unsigned long currentMs)
{
  if (!drop->active)
  {
    // Check if it's time to respawn
    if (currentMs >= drop->respawnTime)
    {
      initRaindrop(drop, true);
    }
    return;
  }

  // Move the raindrop down
  drop->step += drop->speed;

  // Check if raindrop has fallen off the bottom
  if (drop->step > STEP_COUNT + TAIL_LENGTH)
  {
    drop->active = false;
    // Respawn after a random delay (0-2 seconds)
    drop->respawnTime = currentMs + random(0, 2000);
  }
}

// ============================================================================
// Animation Functions
// ============================================================================

/**
 * Run the Matrix raindrop animation.
 */
void runMatrixAnimation()
{
  // Initialize all raindrops with staggered starts
  for (int i = 0; i < NUM_RAINDROPS; i++)
  {
    initRaindrop(&raindrops[i], random(0, 2) == 0); // 50% start immediately
  }

  unsigned long animationStart = millis();
  bool animationComplete = false;

  while (!animationComplete)
  {
    unsigned long currentMs = millis();
    unsigned long elapsed = currentMs - animationStart;

    // Clear the frame
    FastLED.clear();

    // Update and draw all raindrops
    for (int i = 0; i < NUM_RAINDROPS; i++)
    {
      updateRaindrop(&raindrops[i], currentMs);
      drawRaindrop(&raindrops[i]);
    }

    FastLED.show();

    // Check if animation duration has elapsed
    if (elapsed >= ANIMATION_DURATION_MS)
    {
      animationComplete = true;
    }

    delay(FRAME_DELAY_MS);
  }

  // Fade out effect - gradually dim all LEDs
  for (int brightness = 255; brightness >= 0; brightness -= 5)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i].nscale8(250); // Reduce by ~2%
    }
    FastLED.show();
    delay(20);
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

  // Seed random number generator with analog noise
  randomSeed(analogRead(0));
}

void loop()
{
  if (digitalRead(PIR_PIN) == HIGH)
  {
    runMatrixAnimation();
    delay(500); // Prevent immediate re-trigger
  }
  delay(10);
}
