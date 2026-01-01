#include <FastLED.h>

#define NUM_LEDS 980
#define DATA_PIN 25
#define PIR_PIN 4

CRGB leds[NUM_LEDS];

const int STEP_COUNT = 14;
const int MAX_LEDS_PER_STEP = 71;
const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

// Game of Life grid (current and next generation)
bool currentGen[STEP_COUNT][MAX_LEDS_PER_STEP];
bool nextGen[STEP_COUNT][MAX_LEDS_PER_STEP];

// Color palette for live cells
CRGB aliveColor = CRGB::Cyan;
CRGB deadColor = CRGB::Black;

// Animation settings
const int GENERATION_DELAY = 100; // ms between generations

// Motion detection
bool motionDetected = false;

/**
 * Convert 2D coordinates (step, pos) to 1D LED index
 */
int xyToIndex(int step, int pos) {
    if (step < 2 && pos < 7) {
        return NUM_LEDS;
    }
    int adjustedPos = (step < 2) ? pos - 7 : pos;
    int start = STEP_START[step];
    return (step & 1) ? start + (LEDS_PER_STEP[step] - 1 - adjustedPos) : start + adjustedPos;
}

/**
 * Safely set an LED color, handling virtual positions
 */
void setLedSafe(int step, int pos, CRGB color) {
    int index = xyToIndex(step, pos);
    if (index < NUM_LEDS) {
        leds[index] = color;
    }
}

/**
 * Check if a cell position is valid
 */
bool isValidCell(int step, int pos) {
    if (step < 0 || step >= STEP_COUNT) return false;
    if (pos < 0 || pos >= MAX_LEDS_PER_STEP) return false;
    // Handle virtual positions on steps 0-1
    if (step < 2 && pos < 7) return false;
    return true;
}

/**
 * Count living neighbors for a cell
 */
int countNeighbors(int step, int pos) {
    int count = 0;

    // Check all 8 neighbors
    for (int ds = -1; ds <= 1; ds++) {
        for (int dp = -1; dp <= 1; dp++) {
            // Skip the cell itself
            if (ds == 0 && dp == 0) continue;

            int newStep = step + ds;
            int newPos = pos + dp;

            if (isValidCell(newStep, newPos) && currentGen[newStep][newPos]) {
                count++;
            }
        }
    }

    return count;
}

/**
 * Initialize grid with random pattern
 */
void initializeGrid() {
    // Clear grid
    for (int step = 0; step < STEP_COUNT; step++) {
        for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
            currentGen[step][pos] = false;
            nextGen[step][pos] = false;
        }
    }

    // Seed with random cells (30% density)
    random16_set_seed(millis());
    for (int step = 0; step < STEP_COUNT; step++) {
        int maxPos = (step < 2) ? 64 : 71;
        int startPos = (step < 2) ? 7 : 0;

        for (int pos = startPos; pos < maxPos; pos++) {
            if (random8(100) < 30) {
                currentGen[step][pos] = true;
            }
        }
    }

}

/**
 * Compute next generation using Conway's rules
 */
void computeNextGeneration() {
    for (int step = 0; step < STEP_COUNT; step++) {
        int maxPos = (step < 2) ? 64 : 71;
        int startPos = (step < 2) ? 7 : 0;

        for (int pos = startPos; pos < maxPos; pos++) {
            int neighbors = countNeighbors(step, pos);
            bool isAlive = currentGen[step][pos];

            // Conway's Game of Life rules:
            // 1. Any live cell with 2-3 neighbors survives
            // 2. Any dead cell with exactly 3 neighbors becomes alive
            // 3. All other cells die or stay dead

            if (isAlive) {
                nextGen[step][pos] = (neighbors == 2 || neighbors == 3);
            } else {
                nextGen[step][pos] = (neighbors == 3);
            }
        }
    }

    // Copy next generation to current
    for (int step = 0; step < STEP_COUNT; step++) {
        for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
            currentGen[step][pos] = nextGen[step][pos];
        }
    }

}

/**
 * Update LED display from current generation
 */
void updateDisplay() {
    for (int step = 0; step < STEP_COUNT; step++) {
        int maxPos = (step < 2) ? 64 : 71;
        int startPos = (step < 2) ? 7 : 0;

        for (int pos = startPos; pos < maxPos; pos++) {
            CRGB color = currentGen[step][pos] ? aliveColor : deadColor;
            setLedSafe(step, pos, color);
        }
    }
}

/**
 * Check if the grid is empty or stable
 */
bool isGridEmpty() {
    for (int step = 0; step < STEP_COUNT; step++) {
        for (int pos = 0; pos < MAX_LEDS_PER_STEP; pos++) {
            if (currentGen[step][pos]) return false;
        }
    }
    return true;
}

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);

    pinMode(PIR_PIN, INPUT);

    // Clear display
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // Initialize random seed
    random16_set_seed(analogRead(0));
}

void loop() {
    // Check for motion
    if (digitalRead(PIR_PIN) == HIGH) {
        if (!motionDetected) {
            motionDetected = true;
            initializeGrid();
        }
    }

    // Run animation if motion detected
    if (motionDetected) {
        // Reset if grid is empty
        if (isGridEmpty()) {
            initializeGrid();
        }

        // Update display
        updateDisplay();
        FastLED.delay(GENERATION_DELAY);

        // Compute next generation
        computeNextGeneration();

    } else {
        // No motion - wait
        delay(100);
    }
}
