#include "FastLED.h"

// fast led constants
#define DATA_PIN    6        // change to your data pin
#define COLOR_ORDER GRB      // if colors are mismatched; change this
#define NUM_LEDS    (75*8)       // change to the number of LEDs in your strip

// change WS2812B to match your type of LED, if different
// list of supported types is here:
// https://github.com/FastLED/FastLED/wiki/Overview
#define LED_TYPE    WS2812B

// this creates an LED array to hold the values for each led in your strip
CRGB leds[NUM_LEDS];

void setup()
{
	// the wiki features a much more basic setup line:
	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
}

void loop() 
{

  for(int dot = 0; dot < NUM_LEDS; dot++)
  { 
    
    leds[dot] = CRGB::Blue;
    FastLED.show();
    
    // clear this led for the next time around the loop
    leds[dot] = CRGB::Black;
    
    delay(30);
  }

  for(int dot = 0; dot < NUM_LEDS; dot++)
  { 
    
    leds[NUM_LEDS - dot] = CRGB::Blue;
    FastLED.show();
    
    // clear this led for the next time around the loop
    leds[NUM_LEDS - dot] = CRGB::Black;
    
    delay(30);
  }
}
