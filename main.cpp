#include "Arduino.h"
#include "usb_keyboard.h"

#define NUMCOLS 11
#define NUMROWS 4
#define NUMKEYS (NUMCOLS * NUMROWS)

static void scan_keyboard();
static void decode();


// the setup function runs once when you press reset or power the board
void setup() {
    // columns are on pins 0 .. 10, active low
    pinMode(0, INPUT_PULLUP);
    pinMode(1, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);
    pinMode(3, INPUT_PULLUP);
    pinMode(4, INPUT_PULLUP);
    pinMode(5, INPUT_PULLUP);
    pinMode(6, INPUT_PULLUP);
    pinMode(7, INPUT_PULLUP);
    pinMode(8, INPUT_PULLUP);
    pinMode(9, INPUT_PULLUP);
    pinMode(10,INPUT_PULLUP);

    // rows are on pins 14 .. 17, high to start with
    pinMode(14, OUTPUT);
    pinMode(15, OUTPUT);
    pinMode(16, OUTPUT);
    pinMode(17, OUTPUT);

    digitalWrite(14, HIGH);
    digitalWrite(15, HIGH);
    digitalWrite(16, HIGH);
    digitalWrite(17, HIGH);

#if 0
    // debugging aid: LED
    pinMode(13,OUTPUT);
    digitalWrite(13, 0);
#endif
}

// all the keys currently pressed (list of raw keycodes)
static int raw_count = 0;
static int raw_keys[NUMKEYS];

// Scan all keys for pressed keys
// Writes to raw_keys
static void scan_keyboard() {
    raw_count = 0;
    // Rows are on pins 14 .. 17 inclusive
    for(int row = 0; row < NUMROWS; ++row) {
        digitalWrite(14+row, LOW);
        delayMicroseconds(50);
        // Columns are on pins 0 .. 11 inclusive
        for(int col = 0; col < NUMCOLS; ++col) {
            if (!digitalRead(col)) {
                raw_keys[raw_count++] = row * NUMCOLS + col;
            }
        }
        digitalWrite(14+row, HIGH);
    }
}

// This macro relates the physical layout of the keys to their position
// in the key matrix
#define LAYER( \
    K00, K01, K02, K03, K04,           K05, K06, K07, K08, K09, \
    K10, K11, K12, K13, K14,           K15, K16, K17, K18, K19, \
    K20, K21, K22, K23, K24,           K25, K26, K27, K28, K29, \
    K30, K31, K32, K33, K34, K35, K36, K37, K38, K39, K3A, K3B  \
) { \
    K3B, K3A, K39, K38, K37, K36, K34, K33, K32, K31, K30, \
    K29, K28, K27, K26, K25, K35, K24, K23, K22, K21, K20, \
    K19, K18, K17, K16, K15, 0,   K14, K13, K12, K11, K10, \
    K09, K08, K07, K06, K05, 0,   K04, K03, K02, K01, K00  \
}

#define LSHIFT MODIFIERKEY_LEFT_SHIFT
#define RSHIFT MODIFIERKEY_RIGHT_SHIFT
#define LCTRL MODIFIERKEY_LEFT_CTRL
#define RCTRL MODIFIERKEY_RIGHT_CTRL

static const KEYCODE_TYPE layers[][NUMKEYS] = {
    // Hardware Dvorak
    LAYER(
    KEY_QUOTE,     KEY_COMMA, KEY_PERIOD, KEY_P,         KEY_Y,                     KEY_F,     KEY_G,    KEY_C,    KEY_R,  KEY_L,
    KEY_A,         KEY_O,     KEY_E,      KEY_U,         KEY_I,                     KEY_D,     KEY_H,    KEY_T,    KEY_N,  KEY_S,
    KEY_SEMICOLON, KEY_Q,     KEY_J,      KEY_K,         KEY_X,                     KEY_B,     KEY_M,    KEY_W,    KEY_V,  KEY_Z,
    KEY_ESC,       KEY_TAB,   LCTRL,      KEY_BACKSPACE, KEY_ENTER, LSHIFT, RSHIFT, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    ),
    // Qwerty / Software Dvorak
    LAYER(
    KEY_Q,         KEY_W,     KEY_E,      KEY_R,         KEY_T,                     KEY_Y,     KEY_U,    KEY_I,    KEY_O,  KEY_P,
    KEY_A,         KEY_S,     KEY_D,      KEY_F,         KEY_G,                     KEY_H,     KEY_J,    KEY_K,    KEY_L,  KEY_SEMICOLON,
    KEY_Z,         KEY_X,     KEY_C,      KEY_V,         KEY_B,                     KEY_N,     KEY_M,    KEY_COMMA,KEY_PERIOD,  KEY_SLASH,
    KEY_ESC,       KEY_TAB,   LCTRL,      KEY_BACKSPACE, KEY_ENTER, LSHIFT, RSHIFT, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    )
};

static int current_layer = 1;

// decode and send keys to USB
static void decode() {
    int count = 0;
    keyboard_modifier_keys = 0;
    for(int i = 0; i < 6; ++i) {
        keyboard_keys[i] = 0;
    }
    for(int i = 0; i < raw_count; ++i) {
        int raw = raw_keys[i];
        KEYCODE_TYPE keycode = layers[current_layer][raw];
        if (keycode & 0x8000) { // modifier key
            keyboard_modifier_keys |= (keycode & 0xff);
        } else if (keycode & 0x4000) { // normal key
            if (count < 6) {
                keyboard_keys[count++] = keycode & 0xff;
            }
        } else {
            // ignore anything else
        }
    }
}

// Record previous send - to avoid sending same message twice in a row
static uint8_t prev_modifiers;
static uint8_t prev_keys[6];

// the loop function runs over and over again forever
void loop() {
    scan_keyboard();
    decode();

    if (prev_modifiers != keyboard_modifier_keys
       || memcmp(prev_keys, keyboard_keys, 6)) {
        prev_modifiers = keyboard_modifier_keys;
        memcpy(prev_keys, keyboard_keys, 6);
        usb_keyboard_send();
    }

    delay(10); // sample at 100Hz
}
