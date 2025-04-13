#include <Adafruit_NeoPixel.h>
#include <math.h> // For floor()
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp32-hal-timer.h>

#define PIN        15
#define NUMPIXELS  60
#define MAX_BRIGHTNESS 55 // Max brightness (0-255)
#define MAX_TIME_MINUTES 1
#define WIFI_SSID "Estate"
#define WIFI_PASSWORD "wireless"
#define SERVER_URL "http://192.168.1.15:4001"

const unsigned long MAX_TIME_SECONDS = MAX_TIME_MINUTES * 60;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

unsigned long startTimeMillis = 0;
unsigned long endTimeMillis = 0;

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

// Connect to WiFi
bool connectToWiFi() {
  Serial.println("\n=== WiFi Connection Attempt ===");
  Serial.println("Attempting to connect to WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi mode set to STA");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("WiFi.begin() called");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to WiFi! IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    return false;
  }
}

// Animate from off state to current progress over 3 seconds
void animateStartup(float targetProgress, uint8_t max_r, uint8_t max_g, uint8_t max_b) {
  const unsigned long ANIMATION_DURATION_MS = 1000;
  unsigned long animationStartTime = millis();
  
  while (millis() - animationStartTime < ANIMATION_DURATION_MS) {
    float animationProgress = (float)(millis() - animationStartTime) / ANIMATION_DURATION_MS;
    float currentProgress = animationProgress * targetProgress;
    
    // Calculate current color based on progress
    uint8_t current_r, current_g, current_b;
    getColorForProgress(currentProgress, current_r, current_g, current_b);
    
    // Scale colors by MAX_BRIGHTNESS
    float brightnessScale = (float)MAX_BRIGHTNESS / 255.0;
    current_r = (uint8_t)(current_r * brightnessScale);
    current_g = (uint8_t)(current_g * brightnessScale);
    current_b = (uint8_t)(current_b * brightnessScale);
    
    float threshold = currentProgress * NUMPIXELS;
    int lastFullPixel = floor(threshold - 0.0001);
    int fadingPixel = floor(threshold);
    
    for (int i = 0; i < NUMPIXELS; i++) {
      if (i <= lastFullPixel) {
        strip.setPixelColor(i, current_r, current_g, current_b);
      } else if (i == fadingPixel && fadingPixel < NUMPIXELS) {
        float fadeFraction = threshold - floor(threshold);
        uint8_t fade_r = (uint8_t)(current_r * fadeFraction);
        uint8_t fade_g = (uint8_t)(current_g * fadeFraction);
        uint8_t fade_b = (uint8_t)(current_b * fadeFraction);
        strip.setPixelColor(i, fade_r, fade_g, fade_b);
      } else {
        strip.setPixelColor(i, 0, 0, 0);
      }
    }
    
    strip.show();
    delay(10);
  }
}

// Check for next event and handle sleep/start countdown
void checkForNextEvent() {
  Serial.println("\n=== Checking for next event ===");
  
  if (!connectToWiFi()) {
    Serial.println("Failed to connect to WiFi. Retrying in 30 seconds...");
    delay(30000);
    return;
  }

  Serial.print("Making HTTP request to: ");
  Serial.println(SERVER_URL);
  
  HTTPClient http;
  Serial.println("HTTPClient created");
  
  http.begin(SERVER_URL);
  Serial.println("http.begin() completed");
  
  Serial.println("Sending GET request...");
  int httpCode = http.GET();
  Serial.println("GET request completed");

  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.print("Received payload: ");
    Serial.println(payload);
    
    int secondsUntilEvent = payload.toInt();
    int minutesUntilEvent = secondsUntilEvent / 60;

    Serial.print("Next event in ");
    Serial.print(minutesUntilEvent);
    Serial.println(" minutes");

    if (minutesUntilEvent > 30) {
      Serial.println("Going to deep sleep for 30 minutes before event");
      http.end();
      WiFi.disconnect();
      
      uint64_t sleepTime = (minutesUntilEvent - 30) * 60 * 1000000ULL;
      Serial.print("Sleep time in microseconds: ");
      Serial.println(sleepTime);
      
      esp_sleep_enable_timer_wakeup(sleepTime);
      Serial.println("Deep sleep enabled, going to sleep now...");
      esp_deep_sleep_start();
    } else {
      // Start countdown
      startTimeMillis = millis();
      endTimeMillis = startTimeMillis + secondsUntilEvent * 1000UL;
      Serial.print("Starting countdown for ");
      Serial.print(secondsUntilEvent);
      Serial.println(" seconds");
      
      // Calculate initial progress and colors for startup animation
      float initialProgress = 1.0 - ((float)0 / (float)(secondsUntilEvent * 1000));
      uint8_t base_r, base_g, base_b;
      getColorForProgress(initialProgress, base_r, base_g, base_b);
      
      // Scale colors by MAX_BRIGHTNESS
      float brightnessScale = (float)MAX_BRIGHTNESS / 255.0;
      uint8_t max_r = (uint8_t)(base_r * brightnessScale);
      uint8_t max_g = (uint8_t)(base_g * brightnessScale);
      uint8_t max_b = (uint8_t)(base_b * brightnessScale);
      
      // Perform startup animation
      animateStartup(initialProgress, max_r, max_g, max_b);
    }
  } else {
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpCode));
    Serial.println("Retrying in 30 seconds...");
    delay(30000);
  }
  
  http.end();
}

// --- Arduino Setup ---

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial connection
  Serial.println("\n=== Light Strip Initialization ===");
  Serial.println("Initializing NeoPixel strip...");

  strip.begin();
  Serial.println("strip.begin() completed");
  
  strip.clear(); // Initialize all pixels to 'off'
  Serial.println("strip.clear() completed");
  
  strip.show();
  Serial.println("strip.show() completed");
  Serial.println("NeoPixel strip initialized");

  Serial.println("About to check for next event...");
  checkForNextEvent();
  Serial.println("Returned from checkForNextEvent()");
}

// --- Arduino Loop ---

void loop() {
  unsigned long currentTimeMillis = millis();

  // --- Check for Timer Completion ---
  if (currentTimeMillis >= endTimeMillis) {
    Serial.println("\n=== Countdown Complete ===");
    strip.clear(); // Turn all pixels off
    strip.show();
    Serial.println("Checking for next event...");
    checkForNextEvent();
    return;
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