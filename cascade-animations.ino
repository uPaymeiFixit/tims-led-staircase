/**
 * This is meant to be the final code for Tim's steps. It uses two ultrasonic
 * sensors, and when one is triggered each step will begin glowing one after
 * another. It handles a stack of animations.
 */

#include <EEPROM.h>
#include <FastLED.h>
FASTLED_USING_NAMESPACE

// User changable constant variables
#define FRAMES_PER_SECOND 60         // Maximum framerate
#define MAX_SONAR_DISTANCE 35        // Don't detect past this (inches)
#define NUM_STEPS 14                 // Number of steps
#define LEDS_PER_STEP 3              // Number of LEDs on each step
#define BRIGHTNESS 255               // Default brightness of the LED strip
#define WAIT_BETWEEN_TRIGGERS 3000   // Rate limiting between triggers (ms)
#define ANIMATION_STACK_SIZE_MAX 10  // Maximum simultaneous animations (lowers fps)
#define CHRISTMAS_OFF
#define RAINBOW_OFF
#define DEBUG_OFF

// Hardware configuration constant variables
#define BUTTON_MEMORY_ADDRESS 0
#define SONAR_PING_PIN_TOP 7
#define SONAR_ECHO_PIN_TOP 6
#define SONAR_PING_PIN_BOTTOM 4
#define SONAR_ECHO_PIN_BOTTOM 5
#define LED_DATA_PIN 3
#define LED_CHIP WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_STEPS * LEDS_PER_STEP];  // Define leds array

// Run this function once when the Arduino starts
void setup() {

#ifdef DEBUG           //************************* START DEBUG ***************************
  Serial.begin(9600);  // Open a serial connection (for debugging)
  Serial.println("DEBUGGING ENABLED");
#endif  //***************************** END DEBUG *******************************

  // Tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_CHIP, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_STEPS * LEDS_PER_STEP).setCorrection(TypicalLEDStrip);

  // Set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // Configure the ultrasonic sensor pins
  pinMode(SONAR_PING_PIN_TOP, OUTPUT);
  pinMode(SONAR_PING_PIN_BOTTOM, OUTPUT);
  pinMode(SONAR_ECHO_PIN_TOP, INPUT);
  pinMode(SONAR_ECHO_PIN_BOTTOM, INPUT);
}

// Run this function on a loop after setup runs
void loop() {
  sensorLoop();
  animationLoop();
  displayLeds();
  frameLimiter();
}

// Limit framerate to FRAMES_PER_SECOND
unsigned long frame_start_time;
const unsigned int FRAME_DURATION_MAX = 1000 / FRAMES_PER_SECOND;
void frameLimiter() {
  const unsigned long frame_duration = millis() - frame_start_time;
  if (frame_duration < FRAME_DURATION_MAX) {
    const unsigned long reamining_duration = FRAME_DURATION_MAX - frame_duration;
    FastLED.delay(reamining_duration);
  }
#ifdef DEBUG  //************************* START DEBUG ***************************
  else {
    Serial.print("FRAMERATE WARNING: ");
    Serial.println(1000 / frame_duration);
  }
#endif  //***************************** END DEBUG *******************************
  frame_start_time = millis();
}

// Enum to make starting point more clear
enum starting_point {
  TOP,
  BOTTOM
};

// TODO: make an array whose elements look like this:
// [&animation, [start_time, starting_point, vleds]]
// Create a hacky call stack for the animation function
unsigned long animation_stack_start_times[ANIMATION_STACK_SIZE_MAX];
bool animation_stack_starting_point[ANIMATION_STACK_SIZE_MAX];
CRGB animation_stack_steps[ANIMATION_STACK_SIZE_MAX][NUM_STEPS];
byte animation_stack_size;

unsigned long last_trigger_time_top;
unsigned long last_trigger_time_bottom;
// Check sensors, trigger if necessary, and add animations to the stack
void sensorLoop() {
  // Only check distance if our animation stack is full
  if (animation_stack_size < ANIMATION_STACK_SIZE_MAX) {
    // Get distance from the sensors
    byte distance_top = getDistance(SONAR_PING_PIN_TOP, SONAR_ECHO_PIN_TOP);
    byte distance_bottom = getDistance(SONAR_PING_PIN_BOTTOM, SONAR_ECHO_PIN_BOTTOM);

    // Check if distance_top is within triggering range & we're not being rate limited
    if (distance_top < MAX_SONAR_DISTANCE && millis() - last_trigger_time_top > WAIT_BETWEEN_TRIGGERS) {
      last_trigger_time_top = millis();  // Record when we triggered
      pushStack(TOP);                    // Push a new TOP animation onto the stack
    }
    // Check if distance_top is within triggering range & we're not being rate limited
    if (distance_bottom < MAX_SONAR_DISTANCE && millis() - last_trigger_time_bottom > WAIT_BETWEEN_TRIGGERS) {
      last_trigger_time_bottom = millis();  // Record when we triggered
      pushStack(BOTTOM);                    // Push a new BOTTOM animation onto the stack
    }
  }
}

