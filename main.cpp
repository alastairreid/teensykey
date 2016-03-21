#include "Arduino.h"
#include "usb_keyboard.h"

#define NUMCOLS 12
#define NUMROWS 6
#define NUMKEYS (NUMCOLS * NUMROWS)

#define HAVE_TAPPERS  0
#define HAVE_STICKIES 0

#define DEBOUNCE_TIMEOUT 1
#if HAVE_TAPPERS
#define TAPPER_TIMEOUT   30
#endif

// Modifier numbers - in same order as MODIFIERKEY_* in Teensy library
// LAYER select are extensions
#define LEFT_CTRL   0
#define LEFT_SHIFT  1
#define LEFT_ALT    2
#define LEFT_GUI    3
#define RIGHT_CTRL  4
#define RIGHT_SHIFT 5
#define RIGHT_ALT   6
#define RIGHT_GUI   7
#define LAYER0      8
#define LAYER1      9
#define LAYER2      10
#define LAYER3      11

static inline void raw_key_press(uint8_t key);
#if 0
static boolean test_key(uint8_t rawkey);
#endif
static void scan_keyboard();
static void clear_keys();
static void press_key(uint8_t raw, uint8_t key);
static void release_key(uint8_t raw);
static void send_keys();
static void send_unicode(uint16_t keycode);
#if HAVE_TAPPERS
static void clear_tappers();
static void update_tappers();
static void resolve_tappers(boolean tapper, boolean right);
static void press_tapper(uint8_t raw, uint8_t key, uint8_t mod);
static void release_tapper(uint8_t key, uint8_t mod);
#endif
#if HAVE_STICKIES
static void init_stickies();
static void resolve_stickies(boolean down);
static void press_sticky(uint8_t mod);
static void release_sticky(uint8_t mod);
#endif
static uint16_t find_key(uint8_t raw);
static void press_modifier(uint8_t mod);
static void release_modifier(uint8_t mod);
static void decode();

////////////////////////////////////////////////////////////////
// Arduino entry points
////////////////////////////////////////////////////////////////

