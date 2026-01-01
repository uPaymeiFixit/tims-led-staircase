/**
 * Tetris Animation for LED Staircase
 *
 * Simulates falling Tetris blocks on a 71x14 grid.
 * Blocks fall from the top, stack up, and reset when the stack reaches the top.
 * This is a purely decorative animation - no game logic or controls.
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
// Display Configuration
// ============================================================================

const int STEP_COUNT = 14;
const int MAX_LEDS_PER_STEP = 71;
const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

// ============================================================================
// Tetris Configuration
// ============================================================================

#define FALL_DELAY_MS   100   // Delay between fall steps (lower = faster)
#define DROP_DELAY_MS   16    // Delay for fast drop animation

// Tetromino shapes (4x4 grids stored as 16-bit bitmasks)
// Each shape is stored with its rotations
const uint16_t TETROMINOES[][4] = {
    // I piece
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    // O piece
    {0x6600, 0x6600, 0x6600, 0x6600},
    // T piece
    {0x0E40, 0x4C40, 0x4E00, 0x4640},
    // S piece
    {0x06C0, 0x8C40, 0x6C00, 0x4620},
    // Z piece
    {0x0C60, 0x4C80, 0xC600, 0x2640},
    // J piece
    {0x0E80, 0xC440, 0x2E00, 0x44C0},
    // L piece
    {0x0E20, 0x44C0, 0x8E00, 0xC440}
};

// Colors for each piece type
const CRGB PIECE_COLORS[] = {
    CRGB::Cyan,     // I
    CRGB::Yellow,   // O
    CRGB::Purple,   // T
    CRGB::Green,    // S
    CRGB::Red,      // Z
    CRGB::Blue,     // J
    CRGB::Orange    // L
};

const int NUM_PIECE_TYPES = 7;

// ============================================================================
// Global Variables
// ============================================================================

CRGB leds[NUM_LEDS];

// Game board (true = occupied)
bool board[STEP_COUNT][MAX_LEDS_PER_STEP];
CRGB boardColors[STEP_COUNT][MAX_LEDS_PER_STEP];

// Current falling piece
int currentPiece;
int currentRotation;
int pieceX;  // Left edge of 4x4 bounding box
int pieceY;  // Top edge of 4x4 bounding box (0 = top of screen)
CRGB pieceColor;

// ============================================================================
// Coordinate Mapping
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
// Tetromino Functions
// ============================================================================

// Check if a cell in the 4x4 tetromino grid is filled
bool getTetrominoCell(int piece, int rotation, int x, int y) {
    uint16_t shape = TETROMINOES[piece][rotation];
    int bit = y * 4 + x;
    return (shape >> (15 - bit)) & 1;
}

// Spawn a new random piece at the top
void spawnPiece() {
    currentPiece = random(NUM_PIECE_TYPES);
    currentRotation = random(4);
    pieceColor = PIECE_COLORS[currentPiece];
    pieceX = random(MAX_LEDS_PER_STEP - 4);
    pieceY = -4;  // Start above the visible area
}

// Check if piece can be placed at given position
bool canPlace(int px, int py) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (getTetrominoCell(currentPiece, currentRotation, x, y)) {
                int boardX = px + x;
                int boardY = py + y;

                // Check horizontal bounds
                if (boardX < 0 || boardX >= MAX_LEDS_PER_STEP) {
                    return false;
                }

                // Check if below the board
                if (boardY >= STEP_COUNT) {
                    return false;
                }

                // Check collision with existing pieces (only if on visible board)
                if (boardY >= 0 && board[boardY][boardX]) {
                    return false;
                }
            }
        }
    }
    return true;
}

// Lock the current piece into the board
void lockPiece() {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (getTetrominoCell(currentPiece, currentRotation, x, y)) {
                int boardX = pieceX + x;
                int boardY = pieceY + y;

                if (boardY >= 0 && boardY < STEP_COUNT &&
                    boardX >= 0 && boardX < MAX_LEDS_PER_STEP) {
                    board[boardY][boardX] = true;
                    boardColors[boardY][boardX] = pieceColor;
                }
            }
        }
    }
}

// Check if game should reset (piece locked above visible area)
bool shouldReset() {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (getTetrominoCell(currentPiece, currentRotation, x, y)) {
                int boardY = pieceY + y;
                if (boardY < 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Clear a full row and shift everything down
void clearFullRows() {
    for (int row = STEP_COUNT - 1; row >= 0; row--) {
        bool full = true;
        for (int col = 0; col < MAX_LEDS_PER_STEP; col++) {
            if (!board[row][col]) {
                full = false;
                break;
            }
        }

        if (full) {
            // Shift everything down
            for (int r = row; r > 0; r--) {
                for (int c = 0; c < MAX_LEDS_PER_STEP; c++) {
                    board[r][c] = board[r - 1][c];
                    boardColors[r][c] = boardColors[r - 1][c];
                }
            }
            // Clear top row
            for (int c = 0; c < MAX_LEDS_PER_STEP; c++) {
                board[0][c] = false;
                boardColors[0][c] = CRGB::Black;
            }
            // Check this row again
            row++;
        }
    }
}

// ============================================================================
// Display Functions
// ============================================================================

void clearBoard() {
    for (int y = 0; y < STEP_COUNT; y++) {
        for (int x = 0; x < MAX_LEDS_PER_STEP; x++) {
            board[y][x] = false;
            boardColors[y][x] = CRGB::Black;
        }
    }
}

void renderBoard() {
    // Clear LED buffer
    FastLED.clear();

    // Draw locked pieces
    for (int y = 0; y < STEP_COUNT; y++) {
        for (int x = 0; x < MAX_LEDS_PER_STEP; x++) {
            if (board[y][x]) {
                int idx = xyToIndex(y, x);
                if (idx < NUM_LEDS) {
                    leds[idx] = boardColors[y][x];
                }
            }
        }
    }

    // Draw current falling piece
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (getTetrominoCell(currentPiece, currentRotation, px, py)) {
                int boardX = pieceX + px;
                int boardY = pieceY + py;

                if (boardY >= 0 && boardY < STEP_COUNT &&
                    boardX >= 0 && boardX < MAX_LEDS_PER_STEP) {
                    int idx = xyToIndex(boardY, boardX);
                    if (idx < NUM_LEDS) {
                        leds[idx] = pieceColor;
                    }
                }
            }
        }
    }
}

// Reset animation - flash the board before clearing
void resetAnimation() {
    // Flash white
    for (int i = 0; i < 3; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
        delay(100);
        FastLED.clear();
        FastLED.show();
        delay(100);
    }

    // Clear from top to bottom
    for (int y = 0; y < STEP_COUNT; y++) {
        for (int x = 0; x < MAX_LEDS_PER_STEP; x++) {
            board[y][x] = false;
            boardColors[y][x] = CRGB::Black;
        }
        renderBoard();
        FastLED.show();
        delay(50);
    }
}

// ============================================================================
// Setup and Main Loop
// ============================================================================

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    FastLED.clear();
    FastLED.show();

    randomSeed(analogRead(0));
    clearBoard();
    spawnPiece();
}

void loop() {
    // Try to move piece down
    if (canPlace(pieceX, pieceY + 1)) {
        pieceY++;
    } else {
        // Lock the piece
        lockPiece();

        // Check if game over
        if (shouldReset()) {
            resetAnimation();
            clearBoard();
        } else {
            // Clear any full rows
            clearFullRows();
        }

        // Spawn new piece
        spawnPiece();
    }

    // Render and display
    renderBoard();
    FastLED.delay(FALL_DELAY_MS);
}
