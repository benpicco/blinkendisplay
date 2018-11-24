#include <FastLED.h>
#include <LEDMatrix.h>

// fast led constants
#define DATA_PIN    6        // change to your data pin
#define COLOR_ORDER GRB      // if colors are mismatched; change this

#define MATRIX_WIDTH   75
#define MATRIX_HEIGHT  8
#define MATRIX_TYPE    HORIZONTAL_ZIGZAG_MATRIX
#define MATRIX_SIZE    (MATRIX_WIDTH*MATRIX_HEIGHT)
#define NUM_LEDS       MATRIX_SIZE

// change WS2812B to match your type of LED, if different
// list of supported types is here:
// https://github.com/FastLED/FastLED/wiki/Overview
#define LED_TYPE    WS2812B

cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> leds;

void setup()
{
	// the wiki features a much more basic setup line:
	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds[0], leds.Size());
	FastLED.setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(5);
	FastLED.clear(true);
}

void loop() 
{
	for (int i = 0; i < leds.Height(); ++i) {
		FastLED.clear();
	        leds.DrawLine(0, i, leds.Width(), leds.Height() - i, CRGB::Blue);
		FastLED.show();
		delay(30);
	}
}