// the setup function runs once when you press reset or power the board
void setup() {
    // columns are on pins 11, 12, 14 .. 22, active low
    pinMode(11, INPUT_PULLUP);
    pinMode(12, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
    pinMode(15, INPUT_PULLUP);
    pinMode(16, INPUT_PULLUP);
    pinMode(17, INPUT_PULLUP);
    pinMode(18, INPUT_PULLUP);
    pinMode(19, INPUT_PULLUP);
    pinMode(20, INPUT_PULLUP);
    pinMode(21, INPUT_PULLUP);
    pinMode(22, INPUT_PULLUP);
    pinMode(23, INPUT_PULLUP);

    // rows are on pins 0 .. 5, high to start with
    pinMode(0, OUTPUT);
    pinMode(1, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);

    digitalWrite(0, HIGH);
    digitalWrite(1, HIGH);
    digitalWrite(2, HIGH);
    digitalWrite(3, HIGH);
    digitalWrite(4, HIGH);
    digitalWrite(5, HIGH);

#if 0
    // debugging aid: LED
    pinMode(13,OUTPUT);
    digitalWrite(13, 0);
#endif

    clear_keys();
#if HAVE_TAPPERS
    clear_tappers();
#endif
#if HAVE_STICKIES
    init_stickies();
#endif
}

#if 0
// Record previous send - to avoid sending same message twice in a row
static uint8_t prev_modifiers;
static uint8_t prev_keys[6];
#endif

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

////////////////////////////////////////////////////////////////
// Physical keyboard scan/debounce support
////////////////////////////////////////////////////////////////

// Keys are debounced by starting a timeout when a key is pressed.
// If the timeout is non-zero, we send a key press
// When the timeout counts down to zero, we send a key release
static uint8_t timeouts[NUMKEYS];

// list of keys that changed state in last scan (list of raw keycodes, bit 7 set if released)
static uint8_t raw_count = 0;
static uint8_t raw_keys[NUMKEYS];

static inline void raw_key_press(uint8_t key) {
    raw_keys[raw_count++] = key;
}

#if 0
// Test if key is currently pressed
static boolean test_key(uint8_t rawkey) {
    for(int i = 0; i < raw_count; ++i) {
        if (raw_keys[i] == rawkey) {
            return true;
        }
    }
    return timeouts[rawkey] != 0;
}
#endif

// Scan all keys for pressed keys
// Writes to raw_keys
static void scan_keyboard() {
    raw_count = 0;
    for(int row = 0; row < NUMROWS; ++row) {
        int rowpin = row; // Rows are on pins 0 .. 5 inclusive
        digitalWrite(rowpin, LOW);
        delayMicroseconds(50);
        for(int col = 0; col < NUMCOLS; ++col) {
            int colpin = col < 2 ? 11+col : 12+col; // Columns are on pins 11, 12, 14 .. 22 inclusive
            int key = row * NUMCOLS + col;
            int timeout = timeouts[key];
            if (!digitalRead(colpin)) { // pressed down
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
        digitalWrite(rowpin, HIGH);
    }
}

////////////////////////////////////////////////////////////////
// USB Keyboard interface
//
// At most one keypress (and any modifiers) is reported over USB
// at a time.
//
// The physical key pressed is tracked so that the key release
// removes the translated key from the USB buffer.
////////////////////////////////////////////////////////////////

static uint8_t raw_press;

static void clear_keys() {
    keyboard_modifier_keys = 0;
    for(int i = 0; i < 6; ++i) {
        keyboard_keys[i] = 0;
    }
    raw_press = 0xff;
}

static void press_key(uint8_t raw, uint8_t key) {
    raw_press        = raw;
    keyboard_keys[0] = key;
}

static void release_key(uint8_t raw) {
    if (raw == raw_press) {
        keyboard_keys[0] = 0;
    }
}

static void send_keys() {
    usb_keyboard_send();
}

static uint16_t hex_to_raw[16] = {
    KEY_0, KEY_1, KEY_2, KEY_3,
    KEY_4, KEY_5, KEY_6, KEY_7,
    KEY_8, KEY_9, KEY_A, KEY_B,
    KEY_C, KEY_D, KEY_E, KEY_F,
};

static void send_key(uint16_t key) {
    keyboard_keys[0] = (uint8_t)key;
    send_keys();
    delay(10);
    keyboard_keys[0] = 0;
    send_keys();
    delay(10);
}

static void send_unicode(uint16_t code) {
    clear_keys();
    send_key(KEYPAD_2);
    keyboard_modifier_keys = (1 << LEFT_ALT);
    for(int i = 12; i>=0; i-=4) {
        send_key(hex_to_raw[(code >> i) & 0xf]);
    }
    keyboard_modifier_keys = 0;
    send_key(KEYPAD_1);
    clear_keys();
}


#if HAVE_TAPPERS
////////////////////////////////////////////////////////////////
// Tapping modifier support
////////////////////////////////////////////////////////////////

// Tapping modifiers are either a normal key or a modifier but we can't tell
// which until the press times out (it is a normal key) or until a normal key
// is pressed (it is a modifier).
// Tappers can also be resolved by using a key from the other side of the
// keyboard.
// While waiting to resolve, tapping modifiers are buffered
struct tapping_state {
    uint8_t key;
    uint8_t raw;
    uint8_t tick;
    boolean modder;  // is the key acting as a modder?
};
#define NUM_TAPPERS 12
struct tapping_state tappers[NUM_TAPPERS];

static void clear_tappers() {
    for(int i = 0; i < NUM_TAPPERS; ++i) {
        tappers[i].tick   = 0;
        tappers[i].modder = false;
    }
}

// decrement timer on each tapper and trigger if timed out
static void update_tappers() {
    for(int i = 0; i < NUM_TAPPERS; ++i) {
        int tick = tappers[i].tick;
        if (tick) {
            --tick;
            tappers[i].tick = tick;
            if (tick == 0) { // timeout expired
                press_key(tappers[i].raw, tappers[i].key);
                send_keys();
            }
        }
    }
}

// if a normal key is pressed or a tapper from the opposite side,
// all pending tappers are resolved
static void resolve_tappers(boolean tapper, boolean right) {
    for(int i = 0; i < NUM_TAPPERS; ++i) {
        boolean right_tapper = (i & 4) || (i == LAYER1);
        if (!tapper || (right != right_tapper)) {
            int tick = tappers[i].tick;
            if (tick) {
                tappers[i].tick   = 0;
                tappers[i].modder = true;
                press_modifier(i);
            }
        }
    }
}

// set tapper timer if not already running
static void press_tapper(uint8_t raw, uint8_t key, uint8_t mod) {
    if (tappers[mod].tick == 0) { // not already pressed
        tappers[mod].key    = key;
        tappers[mod].raw    = raw;
        tappers[mod].tick   = TAPPER_TIMEOUT;
        tappers[mod].modder = false;
    }
}

static void release_tapper(uint8_t key, uint8_t mod) {
    if (tappers[mod].modder) { // being treated as a modifier
        tappers[mod].tick   = 0;
        tappers[mod].modder = false;
        release_modifier(mod);
    } else { // being treated as a key
        tappers[mod].tick   = 0;
        press_key(tappers[mod].raw, tappers[mod].key);
        send_keys();
        release_key(tappers[mod].raw);
    }
}
#endif

#if HAVE_STICKIES
////////////////////////////////////////////////////////////////
// Sticky modifier support
//
// Sticky modifiers change state each time the modifier is pressed:
// - hold + key == shifted key
// - tap  + key == shifted key
// - double-tap + keys + tap == shifted keys
////////////////////////////////////////////////////////////////

struct sticky_state {
    uint8_t raw;
    uint8_t state; // 0..5
};
#define NUM_STICKY 10
struct sticky_state sticky[NUM_STICKY];

static void init_stickies() {
    for(int i = 0; i < NUM_STICKY; ++i) {
        sticky[i].state = 0;
    }
}

// called when a normal key is pressed
static void resolve_stickies(boolean down) {
    for(int mod = 0; mod < NUM_STICKY; ++mod) {
        if (sticky[mod].state) {
            switch (sticky[mod].state) {
                case 1: sticky[mod].state = 5; break;
                case 2: if (!down) sticky[mod].state = 0; break;
                case 3: sticky[mod].state = 5; break;
            }
            if (sticky[mod].state) {
                press_modifier(mod);
            } else {
                release_modifier(mod);
            }
        }
    }
}

static void press_sticky(uint8_t mod) {
    switch (sticky[mod].state) {
        case 0: sticky[mod].state = 1; break;
        case 2: sticky[mod].state = 3; break;
        case 4: sticky[mod].state = 5; break;
    }
    if (sticky[mod].state) {
        press_modifier(mod);
    }
}

static void release_sticky(uint8_t mod) {
    switch (sticky[mod].state) {
        case 1: sticky[mod].state = 2; break;
        case 3: sticky[mod].state = 4; break;
        case 5: sticky[mod].state = 0; break;
    }
    if (sticky[mod].state == 0) {
        release_modifier(mod);
    }
}
#endif // HAVE_STICKIES

////////////////////////////////////////////////////////////////
// Keyboard mapping support
////////////////////////////////////////////////////////////////

// This macro relates the physical layout of the keys to their position
// in the key matrix
#define LAYER( \
    K00, K01, K02, K03, K04, K05,                K06, K07, K08, K09, K0A, K0B, \
    K10, K11, K12, K13, K14, K15,                K16, K17, K18, K19, K1A, K1B, \
    K20, K21, K22, K23, K24, K25,                K26, K27, K28, K29, K2A, K2B, \
    K30, K31, K32, K33, K34, K35,                K36, K37, K38, K39, K3A, K3B, \
         K41, K42, K43, K44,                          K47, K48, K49, K4A,      \
                                  K51, K52, K53,                               \
                   K60, K61, K62, K63,      K64, K65, K66, K67                 \
) { \
    K67, K66, K65, K64, K53, K52, K63, K62, K61, K60, K51, 0, \
    0,   K4A, K49, K48, K47, 0,   K43, K42, K41, 0,   K44, 0,   \
    K3B, K3A, K39, K38, K37, K36, K33, K32, K31, K30, K34, K35, \
    K2B, K2A, K29, K28, K27, K26, K23, K22, K21, K20, K24, K25, \
    K1B, K1A, K19, K18, K17, K16, K13, K12, K11, K10, K14, K15, \
    K0B, K0A, K09, K08, K07, K06, K03, K02, K01, K00, K04, K05, \
}

// In the teensy firmware, keys are represented by a 16-bit number
// We extend this scheme by using some of the unused encodings
// - bit 15 - set if it is a modifier
//            bits 9:0  = 1 << M (M is modifier number)
//            [bits 9:8 are not part of the Teensy firmware]
// - bit 14 - set if it is a normal key
//            bits 6:0  = which key
// - bits 13:11
//     [Not part of the Teensy firmware]
//     '000' - this extension is not used
//     '001' - tapping modifier
//            bits 6:0  = which key if tapped
//            bits 10:7 = which modifier if held
//                        Each modifier must be used only once in each layer
//                        because of the way tapping modifiers are buffered
//            [This is not part of the Teensy firmware]
//     '010' - key + modifier
//            bits 6:0  = which key is held
//            bits 10:7 = which modifier is held
//            [This is not part of the Teensy firmware]
//     '011' - media key
//            bits 7:0  = 1 << M (M is media key number)
//     '100' - sticky modifier
//            bits 3:0 = which modifier
//     '101' - unicode
//            bits 10:8 = code page
//                '000' U+22xx Math
//                '001' U+21xx Math arrows
//                '010' U+03xx Greek
//                '011' U+00xx Normal
//            bits 7:0 = codepoint<7:0>
//
#define IS_MODIFIER(k) ((k) & 0x8000)
#define IS_NORMAL(k)   ((k) & 0x4000)
#if HAVE_TAPPERS
#define IS_TAPPING(k)  (((k) & 0x3800) == 0x0800)
#endif
#define IS_MODKEY(k)   (((k) & 0x3800) == 0x1000)
#define IS_MEDIA(k)    (((k) & 0x3800) == 0x1800)
#if HAVE_STICKIES
#define IS_STICKY(k)   (((k) & 0x3800) == 0x2000)
#endif
#define IS_UNICODE(k)  (((k) & 0x3800) == 0x2800)

static uint16_t codepage[8] = {
    0x2200,
    0x2100,
    0x0300,
    0x0000,
    0,
    0,
    0,
    0,
};

#define MODIFIER(m) (0x8000 | (1 << (m)))
#if HAVE_TAPPERS
#define TAP(k,m)    (0x0800 | ((m) << 7) | (KEY_##k & 0x7f))
#endif
#define MODKEY(k,m) (0x1000 | (k) | ((m) << 7))
#define MEDIA(k)    (0x1800 | (k))
#if HAVE_STICKIES
#define STICKY(m)   (0x2000 | (m))
#endif
#define UNICODE(p,c) (0x2800 | ((p) << 8) | (c))
#define MOD(m)      MODIFIERKEY_##m

#define KEY_LAYER0  MODIFIER(LAYER0)
#define KEY_LAYER1  MODIFIER(LAYER1)
#define KEY_LAYER2  MODIFIER(LAYER2)
#define KEY_LAYER3  MODIFIER(LAYER3)

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
#define LALT MODIFIERKEY_LEFT_ALT
#define RALT MODIFIERKEY_RIGHT_ALT
#define LGUI MODIFIERKEY_LEFT_GUI
#define RGUI MODIFIERKEY_RIGHT_GUI

#define PREV_TRK   MEDIA(KEY_MEDIA_PREV_TRACK)
#define NEXT_TRK   MEDIA(KEY_MEDIA_NEXT_TRACK)
#define PLAY_PAUSE MEDIA(KEY_MEDIA_PLAY_PAUSE)
#define MUTE       MEDIA(KEY_MEDIA_MUTE)
#define VOL_INC    MEDIA(KEY_MEDIA_VOLUME_INC)
#define VOL_DEC    MEDIA(KEY_MEDIA_VOLUME_DEC)
#define BRIGHT_DEC  KEY_F14
#define BRIGHT_INC  KEY_F15

#define PAGE_MATH_SYMBOL 0
#define PAGE_MATH_ARROW  1
#define PAGE_GREEK       2
#define PAGE_NORMAL      3

#define ARROW_LR    UNICODE(PAGE_MATH_ARROW,  0x94)
#define ARROW_L     UNICODE(PAGE_MATH_ARROW,  0x90)
#define ARROW_R     UNICODE(PAGE_MATH_ARROW,  0x92)
#define MATH_AND    UNICODE(PAGE_MATH_SYMBOL, 0x27)
#define MATH_OR     UNICODE(PAGE_MATH_SYMBOL, 0x28)
#define MATH_NOT    UNICODE(PAGE_NORMAL,      0xAC)
#define MATH_FORALL UNICODE(PAGE_MATH_SYMBOL, 0x00)
#define MATH_EXISTS UNICODE(PAGE_MATH_SYMBOL, 0x03)
#define MATH_TSTILE UNICODE(PAGE_MATH_SYMBOL, 0xA2)

#define GRK_A       UNICODE(PAGE_GREEK, 0xb1)
#define GRK_B       UNICODE(PAGE_GREEK, 0xb2)
#define GRK_C       UNICODE(PAGE_GREEK, 0xb3)
#define GRK_D       UNICODE(PAGE_GREEK, 0xb4)
#define GRK_E       UNICODE(PAGE_GREEK, 0xb5)
#define GRK_F       UNICODE(PAGE_GREEK, 0xc6)
#define GRK_G       UNICODE(PAGE_GREEK, 0xc2)
#define GRK_H       UNICODE(PAGE_GREEK, 0xb7)
#define GRK_I       UNICODE(PAGE_GREEK, 0xb9)
#define GRK_J       UNICODE(PAGE_GREEK, 0xbe)
#define GRK_K       UNICODE(PAGE_GREEK, 0xba)
#define GRK_L       UNICODE(PAGE_GREEK, 0xbb)
#define GRK_M       UNICODE(PAGE_GREEK, 0xbc)
#define GRK_N       UNICODE(PAGE_GREEK, 0xbd)
#define GRK_O       UNICODE(PAGE_GREEK, 0xbf)
#define GRK_P       UNICODE(PAGE_GREEK, 0xc0)
#define GRK_Q       UNICODE(PAGE_GREEK, 0xb8)
#define GRK_R       UNICODE(PAGE_GREEK, 0xc1)
#define GRK_S       UNICODE(PAGE_GREEK, 0xc3)
#define GRK_T       UNICODE(PAGE_GREEK, 0xc4)
#define GRK_U       UNICODE(PAGE_GREEK, 0xc5)
#define GRK_V       UNICODE(PAGE_GREEK, 0xc6)
#define GRK_W       UNICODE(PAGE_GREEK, 0xc9)
#define GRK_X       UNICODE(PAGE_GREEK, 0xc7)
#define GRK_Y       UNICODE(PAGE_GREEK, 0xc8)
#define GRK_Z       UNICODE(PAGE_GREEK, 0xb6)

static const uint16_t layers[][NUMKEYS] = {
    // Qwerty / Software Dvorak
    [0] =
    LAYER(
    KEY_RIGHT_BRACE, KEY_1,KEY_2,          KEY_3,         KEY_4,       KEY_5,         KEY_6,     KEY_7,      KEY_8,     KEY_9,          KEY_0,          KEY_QUOTE,
    KEY_TAB,    KEY_Q,     KEY_W,          KEY_E,         KEY_R,       KEY_T,         KEY_Y,     KEY_U,      KEY_I,     KEY_O,          KEY_P,          KEY_LEFT_BRACE,
    KEY_ESC,    KEY_A,     KEY_S,          KEY_D,         KEY_F,       KEY_G,         KEY_H,     KEY_J,      KEY_K,     KEY_L,          KEY_SEMICOLON,  KEY_BACKSLASH,
    LSHIFT,     KEY_Z,     KEY_X,          KEY_C,         KEY_V,       KEY_B,         KEY_N,     KEY_M,      KEY_COMMA, KEY_PERIOD,     KEY_SLASH,      RSHIFT,
                KEY_TILDE, 0,              KEY_LEFT,      KEY_RIGHT,                             KEY_DOWN,   KEY_UP,    KEY_MINUS,      KEY_EQUAL,
                                                                       LCTRL,  LALT,  LCTRL,
                           KEY_LAYER1,     KEY_BACKSPACE, KEY_ESC,     LGUI,          RGUI,      KEY_ENTER,  KEY_SPACE, KEY_LAYER1
    ),
    // Function key layer
    [1] =
    LAYER(
    VOL_INC,    KEY_F1,    KEY_F2,         KEY_F3,        KEY_F4,        KEY_F5,        KEY_F6,         KEY_F7,     KEY_F8,    KEY_F9,     KEY_F10,        NEXT_TRK,
    MUTE,       ARROW_LR,  ARROW_L,        ARROW_R,       MATH_TSTILE,   GRK_LAMBDA,    0,              KEY_7,      KEY_8,     KEY_9,      KEY_QUOTE,      PLAY_PAUSE,
    VOL_DEC,    MATH_AND,  MATH_OR,        MATH_EXISTS,   MATH_FORALL,   MATH_NOT,      0,              KEY_4,      KEY_5,     KEY_6,      KEY_RIGHT_CURL, PREV_TRK,
    LSHIFT,     0,         0,              0,             BRIGHT_DEC,    BRIGHT_INC,    0,              KEY_1,      KEY_2,     KEY_3,      KEY_ENTER,      RSHIFT,
                MATH_NOT,  0,              KEY_PAGE_UP,   KEY_PAGE_DOWN,                                KEY_HOME,   KEY_END,   KEY_E,      KEY_ENTER,
                                                                         0,       0,    0,
                           KEY_LAYER1,     0,             0,             0,             0,              0,          KEY_0,     KEY_LAYER1
    ),
};

#define NUM_LAYERS (sizeof(layers) / sizeof(layers[0]))

static uint32_t enabled_layers = (0 << 3) | (1 << 0);

static uint16_t find_key(uint8_t raw) {
    for(int layer = NUM_LAYERS-1; layer >= 0; --layer) {
        if (enabled_layers & (1 << layer)) {
            uint16_t keycode = layers[layer][raw];
            if (keycode) {
                return keycode;
            }
        }
    }
    return 0;
}

static void enable_layer(uint8_t layer) {
    enabled_layers |= (1<<layer);
}

static void disable_layer(uint8_t layer) {
    enabled_layers &= ~(1<<layer);
}

static void press_modifier(uint8_t mod) {
    if (mod < LAYER0) {
        keyboard_modifier_keys |= (1 << mod);
    } else {
        enable_layer(mod - LAYER0);
    }
}

static void release_modifier(uint8_t mod) {
    if (mod < LAYER0) {
        keyboard_modifier_keys &= ~(1 << mod);
    } else {
        disable_layer(mod - LAYER0);
    }
}

// decode raw keypresses and put in USB buffer or tapper buffer
static void decode() {
    // first resolve any tappers and modifiers - which may affect
    // the meaning of other keys and which layers are enabled
    for(int i = 0; i < raw_count; ++i) {
        uint8_t raw = raw_keys[i];
        boolean down = raw & 0x80;
        raw = raw & 0x7f;
        uint16_t keycode = find_key(raw);
#if HAVE_TAPPERS
        if (IS_NORMAL(keycode)) { // normal key
            if (down) {
                resolve_tappers(false, false);
            }
        } else if (IS_TAPPING(keycode)) {
            if (down) {
                uint8_t mod = (keycode >> 7) & 0xf;
                boolean right = (mod & 0x4) || (mod == LAYER1);
                resolve_tappers(true, right);
            }
        }
#endif
        if (IS_MODIFIER(keycode)) { // modifier key
            if (down) {
                enabled_layers         |= ((keycode >> LAYER0) & 0xf);
            } else {
                enabled_layers         &= ~((keycode >> LAYER0) & 0xf);
            }
        }
    }

    // now deal with any keys
    for(int i = 0; i < raw_count; ++i) {
        uint8_t raw = raw_keys[i];
        boolean down = raw & 0x80;
        raw = raw & 0x7f;
        uint16_t keycode = find_key(raw);

        // search layers for keycode
        if (IS_MODIFIER(keycode)) { // modifier key
            if (down) {
                keyboard_modifier_keys |= (keycode & 0xff);
            } else {
                keyboard_modifier_keys &= ~(keycode & 0xff);
            }
        } else if (IS_NORMAL(keycode)) { // normal key
#if HAVE_STICKIES
            resolve_stickies(down);
#endif
            if (down) {
                press_key(raw, keycode & 0x7f);
                if (IS_MODKEY(keycode)) {
                    uint8_t mod = (keycode >> 7) & 0xf;
                    press_modifier(mod);
                    press_key(raw, keycode & 0x7f);
                    send_keys();
                    release_modifier(mod);
                } else {
                    press_key(raw, keycode & 0x7f);
                }
            } else {
                release_key(raw);
            }
#if HAVE_TAPPERS
        } else if (IS_TAPPING(keycode)) {
            uint8_t mod = (keycode >> 7) & 0xf;
            uint8_t key = keycode & 0x7f;
            resolve_stickies(down);
            if (down) {
                press_tapper(raw, key, mod);
            } else {
                release_tapper(key, mod);
            }
#endif
#if HAVE_STICKIES
        } else if (IS_STICKY(keycode)) {
            uint8_t mod = keycode & 0xf;
            if (down) {
                press_sticky(mod);
            } else {
                release_sticky(mod);
            }
#endif
        } else if (IS_MEDIA(keycode)) {
            uint8_t media = keycode & 0xff;
            if (down) {
                keyboard_media_keys |= media;
            } else {
                keyboard_media_keys &= ~media;
            }
        } else if (IS_UNICODE(keycode)) {
            if (down) {
                uint8_t  page = (keycode >> 8) & 0x7;
                uint16_t codepoint = codepage[page] | (keycode & 0xff);
                send_unicode(codepoint);
            }
        } else {
            // ignore anything else
        }
    }
#if HAVE_TAPPERS
    update_tappers();
#endif
}

////////////////////////////////////////////////////////////////
// End
////////////////////////////////////////////////////////////////
