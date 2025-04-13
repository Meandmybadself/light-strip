#include <Adafruit_NeoPixel.h>

// Which pin on the ESP32 Feather V2 is connected to the NeoPixels?
#define PIN        13  // Using pin 13 instead of GPIO 5

// How many NeoPixels are attached?
#define NUMPIXELS 120 // 120 LEDs in your strip

// Define the NeoPixel strip object:
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Initialize the NeoPixel strip
  pixels.begin();
  pixels.setBrightness(50);  // Start with 50% brightness (0-255)
  pixels.show();             // Initialize all pixels to 'off'
  
  // Optional: Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("ESP32 Feather V2 NeoPixel Controller Ready");
}

void loop() {
  // Example pattern: Color wipe
  colorWipe(pixels.Color(255, 0, 0), 50); // Red
  colorWipe(pixels.Color(0, 255, 0), 50); // Green
  colorWipe(pixels.Color(0, 0, 255), 50); // Blue
  
  // Example pattern: Theater chase
  theaterChase(pixels.Color(127, 0, 0), 50); // Red
  theaterChase(pixels.Color(0, 127, 0), 50); // Green
  theaterChase(pixels.Color(0, 0, 127), 50); // Blue
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, color);
    pixels.show();
    delay(wait);
  }
}

// Theater-style crawling lights
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times
    for(int b=0; b<3; b++) { // The 'b' loop
      pixels.clear();
      // Turn every third pixel on
      for(int i=b; i<pixels.numPixels(); i += 3) {
        pixels.setPixelColor(i, color);
      }
      pixels.show();
      delay(wait);
    }
  }
}