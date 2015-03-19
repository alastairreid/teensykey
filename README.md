# TeensyKey

Keyboard firmware for Teensy 3.1

This firmware runs on the Teensy 3.1 microcontroller (and maybe other
Arduino platforms?) and supports a range of features suited to
[Atreus](http://atreus.technomancy.us) like keyboards such as:

* Tapping modifiers
  Tapping the key produces a keystroke like space or return.
  Holding the key and hitting a normal key (within the timeout period)
  makes the key act like a modifier.

* Sticky modifier keys
  The shift keys can act like normal shift keys if held down and then
  a normal key is hit.  Or, a single tap causes the next key to be shifted.
  Or a double tap acts like caps-lock.

* Layers
  Keys can have more than the normal two meanings (eg 'a' and 'A' or
  '1' and '!').  Layer modifiers select the meanings.
  The layers currently implemented include a layer of punctuation keys
  and a layer of number keys (and media keys).
  This is necessary because the Atreus only has 42 keys but normal
  keyboards have 80-100 keys.

These ideas are based on similar features found in the [Atreus
firmware](https://github.com/technomancy/atreus-firmware), in
the [TMK Keyboard firmware](https://github.com/tmk/tmk_keyboard)
and in [Edgar Matias's Half
Qwerty](http://edgarmatias.com/papers/hci96/index.html).  I made my own
implementation because I was having trouble porting TMK and Atreus to
the Teensy 3.0.

## Future directions

* It is traditional to put the keymap in a separate .h file and select the
  keymap at build time.

* The software does a lot of busy waits.  It should go into a low power sleep
  mode instead.  (I want to get a power meter first so that I can see if this
  matters.)

* I keep thinking about adding a mini joystick and sending mouse move commands.
  I am not sure if there is any point unless I also have mouse buttons though.

* The binary is far larger than it should be.  I need to be smarter about which
  parts of the Teensy library I link in.
