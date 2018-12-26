#define FASTLED_ESP8266_DMA

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*a))

// fast led constants
#define DATA_PIN	3	// labeled as RX on nodeMCU
#define COLOR_ORDER	GRB	// if colors are mismatched; change this

#define M_WIDTH 	75	// the width of the LED matrix
#define M_HEIGHT	8	// the height of the LED matrix
#define NUM_LEDS	(M_WIDTH*M_HEIGHT)

#define STRLEN_MAX 128

// change WS2812B to match your type of LED, if different
// list of supported types is here:
// https://github.com/FastLED/FastLED/wiki/Overview
#define LED_TYPE    WS2812B

static CRGB leds[NUM_LEDS];
static FastLED_NeoMatrix matrix = FastLED_NeoMatrix(leds, M_WIDTH, M_HEIGHT, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);

static WiFiUDP Udp;

static char strbuffer[2][STRLEN_MAX];
static int active_buffer;
static bool string_pending;

void setup()
{
	Serial.begin(115200);
	Serial.println("Hello World!");

//	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
//	FastLED.setCorrection(TypicalLEDStrip);
//	FastLED.setBrightness(5);	// low brightness so we can test the strip just using USB
//	FastLED.clear(true);

	matrix.begin();

	WiFi.begin("35C3-insecure");
	Udp.begin(1337);
}

static bool is_sane_string(char* s, size_t len) {
	for (; *s && len--; ++s) {
		if (*s == '\n' || *s == '\r')
			*s = ' ';

		if (*s < ' ' || *s > '~')
			return false;
	}

	return true;
}

static void send_reply(const char* msg) {
	Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
	Udp.write(msg);
	Udp.endPacket();
}

static void rx_string(char* dst, size_t n) {
	int len = Udp.parsePacket();

	if (len <= 0 || len > n)
		return;

	len = Udp.read(dst, n);

	if (len <= 0)
		return;
	dst[len] = 0;

	if (!is_sane_string(dst, len)) {
		send_reply("Invalid String\n");
		return;
	}

	printf("Got String: '%s'\n", dst);
	send_reply("OK\n");

	string_pending = true;
}

static void draw_string(void) {
	const uint16_t colors[] = {
		matrix.Color(255, 0, 0),
		matrix.Color(0, 255, 0),
		matrix.Color(0, 0, 255)};

	static int pass, bounds;
	static int x;

	matrix.clear();
	matrix.setCursor(x, 0);
	matrix.print(strbuffer[active_buffer]);

	if (++x > matrix.width()) {

		if (string_pending) {
			string_pending = false;
			active_buffer = !active_buffer;
		}

		int16_t  x1, y1; // can these be NULL?
		uint16_t w, h;

		matrix.getTextBounds(strbuffer[active_buffer], 0, 0, &x1, &y1, &w, &h);
		x = -w;

		if (++pass >= ARRAYSIZE(colors))
			pass = 0;
		matrix.setTextColor(colors[pass]);
	}
}


void loop()
{
	rx_string(strbuffer[!active_buffer], sizeof(strbuffer));
	draw_string();

//	matrix.show();
	delay(30);
}
