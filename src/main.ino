#include <SPI.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <fletcher16.h>

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*a))

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))

// fast led constants
#define DATA_PIN	3	// labeled as RX on nodeMCU
#define COLOR_ORDER	GRB	// if colors are mismatched; change this

#define M_WIDTH 	75	// the width of the LED matrix
#define M_HEIGHT	8	// the height of the LED matrix
#define NUM_LEDS	(M_WIDTH*M_HEIGHT)

#define STRLEN_MAX	256
#define UDP_PORT	1337
#define TEXT_TTL	16
#define MSG_HASH_SIZE 16

// change WS2812B to match your type of LED, if different
// list of supported types is here:
// https://github.com/FastLED/FastLED/wiki/Overview
#define LED_TYPE    WS2812B

static CRGB leds[NUM_LEDS];
static FastLED_NeoMatrix matrix = FastLED_NeoMatrix(leds, M_WIDTH, M_HEIGHT, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);

static WiFiUDP Udp;

#define FLAG_VALID	(1 << 0)
#define FLAG_FIRE	(1 << 1)
#define FLAG_PLASMA 	(1 << 2)

static uint8_t flags[2];
static char strbuffer[2][STRLEN_MAX];
static int active_buffer;
static bool string_pending;
static int text_ttl;

uint16_t msg_hashes[MSG_HASH_SIZE];
int cur_hash_slot = 0;

static uint8_t fire_matrix[M_HEIGHT+1][M_WIDTH];

static void init_fire(uint8_t fire[M_HEIGHT][M_WIDTH], unsigned w, unsigned h) {
	memset(fire[h], 255, w);
}

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING  55

static int check_msg_hashes(uint16_t msg){
  for (int i = 0; i < MSG_HASH_SIZE; i++ ){
    if ( msg == msg_hashes[i] )
      return 1;
  }
  return 0;
}

static void draw_fire(uint8_t fire[M_HEIGHT][M_WIDTH], unsigned w, unsigned h) {
	for (int x = 0 ; x < w; ++x) {

		// Step 1.  Cool down every cell a little
		for(int y = 0; y < h; ++y) {
			fire[y][x] = qsub8(fire[y][x],  random8(0, ((COOLING * 10) / h) + 2));
		}

		// Step 2.  Heat from each cell drifts 'up' and diffuses a little
		for(int y = h - 1; y >= 0; --y) {
			unsigned _x = random8(max(0, x-1), min(w-1, x+1));
			int new_val = fire[y + 1][_x] - random8(0, 64);
			fire[y][x] = new_val < 0 ? 0 : new_val;
		}

		for (int y = 0; y < h; ++y) {
			CRGB color = HeatColor(scale8(fire[y][x], 240));
			matrix.drawPixel(x, y, matrix.Color(color.r, color.g, color.b));
		}
	}
}

void setup()
{
	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
	FastLED.setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(16);	// low brightness so we can test the strip just using USB
	FastLED.clear(true);


	matrix.begin();
	matrix.setTextWrap(false);	// so getTextBounds() gives proper results

  // clear the 'ring buffer'
  for (int i = 0; i < MSG_HASH_SIZE; i++ ){
    msg_hashes[i] = 0;
  }

	WiFi.begin("36C3-insecure");
	Udp.begin(UDP_PORT);

	init_fire(fire_matrix, M_WIDTH, M_HEIGHT);
}

