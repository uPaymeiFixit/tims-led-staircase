/**
 * LED Staircase - Step-by-Step Activation
 *
 * Motion-activated LED animation that:
 * 1. Detects motion via PIR sensor
 * 2. Turns on each step one at a time (0.5s delay between steps)
 * 3. Keeps all steps lit for 5 seconds
 * 4. Turns off all steps and waits for motion again
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

#define LED_PIN         25      // GPIO pin for LED data signal
#define PIR_PIN         4       // GPIO pin for PIR motion sensor
#define NUM_LEDS        980     // Total LEDs in the strip
#define LED_TYPE        WS2812B // WS2815 uses WS2812B protocol
#define COLOR_ORDER     GRB     // Color order for WS2815

// ============================================================================
// Animation Timing
// ============================================================================

#define STEP_DELAY_MS       500   // 0.5 second delay between steps
#define HOLD_DURATION_MS    5000  // 5 seconds hold time when fully lit

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
// Color Configuration
// ============================================================================

// Warm white color (similar to incandescent bulb, ~2700K)
const CRGB WARM_WHITE = CRGB(255, 180, 100);

// ============================================================================
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS];  // LED color buffer

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
int xyToIndex(int step, int pos) {
    // Bounds checking
    if (step < 0 || step >= STEP_COUNT || pos < 0 || pos >= MAX_LEDS_PER_STEP) {
        return NUM_LEDS;
    }

    // Steps 0-1 have 7 fewer LEDs on the left side
    if (step < 2 && pos < 7) {
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
void setLedSafe(int index, CRGB color) {
    if (index < NUM_LEDS) {
        leds[index] = color;
    }
}

// ============================================================================
// Animation Functions
// ============================================================================

/**
 * Light up a single step with warm white color.
 *
 * @param step  Step number (0-13)
 */
void lightStep(int step) {
    for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
        int index = xyToIndex(step, pos);
        setLedSafe(index, WARM_WHITE);
    }
    FastLED.show();
}

/**
 * Run the step-by-step activation sequence.
 *
 * Turns on each step one at a time from top to bottom,
 * holds for 5 seconds, then turns off all LEDs.
 */
void runStepSequence() {
    // Turn on each step one at a time
    for (int step = 0; step < STEP_COUNT; step++) {
        lightStep(step);
        delay(STEP_DELAY_MS);
    }

    // Hold all steps lit for 5 seconds
    delay(HOLD_DURATION_MS);

    // Turn off all LEDs
    FastLED.clear();
    FastLED.show();
}

// ============================================================================
// Setup and Main Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("LED Staircase Starting...");

    // Initialize PIR sensor pin
    pinMode(PIR_PIN, INPUT);

    // Initialize FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(128);  // 50% brightness

    // Start with all LEDs off
    FastLED.clear();
    FastLED.show();

    Serial.println("Ready. Waiting for motion...");
}

void loop() {
    // Check PIR sensor for motion
    if (digitalRead(PIR_PIN) == HIGH) {
        Serial.println("Motion detected! Lighting steps...");

        runStepSequence();

        Serial.println("Sequence complete. Waiting for motion...");
    }

    // Small delay to prevent CPU spinning
    delay(10);
}
