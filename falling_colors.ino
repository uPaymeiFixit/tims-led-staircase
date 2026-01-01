/**
 * Falling Colors Animation
 *
 * Each step displays a bright, happy color. Every second, all colors
 * shift down one step and a new random color appears at the top.
 * The animation runs indefinitely without motion sensor input.
 *
 * Hardware:
 * - ESP32-WROOM-32
 * - WS2815 LED strip (980 LEDs across 14 steps)
 */

#include <FastLED.h>

// ============================================================================
// Hardware Configuration
// ============================================================================

#define LED_PIN         25
#define NUM_LEDS        980
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB

// ============================================================================
// Animation Timing
// ============================================================================

#define FALL_INTERVAL_MS  1000  // Time between color shifts (1 second)

// ============================================================================
// Staircase Layout Constants
// ============================================================================

const int STEP_COUNT = 14;
const int MAX_LEDS_PER_STEP = 71;

const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

// ============================================================================
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS];
CRGB stepColors[STEP_COUNT];  // Color for each step

// ============================================================================
// Coordinate Mapping Function
// ============================================================================

int xyToIndex(int step, int pos) {
    if (step < 0 || step >= STEP_COUNT || pos < 0 || pos >= MAX_LEDS_PER_STEP) {
        return NUM_LEDS;
    }

    if (step < 2 && pos < 7) {
        return NUM_LEDS;
    }

    int adjustedPos = (step < 2) ? pos - 7 : pos;
    int start = STEP_START[step];

    return (step & 1) ? start + (LEDS_PER_STEP[step] - 1 - adjustedPos) : start + adjustedPos;
}

// ============================================================================
// Color Functions
// ============================================================================

/**
 * Generate a bright, saturated "happy" color using HSV.
 * Full saturation and brightness with random hue.
 */
CRGB randomHappyColor() {
    return CHSV(random8(), 255, 255);
}

/**
 * Fill an entire step with a single color.
 */
void fillStep(int step, CRGB color) {
    for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
        int index = xyToIndex(step, pos);
        if (index < NUM_LEDS) {
            leds[index] = color;
        }
    }
}

/**
 * Shift all colors down one step and add a new color at the top.
 */
void shiftColorsDown() {
    // Shift colors down (bottom step color is lost)
    for (int step = STEP_COUNT - 1; step > 0; step--) {
        stepColors[step] = stepColors[step - 1];
    }

    // New random color at the top
    stepColors[0] = randomHappyColor();
}

/**
 * Apply stored colors to all steps.
 */
void updateLeds() {
    for (int step = 0; step < STEP_COUNT; step++) {
        fillStep(step, stepColors[step]);
    }
}

// ============================================================================
// Setup and Main Loop
// ============================================================================

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Initialize each step with a different happy color
    for (int step = 0; step < STEP_COUNT; step++) {
        stepColors[step] = randomHappyColor();
    }

    updateLeds();
    FastLED.show();
}

void loop() {
    shiftColorsDown();
    updateLeds();
    FastLED.delay(FALL_INTERVAL_MS);
}