static const struct dict {
	const char* utf8;
	uint8_t cp437;
} mapping[] = {
	{.utf8 = "ð", .cp437 = 0x1},
	{.utf8 = "ð", .cp437 = 0x2},
	{.utf8 = "♥", .cp437 = 0x3},
	{.utf8 = "♦", .cp437 = 0x4},
	{.utf8 = "♣", .cp437 = 0x5},
	{.utf8 = "♠", .cp437 = 0x6},
	{.utf8 = "•", .cp437 = 0x7},
	{.utf8 = "◘", .cp437 = 0x8},
	{.utf8 = "○", .cp437 = 0x9},
	{.utf8 = "◙", .cp437 = 0xa},
	{.utf8 = "♂", .cp437 = 0xb},
	{.utf8 = "♀", .cp437 = 0xc},
	{.utf8 = "♫", .cp437 = 0xe},
	{.utf8 = "☼", .cp437 = 0xf},
	{.utf8 = "▶", .cp437 = 0x10},
	{.utf8 = "◀", .cp437 = 0x11},
	{.utf8 = "↕", .cp437 = 0x12},
	{.utf8 = "‼", .cp437 = 0x13},
	{.utf8 = "¶", .cp437 = 0x14},
	{.utf8 = "§", .cp437 = 0x15},
	{.utf8 = "▬", .cp437 = 0x16},
	{.utf8 = "↨", .cp437 = 0x17},
	{.utf8 = "↑", .cp437 = 0x18},
	{.utf8 = "↓", .cp437 = 0x19},
	{.utf8 = "→", .cp437 = 0x1a},
	{.utf8 = "←", .cp437 = 0x1b},
	{.utf8 = "∟", .cp437 = 0x1c},
	{.utf8 = "↔", .cp437 = 0x1d},
	{.utf8 = "▲", .cp437 = 0x1e},
	{.utf8 = "▼", .cp437 = 0x1f},

	{.utf8 = "Ç", .cp437 = 0x80},
	{.utf8 = "ü", .cp437 = 0x81},
	{.utf8 = "é", .cp437 = 0x82},
	{.utf8 = "â", .cp437 = 0x83},
	{.utf8 = "ä", .cp437 = 0x84},
	{.utf8 = "à", .cp437 = 0x85},
	{.utf8 = "å", .cp437 = 0x86},
	{.utf8 = "ç", .cp437 = 0x87},
	{.utf8 = "ê", .cp437 = 0x88},
	{.utf8 = "ë", .cp437 = 0x89},
	{.utf8 = "è", .cp437 = 0x8a},
	{.utf8 = "ï", .cp437 = 0x8b},
	{.utf8 = "î", .cp437 = 0x8c},
	{.utf8 = "ì", .cp437 = 0x8d},
	{.utf8 = "Ä", .cp437 = 0x8e},
	{.utf8 = "Å", .cp437 = 0x8f},
	{.utf8 = "É", .cp437 = 0x90},
	{.utf8 = "æ", .cp437 = 0x91},
	{.utf8 = "Æ", .cp437 = 0x92},
	{.utf8 = "ô", .cp437 = 0x93},
	{.utf8 = "ö", .cp437 = 0x94},
	{.utf8 = "ò", .cp437 = 0x95},
	{.utf8 = "û", .cp437 = 0x96},
	{.utf8 = "ù", .cp437 = 0x97},
	{.utf8 = "ÿ", .cp437 = 0x98},
	{.utf8 = "Ö", .cp437 = 0x99},
	{.utf8 = "Ü", .cp437 = 0x9a},
	{.utf8 = "¢", .cp437 = 0x9b},
	{.utf8 = "£", .cp437 = 0x9c},
	{.utf8 = "¥", .cp437 = 0x9d},
	{.utf8 = "₧", .cp437 = 0x9e},
	{.utf8 = "ƒ", .cp437 = 0x9f},
	{.utf8 = "á", .cp437 = 0xa0},
	{.utf8 = "í", .cp437 = 0xa1},
	{.utf8 = "ó", .cp437 = 0xa2},
	{.utf8 = "ú", .cp437 = 0xa3},
	{.utf8 = "ñ", .cp437 = 0xa4},
	{.utf8 = "Ñ", .cp437 = 0xa5},
	{.utf8 = "ª", .cp437 = 0xa6},
	{.utf8 = "º", .cp437 = 0xa7},
	{.utf8 = "¿", .cp437 = 0xa8},
	{.utf8 = "⌐", .cp437 = 0xa9},
	{.utf8 = "¬", .cp437 = 0xaa},
	{.utf8 = "½", .cp437 = 0xab},
	{.utf8 = "¼", .cp437 = 0xac},
	{.utf8 = "¡", .cp437 = 0xad},
	{.utf8 = "«", .cp437 = 0xae},
	{.utf8 = "»", .cp437 = 0xaf},
	{.utf8 = "░", .cp437 = 0xb0},
	{.utf8 = "▒", .cp437 = 0xb1},
	{.utf8 = "▓", .cp437 = 0xb2},
	{.utf8 = "│", .cp437 = 0xb3},
	{.utf8 = "┤", .cp437 = 0xb4},
	{.utf8 = "╡", .cp437 = 0xb5},
	{.utf8 = "╢", .cp437 = 0xb6},
	{.utf8 = "╖", .cp437 = 0xb7},
	{.utf8 = "╕", .cp437 = 0xb8},
	{.utf8 = "╣", .cp437 = 0xb9},
	{.utf8 = "║", .cp437 = 0xba},
	{.utf8 = "╗", .cp437 = 0xbb},
	{.utf8 = "╝", .cp437 = 0xbc},
	{.utf8 = "╜", .cp437 = 0xbd},
	{.utf8 = "╛", .cp437 = 0xbe},
	{.utf8 = "┐", .cp437 = 0xbf},
	{.utf8 = "└", .cp437 = 0xc0},
	{.utf8 = "┴", .cp437 = 0xc1},
	{.utf8 = "┬", .cp437 = 0xc2},
	{.utf8 = "├", .cp437 = 0xc3},
	{.utf8 = "─", .cp437 = 0xc4},
	{.utf8 = "┼", .cp437 = 0xc5},
	{.utf8 = "╞", .cp437 = 0xc6},
	{.utf8 = "╟", .cp437 = 0xc7},
	{.utf8 = "╚", .cp437 = 0xc8},
	{.utf8 = "╔", .cp437 = 0xc9},
	{.utf8 = "╩", .cp437 = 0xca},
	{.utf8 = "╦", .cp437 = 0xcb},
	{.utf8 = "╠", .cp437 = 0xcc},
	{.utf8 = "═", .cp437 = 0xcd},
	{.utf8 = "╬", .cp437 = 0xce},
	{.utf8 = "╧", .cp437 = 0xcf},
	{.utf8 = "╨", .cp437 = 0xd0},
	{.utf8 = "╤", .cp437 = 0xd1},
	{.utf8 = "╥", .cp437 = 0xd2},
	{.utf8 = "╙", .cp437 = 0xd3},
	{.utf8 = "╘", .cp437 = 0xd4},
	{.utf8 = "╒", .cp437 = 0xd5},
	{.utf8 = "╓", .cp437 = 0xd6},
	{.utf8 = "╫", .cp437 = 0xd7},
	{.utf8 = "╪", .cp437 = 0xd8},
	{.utf8 = "┘", .cp437 = 0xd9},
	{.utf8 = "┌", .cp437 = 0xda},
	{.utf8 = "█", .cp437 = 0xdb},
	{.utf8 = "▄", .cp437 = 0xdc},
	{.utf8 = "▌", .cp437 = 0xdd},
	{.utf8 = "▐", .cp437 = 0xde},
	{.utf8 = "▀", .cp437 = 0xdf},
	{.utf8 = "α", .cp437 = 0xe0},
	{.utf8 = "ß", .cp437 = 0xe1},
	{.utf8 = "Γ", .cp437 = 0xe2},
	{.utf8 = "π", .cp437 = 0xe3},
	{.utf8 = "Σ", .cp437 = 0xe4},
	{.utf8 = "σ", .cp437 = 0xe5},
	{.utf8 = "µ", .cp437 = 0xe6},
	{.utf8 = "τ", .cp437 = 0xe7},
	{.utf8 = "Φ", .cp437 = 0xe8},
	{.utf8 = "Θ", .cp437 = 0xe9},
	{.utf8 = "Ω", .cp437 = 0xea},
	{.utf8 = "δ", .cp437 = 0xeb},
	{.utf8 = "∞", .cp437 = 0xec},
	{.utf8 = "φ", .cp437 = 0xed},
	{.utf8 = "ε", .cp437 = 0xee},
	{.utf8 = "∩", .cp437 = 0xef},
	{.utf8 = "≡", .cp437 = 0xf0},
	{.utf8 = "±", .cp437 = 0xf1},
	{.utf8 = "≥", .cp437 = 0xf2},
	{.utf8 = "≤", .cp437 = 0xf3},
	{.utf8 = "⌠", .cp437 = 0xf4},
	{.utf8 = "⌡", .cp437 = 0xf5},
	{.utf8 = "÷", .cp437 = 0xf6},
	{.utf8 = "≈", .cp437 = 0xf7},
	{.utf8 = "°", .cp437 = 0xf8},
	{.utf8 = "∙", .cp437 = 0xf9},
	{.utf8 = "·", .cp437 = 0xfa},
	{.utf8 = "√", .cp437 = 0xfb},
	{.utf8 = "ⁿ", .cp437 = 0xfc},
	{.utf8 = "²", .cp437 = 0xfd},
	{.utf8 = "■", .cp437 = 0xfe},
};

