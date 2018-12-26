BOARD := nodemcuv2
UPLOAD_SPEED := 115200

CUSTOM_LIBS := ../libraries

ARDUINO_LIBS += SPI
ARDUINO_LIBS += FastLED
ARDUINO_LIBS += FastLED_NeoMatrix
ARDUINO_LIBS += Adafruit-GFX

BUILD_EXTRA_FLAGS += -DFASTLED_ESP8266_DMA -Os

BUILD_ROOT := build
ESP_ROOT := ../esp-toolchain
include ../makeEspArduino/makeEspArduino.mk
