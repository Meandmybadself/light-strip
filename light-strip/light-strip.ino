#include <Adafruit_NeoPixel.h>
#include <math.h> // For floor()

#define PIN        15
#define NUMPIXELS  60
#define MAX_BRIGHTNESS 100 // Max brightness (0-255)
#define MAX_TIME_MINUTES 1

const unsigned long MAX_TIME_SECONDS = MAX_TIME_MINUTES * 60;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

unsigned long startTimeMillis = 0;
unsigned long endTimeMillis = 0;
bool timerRunning = false;

// --- Helper Functions ---

// Linear interpolation
float custom_lerp(float start, float end, float t) {
  // Clamp t to [0, 1]
  t = (t < 0.0) ? 0.0 : ((t > 1.0) ? 1.0 : t);
  return start + t * (end - start);
}

// Calculate R, G, B color based on progress (0.0 to 1.0)
// Progress 1.0 = Green, 0.5 = Orange, 0.0 = Red
void getColorForProgress(float progress, uint8_t& r, uint8_t& g, uint8_t& b) {
  // Clamp progress to [0, 1]
  progress = (progress < 0.0) ? 0.0 : ((progress > 1.0) ? 1.0 : progress);

  // Define key colors
  const uint8_t R_RED = 255, G_RED = 0,   B_RED = 0;
  const uint8_t R_ORG = 255, G_ORG = 165, B_ORG = 0;
  const uint8_t R_GRN = 0,   G_GRN = 255, B_GRN = 0;

  if (progress > 0.5) {
    // Interpolate between Orange and Green
    float t = (progress - 0.5) * 2.0;
    r = (uint8_t)custom_lerp(R_ORG, R_GRN, t);
    g = (uint8_t)custom_lerp(G_ORG, G_GRN, t);
    b = (uint8_t)custom_lerp(B_ORG, B_GRN, t);
  } else {
    // Interpolate between Red and Orange
    float t = progress * 2.0;
    r = (uint8_t)custom_lerp(R_RED, R_ORG, t);
    g = (uint8_t)custom_lerp(G_RED, G_ORG, t);
    b = (uint8_t)custom_lerp(B_RED, B_ORG, t);
  }
}

// --- Arduino Setup ---

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial connection
  Serial.println("Initializing Countdown Timer...");

  strip.begin();
  strip.clear(); // Initialize all pixels to 'off'
  strip.show();

  // Start the timer immediately for testing
  startTimeMillis = millis();
  endTimeMillis = startTimeMillis + MAX_TIME_SECONDS * 1000UL; // Use UL for unsigned long
  timerRunning = true;
  Serial.print("Timer started. Ends in: ");
  Serial.print(MAX_TIME_SECONDS);
  Serial.println(" seconds.");
}

// --- Arduino Loop ---

void loop() {
  if (!timerRunning) {
    // Do nothing if timer is not running
    // (Future: check for HTTP request here)
    delay(100); // Prevent busy-waiting
    return;
  }

  unsigned long currentTimeMillis = millis();

  // --- Check for Timer Completion ---
  if (currentTimeMillis >= endTimeMillis) {
    Serial.println("Timer finished!");
    strip.clear(); // Turn all pixels off
    strip.show();
    timerRunning = false;
    return; // Exit loop iteration
  }

  // --- Calculate Progress ---
  unsigned long elapsedMillis = currentTimeMillis - startTimeMillis;
  unsigned long totalDurationMillis = endTimeMillis - startTimeMillis;
  float progress = 1.0 - ((float)elapsedMillis / (float)totalDurationMillis);
  // Clamp progress just in case of floating point inaccuracies
  progress = (progress < 0.0) ? 0.0 : ((progress > 1.0) ? 1.0 : progress);

  // --- Determine Base Color ---
  uint8_t base_r, base_g, base_b;
  getColorForProgress(progress, base_r, base_g, base_b);

  // Scale base color by MAX_BRIGHTNESS
  float brightnessScale = (float)MAX_BRIGHTNESS / 255.0;
  uint8_t max_r = (uint8_t)(base_r * brightnessScale);
  uint8_t max_g = (uint8_t)(base_g * brightnessScale);
  uint8_t max_b = (uint8_t)(base_b * brightnessScale);


  // --- Update LEDs ---
  float threshold = progress * NUMPIXELS; // The point separating on/off pixels
  int lastFullPixel = floor(threshold - 0.0001); // Index of the last pixel fully on
  int fadingPixel = floor(threshold); // Index of the pixel currently fading

  for (int i = 0; i < NUMPIXELS; i++) {
    if (i <= lastFullPixel) {
       // Pixels fully on
       strip.setPixelColor(i, max_r, max_g, max_b);
    } else if (i == fadingPixel && fadingPixel < NUMPIXELS) {
        // The pixel that is currently fading
        float fadeFraction = threshold - floor(threshold); // How much "on" this pixel is (0.0 to 1.0)
        uint8_t fade_r = (uint8_t)(max_r * fadeFraction);
        uint8_t fade_g = (uint8_t)(max_g * fadeFraction);
        uint8_t fade_b = (uint8_t)(max_b * fadeFraction);
        strip.setPixelColor(i, fade_r, fade_g, fade_b);
    } else {
        // Pixels that are off
        strip.setPixelColor(i, 0, 0, 0);
    }
  }

  strip.show(); // Update the strip

  // Optional: Add a small delay to avoid overwhelming the ESP32,
  // although show() has some built-in delay. Adjust as needed.
  delay(10);
}