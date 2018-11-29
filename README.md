This is a simple project for driving a strip of serial LEDs with an esp8826 using the Adafruit_GFX library.

# Modules used:
 - [FastLED](https://github.com/coryking/FastLED) fork with DMA for esp8266 to drive the LEDs
 - [FastLED_NeoMatrix](https://github.com/marcmerlin/FastLED_NeoMatrix) to map the LEDs onto a grid
 - [Adafruit-GFX](https://github.com/adafruit/Adafruit-GFX-Library) for graphic operations on the grid (draw shapes, text, etc)

# Building
 - run `./prepare.sh` to set up submodules and toolchain (only needed once)
 - run `make upload` to build the project and flash the esp8826
 
 This project will simply connect to the WiFi network specified in [main.ino](https://github.com/benpicco/blinkendisplay/blob/master/src/main.ino#L20) and display the IP address it got once it's connected.