static void str_contract(unsigned char* str, size_t offset, size_t len) {
	while (*str && --len) {
		*str++ = str[offset];
	}
	*str = 0;
}

static uint8_t find_mapping(const unsigned char* c, size_t* len) {
	for (int i = 0; i < ARRAYSIZE(mapping); ++i) {
		if (memcmp(c, mapping[i].utf8, strlen(mapping[i].utf8)) == 0) {
			*len = strlen(mapping[i].utf8) - 1;
			return mapping[i].cp437;
		}
	}

	return 0;
}

static size_t cp437_replace(unsigned char* str, size_t len) {
	unsigned char* start = str;
	uint8_t cp437;
	size_t offset = 0;
	while (*str && len) {
		if ((*str > 127) && (cp437 = find_mapping(str, &offset))) {
			*str = cp437;
			len -= offset;
			str_contract(++str, offset, len);
		} else {
			++str;
			--len;
		}
	}

	return str - start;
}

static uint8_t process_string(char* s, size_t len) {
	uint8_t flags = 0;

	for (; *s && len--; ++s) {
		if (*s == '\n' || *s == '\r') {
			*s = ' ';
			continue;
		}

		if (*s < ' ') {
			switch (*s) {
				case '\b':
					flags |= FLAG_PLASMA;
					*s = ' ';
					break;
				case '\f':
					flags |= FLAG_FIRE;
					*s = ' ';
					break;
			}
		} else if (!isspace(*s))
			flags |= FLAG_VALID;
	}

	return flags;
}

