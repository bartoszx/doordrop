#include <Adafruit_NeoPixel.h>
#include "led_control.h"

// Pin definitions
#define LED_PIN D5 // Pin for WS2812B
#define NUM_LEDS 5 // Number of LEDs in the strip

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void initLED() {
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'

    // Turn on LED to white (full brightness for R, G, and B)
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
    }
    strip.show();
    Serial.println("LED initialized to white");
}

void setLEDColor(const String& color) {
    Serial.print("Setting LED color to: ");
    Serial.println(color);
    uint32_t ledColor;

    if (color == "green") {
        ledColor = strip.Color(255, 0, 0);
    } else if (color == "red") {
        ledColor = strip.Color(0, 255, 0);
    } else {
        ledColor = strip.Color(255, 255, 255); // Default to white
    }

    // Reset all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(100); // Short delay to ensure reset

    // Set new color
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, ledColor);
        Serial.printf("LED %d set to color %d\n", i, ledColor);
    }
    strip.show();
    Serial.println("LED colors updated");
}
