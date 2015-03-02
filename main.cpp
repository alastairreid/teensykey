#include "Arduino.h"
#include "usb_keyboard.h"

#define NUMCOLS 12
#define NUMROWS 4
#define NUMKEYS (NUMCOLS * NUMROWS)

// the setup function runs once when you press reset or power the board
void setup() {
    // columns are on pins 0 .. 11, active low
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
    pinMode(11,INPUT_PULLUP);

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
int raw_count = 0;
int raw_keys[NUMKEYS];

// Scan all keys for pressed keys
// Writes to raw_keys
void scan_keyboard() {
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

KEYCODE_TYPE layers[][NUMKEYS] = {
    // Dvorak
    {
    KEY_QUOTE,     KEY_COMMA, KEY_PERIOD, KEY_P,         KEY_Y,     0, 0, KEY_F,     KEY_G,    KEY_C,    KEY_R,  KEY_L,
    KEY_A,         KEY_O,     KEY_E,      KEY_U,         KEY_I,     0, 0, KEY_D,     KEY_H,    KEY_T,    KEY_N,  KEY_S,
    KEY_SEMICOLON, KEY_Q,     KEY_J,      KEY_K,         KEY_X,     0, 0, KEY_B,     KEY_M,    KEY_W,    KEY_V,  KEY_Z,
    KEY_ESC,       KEY_TAB,   KEY_TAB,    KEY_BACKSPACE, KEY_ENTER, 0, 0, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    },
    // Qwerty
    {
    KEY_Q,         KEY_W,     KEY_E,      KEY_R,         KEY_T,     0, 0, KEY_U,     KEY_I,    KEY_O,    KEY_P,  KEY_LEFT_BRACE,
    KEY_A,         KEY_S,     KEY_D,      KEY_F,         KEY_G,     0, 0, KEY_H,     KEY_J,    KEY_K,    KEY_L,  KEY_SEMICOLON,
    KEY_Z,         KEY_X,     KEY_C,      KEY_V,         KEY_B,     0, 0, KEY_N,     KEY_M,    KEY_COMMA,KEY_PERIOD,  KEY_SLASH,
    KEY_ESC,       KEY_TAB,   KEY_TAB,    KEY_BACKSPACE, KEY_ENTER, 0, 0, KEY_SPACE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
    }
};

int current_layer = 1;

// decode and send keys to USB
void decode() {
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
    // whatever we have, send it
    // todo: only do this if something changed since last time
    // to avoid swamping USB
    usb_keyboard_send();
}

// the loop function runs over and over again forever
void loop() {
    scan_keyboard();
    decode();
    delay(10); // sample at 100Hz
}