// Calculate the maximum time we need to let the pulseIn function run
// No, I don't know where this math comes from. I tested the maximum distance
// at a bunch of different timeouts and made a graph from that.
const unsigned int SONAR_TIMEOUT = 74 * 2 * MAX_SONAR_DISTANCE + 3200;
// Returns the distance in inches from a sensor on the provided pins
byte getDistance(byte sonar_ping_pin, byte sonar_echo_pin) {
  // Set the ultrasonic sensor ping low for 2µs
  digitalWrite(sonar_ping_pin, LOW);
  delayMicroseconds(2);

  // Tell the ultrasonic to emit a 10µs burst
  digitalWrite(sonar_ping_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sonar_ping_pin, LOW);

  // Read the time it takes for teh sensor to receive the sound back in µs
  unsigned long duration = pulseIn(sonar_echo_pin, HIGH, SONAR_TIMEOUT);
  byte distance = duration / 74 / 2;  // Convert microseconds response to inches

  // A reading of 0 means the sensor timed out, which is usually infinite
  // distance (beyond the sensor's capabilities) since a reading of 0 inches
  // cannot often be achieved.
  distance = distance == 0 ? MAX_SONAR_DISTANCE : distance;
  // Make sure distance does not exceed MAX_SONAR_DISTANCE
  distance = distance > MAX_SONAR_DISTANCE ? MAX_SONAR_DISTANCE : distance;

  return distance;
}

// Calculate how long the animation runs before removing it from the stack
const unsigned int ANIMATION_RUNTIME = 20000 + NUM_STEPS * 1000l;
// Maintain animations on the stack
void animationLoop() {
  // If there is at least one animation on the stack, check to make sure it's not expired
  while (animation_stack_size > 0 && millis() - animation_stack_start_times[0] > ANIMATION_RUNTIME) {
    popStack();  // If it's expired, remove it from the stack
  }

  // Loop through the hacky animation call stack
  for (int i = 0; i < animation_stack_size; i++) {
    // Call the stack item
    animate(animation_stack_start_times[i], animation_stack_starting_point[i], animation_stack_steps[i]);
  }
}

// Animate a single frame of the LEDs
void animate(unsigned long start_time, bool starting_point, CRGB virtual_steps[]) {
  // Calculate how long this animation has been running (t)
  unsigned long duration = millis() - start_time;
  // Loop through LEDs
  for (int i = 0; i < NUM_STEPS; i++) {
    // Calculate brightness according to the following graph:
    // https://www.desmos.com/calculator/utjgmvixct
    unsigned long brightness = 255 * pow(1.5, -pow((duration - i * 1000) / 7400. - 1.35, 10));
    // Set LED brightness starting from top or bottom of steps
    const int led_index = starting_point ? i : NUM_STEPS - i;
#ifdef CHRISTMAS
    virtual_steps[led_index].r = i % 2 ? brightness : 0;
    virtual_steps[led_index].g = i % 2 ? 0 : brightness;
    virtual_steps[led_index].b = 0;
#elif defined(RAINBOW)
    virtual_steps[led_index] = CHSV(360 / (NUM_STEPS - 1) * led_index, 255, brightness);
#else
    virtual_steps[led_index].r = brightness;
    virtual_steps[led_index].g = brightness;
    virtual_steps[led_index].b = brightness;
#endif
  }
}

// Push an item onto the back of the animation stack
void pushStack(bool starting_point) {
  // Add animation variables to the stack
  animation_stack_start_times[animation_stack_size] = millis();
  animation_stack_starting_point[animation_stack_size] = starting_point;
  animation_stack_size++;

#ifdef DEBUG  //************************* START DEBUG ***************************
  Serial.print("ANIMATION TRIGGERED: ");
  Serial.print(starting_point ? "TOP @ " : "BOTTOM @ ");
  Serial.print(animation_stack_start_times[animation_stack_size - 1]);
  Serial.print("ms\nANIMATION_STACK_SIZE: ");
  Serial.println(animation_stack_size);
  Serial.println();
#endif  //***************************** END DEBUG *******************************
}

// Pop an item off the front of the animation stack
void popStack() {
  for (int i = 0; i < animation_stack_size - 1; i++) {
    animation_stack_start_times[i] = animation_stack_start_times[i + 1];
    animation_stack_starting_point[i] = animation_stack_starting_point[i + 1];
    *animation_stack_steps[i] = &animation_stack_steps[i + 1];
  }
  animation_stack_size--;

#ifdef DEBUG  //************************* START DEBUG ***************************
  Serial.print("ANIMATION COMPLETED: ");
  Serial.print(animation_stack_starting_point[animation_stack_size] ? "TOP @ " : "BOTTOM @ ");
  Serial.print(animation_stack_start_times[animation_stack_size]);
  Serial.print("ms\nANIMATION_STACK_SIZE: ");
  Serial.println(animation_stack_size);
  Serial.println();
#endif  //***************************** END DEBUG *******************************
}

// Combines virtual LEDs arrays from the stack into a real LED strip
void displayLeds() {
  // Itterate through each step
  for (int step = 0; step < NUM_STEPS; step++) {
    // Itterate through each LED on step
    for (int led_on_step = 0; led_on_step < LEDS_PER_STEP; led_on_step++) {
      // Start LED at black
      const unsigned int i = step * LEDS_PER_STEP + led_on_step;
      leds[i] = CRGB(0, 0, 0);
      // Sum up brightness in leds stack onto the real leds
      for (int animation = 0; animation < animation_stack_size; animation++) {
        const unsigned int sum_r = leds[i].r + animation_stack_steps[animation][step].r;
        leds[i].r = sum_r > 255 ? 255 : sum_r;
        const unsigned int sum_g = leds[i].g + animation_stack_steps[animation][step].g;
        leds[i].g = sum_g > 255 ? 255 : sum_g;
        const unsigned int sum_b = leds[i].b + animation_stack_steps[animation][step].b;
        leds[i].b = sum_b > 255 ? 255 : sum_b;
      }
    }
  }
  // Tell FastLED library to send data to the LED strip
  FastLED.show();
}
