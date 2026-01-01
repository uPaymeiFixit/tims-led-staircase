/**
 * LED Staircase Animation Example
 *
 * This example demonstrates a motion-activated LED animation sequence:
 * 1. LEDs turn on from left to right across all steps simultaneously
 * 2. LEDs turn off from right to left
 * 3. LEDs turn on from top step to bottom step (full rows)
 * 4. LEDs turn off from bottom step to top step
 *
 * Hardware:
 * - ESP32-WROOM-32
 * - WS2815 LED strip (980 LEDs across 14 steps)
 * - PIR motion sensor
 *
 * Wiring:
 * - LED Data: GPIO 25
 * - PIR Sensor: GPIO 4
 */

#include <FastLED.h>

// ============================================================================
// Hardware Configuration
// ============================================================================

#define LED_PIN         25      // GPIO pin for LED data signal
#define PIR_PIN         4       // GPIO pin for PIR motion sensor
#define NUM_LEDS        980     // Total LEDs in the strip
#define LED_TYPE        WS2812B // WS2815 uses WS2812B protocol
#define COLOR_ORDER     GRB     // Color order for WS2815

// ============================================================================
// Animation Timing (in milliseconds)
// ============================================================================

#define HORIZONTAL_DELAY_MS  28   // Delay between each LED column (left/right)
#define VERTICAL_DELAY_MS    143  // Delay between each step row (top/bottom)

// ============================================================================
// Staircase Layout Constants
// ============================================================================

const int STEP_COUNT = 14;
const int MAX_LEDS_PER_STEP = 71;  // Maximum LEDs on any step (steps 3-14)

// Number of LEDs on each step (steps 0-1 have 64, steps 2-13 have 71)
const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};

// Starting index in the LED array for each step
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

// ============================================================================
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS];              // LED color buffer
CRGB animationColor = CRGB::White; // Color used for animations

// ============================================================================
// Coordinate Mapping Function
// ============================================================================

/**
 * Convert 2D grid coordinates to 1D LED strip index.
 *
 * The LED strip snakes through the steps in a zigzag pattern:
 * - Odd steps (1, 3, 5...): Signal flows RIGHT to LEFT
 * - Even steps (2, 4, 6...): Signal flows LEFT to RIGHT
 *
 * This function abstracts away the wiring complexity, allowing animation
 * code to treat the staircase as a simple 2D grid where:
 * - X axis (pos): 0 = leftmost, 70 = rightmost
 * - Y axis (step): 0 = top step, 13 = bottom step
 *
 * @param step  Step number (0-13, where 0 is top step)
 * @param pos   Horizontal position (0-70, where 0 is leftmost)
 * @return      LED index in the strip, or NUM_LEDS for virtual positions
 *
 * Note: Steps 0-1 have only 64 LEDs (positions 7-70 are valid).
 *       Positions 0-6 on these steps return NUM_LEDS (a safe "null" index).
 */
int xyToIndex(int step, int pos) {
    // Bounds checking
    if (step < 0 || step >= STEP_COUNT || pos < 0 || pos >= MAX_LEDS_PER_STEP) {
        return NUM_LEDS;
    }

    // Steps 0-1 have 7 fewer LEDs on the left side
    // Return NUM_LEDS (non-existent LED) for virtual positions
    if (step < 2 && pos < 7) {
        return NUM_LEDS;
    }

    // Adjust position for shorter steps
    int adjustedPos = (step < 2) ? pos - 7 : pos;
    int start = STEP_START[step];

    // Handle zigzag wiring: even steps go left-to-right, odd steps go right-to-left
    return (step & 1) ? start + (LEDS_PER_STEP[step] - 1 - adjustedPos) : start + adjustedPos;
}


// ============================================================================
// Animation Functions
// ============================================================================

/**
 * Turn on all LEDs column by column from left to right.
 *
 * Each column spans all 14 steps simultaneously. The animation takes
 * approximately 71 * 28ms = ~2 seconds to complete.
 */
void sweepLeftToRight() {
    for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
        // Light up this column on all steps
        for (int step = 0; step < STEP_COUNT; step++) {
            int index = xyToIndex(step, pos);
            leds[index] = animationColor;
        }
        FastLED.delay(HORIZONTAL_DELAY_MS);
    }
}

/**
 * Turn off all LEDs column by column from right to left.
 *
 * Reverses the left-to-right sweep, turning off LEDs starting from
 * the rightmost column.
 */
void sweepRightToLeft() {
    for (int pos = MAX_LEDS_PER_STEP - 1; pos >= 0; pos--) {
        // Turn off this column on all steps
        for (int step = 0; step < STEP_COUNT; step++) {
            int index = xyToIndex(step, pos);
            leds[index] = CRGB::Black;
        }
        FastLED.show();
        delay(HORIZONTAL_DELAY_MS);
    }
}

/**
 * Turn on all LEDs row by row from top to bottom.
 *
 * Each step (row) lights up entirely before moving to the next.
 * The animation takes approximately 14 * 143ms = ~2 seconds.
 */
void sweepTopToBottom() {
    for (int step = 0; step < STEP_COUNT; step++) {
        // Light up entire step
        for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
            int index = xyToIndex(step, pos);
            leds[index] = animationColor;
        }
        FastLED.delay(VERTICAL_DELAY_MS);
    }
}

/**
 * Turn off all LEDs row by row from bottom to top.
 *
 * Reverses the top-to-bottom sweep, turning off entire steps starting
 * from the bottom.
 */
void sweepBottomToTop() {
    for (int step = STEP_COUNT - 1; step >= 0; step--) {
        // Turn off entire step
        for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
            int index = xyToIndex(step, pos);
            leds[index] = CRGB::Black;
        }
        FastLED.show();
        delay(VERTICAL_DELAY_MS);
    }
}

/**
 * Run the complete animation sequence.
 *
 * Sequence:
 * 1. Horizontal sweep on (left to right)
 * 2. Horizontal sweep off (right to left)
 * 3. Vertical sweep on (top to bottom)
 * 4. Vertical sweep off (bottom to top)
 */
void runAnimationSequence() {
    // Horizontal animation
    sweepLeftToRight();
    sweepRightToLeft();

    // Vertical animation
    sweepTopToBottom();
    sweepBottomToTop();
}

// ============================================================================
// Setup and Main Loop
// ============================================================================

void setup() {
    // Initialize PIR sensor pin
    pinMode(PIR_PIN, INPUT);

    // Initialize FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Start with all LEDs off
    FastLED.clear();
    FastLED.show();
}

void loop() {
    // Check PIR sensor for motion
    if (digitalRead(PIR_PIN) == HIGH) {
        runAnimationSequence();

        // Small delay to prevent immediate re-trigger
        delay(500);
    }

    // Small delay to prevent CPU spinning
    delay(10);
}
