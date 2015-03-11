#include "Arduino.h"
#include "usb_keyboard.h"

#define NUMCOLS 11
#define NUMROWS 4
#define NUMKEYS (NUMCOLS * NUMROWS)

#define DEBOUNCE_TIMEOUT 1

static inline void raw_key_press(uint8_t key);
static boolean test_key(int rawkey);
static void scan_keyboard();
static void clear_keys();
static void press_key(uint8_t key);
static void release_key(uint8_t key);
static void send_keys();
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

    clear_keys();
}

// Keys are debounced by starting a timeout when a key is pressed.
// If the timeout is non-zero, we send a key press
// When the timeout counts down to zero, we send a key release
static uint8_t timeouts[NUMKEYS];

// list of keys that changed state in last scan (list of raw keycodes, bit 7 set if released)
static uint8_t raw_count = 0;
static uint16_t raw_keys[NUMKEYS];

static inline void raw_key_press(uint8_t key) {
    raw_keys[raw_count++] = key;
}

// Test if key is currently pressed
static boolean test_key(int rawkey) {
    for(int i = 0; i < raw_count; ++i) {
        if (raw_keys[i] == rawkey) {
            return true;
        }
    }
    return timeouts[rawkey] != 0;
}

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
            int key = row * NUMCOLS + col;
            int timeout = timeouts[key];
            if (!digitalRead(col)) { // pressed down
                if (timeout == 0) {
                    raw_key_press(key | 0x80); // newly pressed
                }
                timeouts[key] = DEBOUNCE_TIMEOUT;
            } else { // not pressed
                if (timeout > 0) { // previously pressed
                    --timeout;
                    timeouts[key] = timeout;
                    if (timeout == 0) {
                        raw_key_press(key); // newly released
                    }
                }
            }
        }
        digitalWrite(14+row, HIGH);
    }
}

static void clear_keys() {
    keyboard_modifier_keys = 0;
    for(int i = 0; i < 6; ++i) {
        keyboard_keys[i] = 0;
    }
}

static void press_key(uint8_t key) {
    for(int i = 0; i < 6; ++i) {
        if (keyboard_keys[i] == 0) {
            keyboard_keys[i] = key;
            return;
        }
    }
}

static void release_key(uint8_t key) {
    for(int i = 0; i < 6; ++i) {
        if (keyboard_keys[i] == key) {
            keyboard_keys[i] = 0;
        }
    }
}