static void send_reply(const char* msg) {
	Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
	Udp.write(msg);
	Udp.write("Previous msg: ");
	Udp.write(strbuffer[active_buffer]);
	Udp.endPacket();
}

static void rx_string(char* dst, size_t n) {
	int len = Udp.parsePacket();

	if (len <= 0)
		return;

	dst[0] = 0; // len == 0 should be detected - why check twice?
	len = Udp.read(dst, n);

	if (len <= 0 || len > n) {
		send_reply("Invalid String\n");
		return;
	}
	dst[len] = 0;

	len = cp437_replace((unsigned char *)dst, len);
	uint8_t f = process_string(dst, len);

	uint16_t cur_hash = fletcher16((unsigned char *)dst, len);
	if (check_msg_hashes(cur_hash) == 1){
		send_reply("Please dohnut spam\n");
		return;
	}

	if ((f & FLAG_VALID) == 0) {
		send_reply("Invalid String\n");
		return;
	}

	msg_hashes[cur_hash_slot] = cur_hash;
	cur_hash_slot = (cur_hash_slot+1) % MSG_HASH_SIZE;

	send_reply("OK\n");

	string_pending = true;
	flags[!active_buffer] = f;
	return;
}

static inline uint8_t _get_color_cos(int frame, float period_mod) {
	return cos8(frame * period_mod);
}

