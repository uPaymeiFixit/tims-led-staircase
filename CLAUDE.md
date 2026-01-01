# LED Staircase Hardware Specification

Motion-activated LED lighting system for a 14-step staircase. LED strips mounted under each step illuminate when the PIR sensor detects movement.

## Microcontroller

- **Board:** ESP32-WROOM-32
- **LED Data Pin:** D25
- **PIR Sensor Pin:** D04

## LED Strip

- **Type:** WS2815 RGB (12V, single data line)
- **Total LEDs:** 980
- **Steps:** 14 (Step 1 = top, Step 14 = bottom)

## LED Layout Per Step

| Steps | LEDs per Step |
| ----- | ------------- |
| 1-2   | 64            |
| 3-14  | 71            |

## Signal Path

The LED data signal snakes through the steps in a zigzag pattern:

- **Odd steps (1, 3, 5...):** Signal enters from RIGHT
- **Even steps (2, 4, 6...):** Signal enters from LEFT

```
Step 1:  [RIGHT] >>>>>>>>>>>>>>>>>> [LEFT]
Step 2:  [RIGHT] <<<<<<<<<<<<<<<<<< [LEFT]
Step 3:  [RIGHT] >>>>>>>>>>>>>>>>>> [LEFT]
...continues alternating...
```

## LED Index Mapping

**Always use `xyToIndex()` to address LEDs.** This function handles the zigzag wiring so animation code can treat the staircase as a simple 2D grid.

```cpp
const int STEP_COUNT = 14;
const int LEDS_PER_STEP[] = {64, 64, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71};
const int STEP_START[] = {0, 64, 128, 199, 270, 341, 412, 483, 554, 625, 696, 767, 838, 909};

/**
 * Convert 2D coordinates (step, pos) to 1D LED index
 * @step: 0-13 (0 = top)
 * @pos: 0-70 (0 = left)
 * Returns NUM_LEDS for virtual positions (0-6 on steps 0-1)
*/
int xyToIndex(int step, int pos) {
    // Steps 0-1 have 7 fewer LEDs on the left side
    // Return NUM_LEDS (non-existent LED) for virtual positions
    if (step < 2 && pos < 7) {
        return NUM_LEDS;
    }
    int adjustedPos = (step < 2) ? pos - 7 : pos;
    int start = STEP_START[step];
    return (step & 1) ? start + (LEDS_PER_STEP[step] - 1 - adjustedPos) : start + adjustedPos;
}
```

## Step Index Reference Table

| Step | Start Index | End Index | Direction |
| ---- | ----------- | --------- | --------- |
| 1    | 0           | 63        | R to L    |
| 2    | 64          | 127       | L to R    |
| 3    | 128         | 198       | R to L    |
| 4    | 199         | 269       | L to R    |
| 5    | 270         | 340       | R to L    |
| 6    | 341         | 411       | L to R    |
| 7    | 412         | 482       | R to L    |
| 8    | 483         | 553       | L to R    |
| 9    | 554         | 624       | R to L    |
| 10   | 625         | 695       | L to R    |
| 11   | 696         | 766       | R to L    |
| 12   | 767         | 837       | L to R    |
| 13   | 838         | 908       | R to L    |
| 14   | 909         | 979       | L to R    |

## Library

- **FastLED** (WS2815 uses WS2812B protocol, single-wire 800kHz)

## Code Guidelines

- **No Serial debugging:** Do not include `Serial.begin()`, `Serial.print()`, or any Serial output unless explicitly requested. The serial port is not monitored.
- **Use FastLED.delay():** Use `FastLED.delay()` instead of `delay()` or `FastLED.show()` when we want to enable dithering for smoother color output. Continue using normal `delay()` when delaying when lights are not animating, or `FastLED.show()` when turning LEDs off.
- **Target framerate:** Aim for 60 frames per second (16ms delay between frames).

## Example Code

See [example.ino](example.ino) for a complete motion-activated animation demonstrating:

- Horizontal sweeps (left-to-right on, right-to-left off)
- Vertical sweeps (top-to-bottom on, bottom-to-top off)
- Proper use of `xyToIndex()` for coordinate mapping
- Safe LED addressing with `setLedSafe()` for virtual positions