static void send_keys() {
    usb_keyboard_send();
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

// Modifier numbers - in same order as MODIFIERKEY_* in Teensy library
#define LEFT_CTRL   0
#define LEFT_SHIFT  1
#define LEFT_ALT    2
#define LEFT_GUI    3
#define RIGHT_CTRL  4
#define RIGHT_SHIFT 5
#define RIGHT_ALT   6
#define RIGHT_GUI   7

// In the teensy firmware, keys are represented by a 16-bit number
// We extend this scheme by using some of the unused encodings
// - bit 15 - set if it is a modifier
//            bits 0-7  = 1 << M (M is modifier number)
// - bit 14 - set if it is a normal key
//            bits 0-7  = which key
// - bit 11 - set if it has a modifier
//            bits 0-7  = which key is held
//            bits 8-10 = which modifier is held
//            [This is not part of the Teensy firmware]
#define IS_MODIFIER(k) ((k) & 0x8000)
#define IS_NORMAL(k)   ((k) & 0x4000)
#define IS_MODKEY(k)   ((k) & 0x0800)

#define MODKEY(k,m) (0x0800 | (k) | ((m) << 8))
#define SHIFT(k) MODKEY(k, LEFT_SHIFT)

#define KEY_DOUBLEQUOTE SHIFT(KEY_QUOTE)
#define KEY_BACKQUOTE   SHIFT(KEY_TILDE)
#define KEY_BANG        SHIFT(KEY_1)
#define KEY_SPLAT       SHIFT(KEY_2)
#define KEY_HASH        SHIFT(KEY_3)
#define KEY_DOLLAR      SHIFT(KEY_4)
#define KEY_PERCENT     SHIFT(KEY_5)
#define KEY_CARET       SHIFT(KEY_6)
#define KEY_AMPERSAND   SHIFT(KEY_7)
#define KEY_STAR        SHIFT(KEY_8)
#define KEY_LEFT_PAREN  SHIFT(KEY_9)
#define KEY_RIGHT_PAREN SHIFT(KEY_0)
#define KEY_PLUS        SHIFT(KEY_EQUAL)
#define KEY_UNDERSCORE  SHIFT(KEY_MINUS)
#define KEY_QUERY       SHIFT(KEY_SLASH)
#define KEY_PIPE        SHIFT(KEY_BACKSLASH)
#define KEY_LEFT_CURL   SHIFT(KEY_LEFT_BRACE)
#define KEY_RIGHT_CURL  SHIFT(KEY_RIGHT_BRACE)

#define LSHIFT MODIFIERKEY_LEFT_SHIFT
#define RSHIFT MODIFIERKEY_RIGHT_SHIFT
#define LCTRL MODIFIERKEY_LEFT_CTRL
#define RCTRL MODIFIERKEY_RIGHT_CTRL

static const KEYCODE_TYPE layers[][NUMKEYS] = {
    // Qwerty / Software Dvorak
    [0] =
    LAYER(
    KEY_Q,         KEY_W,     KEY_E,      KEY_R,         KEY_T,             KEY_Y,     KEY_U,    KEY_I,    KEY_O,       KEY_P,
    KEY_A,         KEY_S,     KEY_D,      KEY_F,         KEY_G,             KEY_H,     KEY_J,    KEY_K,    KEY_L,       KEY_SEMICOLON,
    KEY_Z,         KEY_X,     KEY_C,      KEY_V,         KEY_B,             KEY_N,     KEY_M,    KEY_COMMA,KEY_PERIOD,  KEY_SLASH,
    0,             0,         0,          0,             0,       0, 0,     0,         0,        0,        0,      0
    ),

    // Punctuation (for Software Dvorak)
    [1] =
    LAYER(
    KEY_TILDE, KEY_BACKQUOTE, KEY_RIGHT_BRACE, KEY_RIGHT_CURL, 0,           0,         KEY_QUOTE,     KEY_DOUBLEQUOTE, KEY_UNDERSCORE, KEY_PLUS,
    KEY_BANG,      KEY_SPLAT, KEY_HASH,        KEY_DOLLAR,     KEY_PERCENT, KEY_CARET, KEY_AMPERSAND, KEY_STAR,        KEY_LEFT_PAREN, KEY_RIGHT_PAREN,
    0,             0,         KEY_LEFT_CURL,   KEY_LEFT_BRACE, 0,           0,         KEY_BACKSLASH, KEY_PIPE,        KEY_MINUS,      KEY_EQUAL,
    0,             0,         0,          0,             0,       0, 0,     0,         0,        0,        0,      0
    ),

    // Numbers
    [2] =
    LAYER(
    0,             0,         0,          0,             0,                 0,         0,        0,        0,      0,
    KEY_1,         KEY_2,     KEY_3,      KEY_4,         KEY_5,             KEY_6,     KEY_7,    KEY_8,    KEY_9,  KEY_0,
    0,             0,         0,          0,             0,                 0,         0,        0,        0,      0,
    0,             0,         0,          0,             0,       0, 0,     0,         0,        0,        0,      0
    ),

    // Modifiers
    [3] =
    LAYER(
    0,             0,         0,          0,             0,                 0,         0,        0,        0,      0,
    0,             0,         0,          0,             0,                 0,         0,        0,        0,      0,
    0,             0,         0,          0,             0,                 0,         0,        0,        0,      0,
    KEY_ESC,       KEY_TAB,   LCTRL,           KEY_BACKSPACE,  KEY_ENTER, 0, 0, KEY_SPACE, KEY_LEFT,      KEY_DOWN,        KEY_UP,         KEY_RIGHT
    )

#if 0
    // Hardware Dvorak
    [3] =
    LAYER(
    KEY_QUOTE,     KEY_COMMA, KEY_PERIOD, KEY_P,         KEY_Y,                     KEY_F,     KEY_G,    KEY_C,    KEY_R,  KEY_L,
    KEY_A,         KEY_O,     KEY_E,      KEY_U,         KEY_I,                     KEY_D,     KEY_H,    KEY_T,    KEY_N,  KEY_S,
    KEY_SEMICOLON, KEY_Q,     KEY_J,      KEY_K,         KEY_X,                     KEY_B,     KEY_M,    KEY_W,    KEY_V,  KEY_Z,
    KEY_ESC,       KEY_TAB,   LCTRL,      KEY_BACKSPACE, KEY_ENTER, LSHIFT, RSHIFT, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    ),

    // Punctuation (for Qwerty and Hardware Dvorak)
    [4] =
    LAYER(
    KEY_TILDE, KEY_BACKQUOTE, KEY_EQUAL,  KEY_PLUS,      0,                 0,         KEY_MINUS,     KEY_UNDERSCORE,KEY_LEFT_CURL,  KEY_RIGHT_CURL,
    KEY_BANG,      KEY_SPLAT, KEY_HASH,   KEY_DOLLAR,    KEY_PERCENT,       KEY_CARET, KEY_AMPERSAND, KEY_STAR, KEY_LEFT_PAREN,  KEY_RIGHT_PAREN,
    0,             0,         KEY_QUERY,  KEY_SLASH,     0,                 0,         KEY_BACKSLASH, KEY_PIPE, KEY_LEFT_BRACE,  KEY_RIGHT_BRACE,
    KEY_ESC,       KEY_TAB,   LCTRL,      KEY_BACKSPACE, KEY_ENTER, 0, 0, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    ),
#endif
};

#define NUM_LAYERS (sizeof(layers) / sizeof(layers[0]))
// #define NUM_LAYERS 5

static uint32_t enabled_layers = (1 << 3) | (1 << 0);

// decode and send keys to USB
static void decode() {
    for(int i = 0; i < raw_count; ++i) {
        int raw = raw_keys[i];
        boolean down = raw & 0x80;
        raw = raw & 0x7f;
        KEYCODE_TYPE keycode = 0;
        // search layers for keycode
        for(int layer = NUM_LAYERS-1; layer >= 0; --layer) {
            if (enabled_layers & (1 << layer)) {
                keycode = layers[layer][raw];
                if (keycode) {
                    goto found;
                }
            }
        }
found:
        if (raw == 5 && down) { // left shift key
            enabled_layers ^= 2; // toggle punctuation
        } else if (IS_MODIFIER(keycode)) { // modifier key
            if (down) {
                keyboard_modifier_keys |= (keycode & 0xff);
            } else {
                keyboard_modifier_keys &= ~(keycode & 0xff);
            }
        } else if (IS_NORMAL(keycode)) { // normal key
            if (down) {
                press_key(keycode & 0xff);
            } else {
                release_key(keycode & 0xff);
            }
            if (IS_MODKEY(keycode)) {
                uint8_t mod = (keycode >> 8) & 0x7;
                if (down) {
                    keyboard_modifier_keys |= (1 << mod);
                } else {
                    keyboard_modifier_keys &= ~(1 << mod);
                }
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

#if 0
    if (prev_modifiers != keyboard_modifier_keys
       || memcmp(prev_keys, keyboard_keys, 6)) {
        prev_modifiers = keyboard_modifier_keys;
        memcpy(prev_keys, keyboard_keys, 6);
        send_keys();
    }
#else
    send_keys();
#endif

    delay(10); // sample at 100Hz
}