static void rgb_cycle(uint32_t frame, const uint32_t frames_per_color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = 0;
    *g = 0;
    *b = 0;

    const float period_mod = 0xff / (float) frames_per_color;
    const uint32_t half_period = frames_per_color / 2;
    frame = frame % (3 * half_period);

    if (frame <= half_period)
        *r = _get_color_cos(frame, period_mod);
    if (frame <= 2 * half_period)
        *g = _get_color_cos(frame - half_period, period_mod);
    if (frame >= half_period)
        *b = _get_color_cos(frame - 2 * half_period, period_mod);
    if (frame >= 2 * half_period)
        *r = _get_color_cos(frame - 3 * half_period, period_mod);
}

static CRGBPalette16 currentPalette = RainbowColors_p;
static CRGB currentColor;

static CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
	return ColorFromPalette(currentPalette, index, brightness, blendType);
}

static void rainbow_print(const char* s, bool random_color) {
	static unsigned i;
	CRGB color = CRGB::Black;
	bool wobbly = false;

	for (; *s; ++s) {
		if (*s == '\a') {
			wobbly = !wobbly;
			continue;
		}

		if (wobbly)
			matrix.setCursor(matrix.getCursorX(), 2 - 4 * sin8(8 * matrix.getCursorX()) / 255);
		else
			matrix.setCursor(matrix.getCursorX(), 0);

		if (random_color)
			rgb_cycle(++i/2 - 10 * matrix.getCursorX(), 1000, &color.r, &color.g, &color.b);

		matrix.setTextColor(matrix.Color(color.r, color.g, color.b));
		matrix.print(*s);
	}
}

static void draw_plasma(void) {
	static int time, cycles;

	for (int x = 0; x <  matrix.width(); x++) {
		for (int y = 0; y <  matrix.height(); y++) {
			int16_t v = 0;
			uint8_t wibble = sin8(time);
			v += sin16(x * wibble * 3 + time);
			v += cos16(y * (128 - wibble)  + time);
			v += sin16(y * x * cos8(-time) / 8);

			currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
			matrix.drawPixel(x, y, matrix.Color(currentColor.r, currentColor.g, currentColor.b));
		}
	}

	time += 1;
	cycles++;

	if (cycles >= 2048) {
		time = 0;
		cycles = 0;
	}
}

static void draw_string(uint8_t current_flags) {

	static int x;
	static uint16_t width_txt;

	matrix.clear();
	matrix.setCursor(x, 0);

	bool random = true;
	if (current_flags & FLAG_PLASMA) {
		random = false;
		draw_plasma();
	}
	else if (current_flags & FLAG_FIRE) {
		random = false;
		draw_fire(fire_matrix, M_WIDTH, M_HEIGHT);
	}

	rainbow_print(strbuffer[active_buffer], random);

	if (--x < -width_txt) {

		if (string_pending) {
			string_pending = false;
			active_buffer = !active_buffer;
			text_ttl = TEXT_TTL;
		}

		if (text_ttl == 0) {
			switch (WiFi.status()) {
				case WL_CONNECTED:
					text_ttl = 16;
					flags[active_buffer] = 0;
					snprintf(strbuffer[active_buffer], STRLEN_MAX, "\aSend me Text!\a - UDP %s:%d", "led.ecohacker.farm", UDP_PORT);
//					snprintf(strbuffer[active_buffer], STRLEN_MAX, "\aSend me Text!\a - UDP %s:%d", WiFi.localIP().toString().c_str(), UDP_PORT);
					break;
				default:
					text_ttl = 1;
					snprintf(strbuffer[active_buffer], STRLEN_MAX, "trying to connect to %s...", WiFi.SSID().c_str());
			}
		} else
			--text_ttl;

		int16_t  x1, y1; // getTextBounds() doesn't accept NULL
		uint16_t h;	 // for parameters we are not intrested in

		matrix.getTextBounds(strbuffer[active_buffer], 0, 0, &x1, &y1, &width_txt, &h);
		x = matrix.width();
	}
}


void loop()
{
	rx_string(strbuffer[!active_buffer], STRLEN_MAX);
	draw_string(flags[active_buffer]);

	matrix.show();
	delay(30);
}
