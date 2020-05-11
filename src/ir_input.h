
/* Copyright (c) 2010-2020, Peter Barrett
 **
 ** Permission to use, copy, modify, and/or distribute this software for
 ** any purpose with or without fee is hereby granted, provided that the
 ** above copyright notice and this permission notice appear in all copies.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 ** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
 ** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
 ** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 ** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 ** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 ** SOFTWARE.
 */

#ifndef __ir_input__
#define __ir_input__

// generate events on state changes of IR pin, feed to state machines.
// event timings  have a resolution of HSYNC, timing is close enough between 15720 and 15600 to make this work
// poll to synthesize hid events at every frame
// i know there is a perfectly good peripheral for this in the ESP32 but this seems more fun somehow

#define WEBTV_KEYBOARD
#define RETCON_CONTROLLER
#define FLASHBACK_CONTROLLER
//#define APPLE_TV_CONTROLLER

uint8_t _ir_last = 0;
uint8_t _ir_count = 0;
uint8_t _keyDown = 0;
uint8_t _keyUp = 0;

void IRAM_ATTR ir_event(uint8_t ticks, uint8_t value); // t is HSYNCH ticks, v is value

inline void IRAM_ATTR ir_sample()
{
    uint8_t ir = (GPIO.in & (1 << IR_PIN)) != 0;
    if (ir != _ir_last)
    {
        ir_event(_ir_count,_ir_last);
        _ir_count = 0;
        _ir_last = ir;
    }
    if (_ir_count != 0xFF)
        _ir_count++;
}

class IRState {
public:
    uint8_t _state = 0;
    uint16_t _code = 0;
    uint16_t _output = 0;
    uint16_t _joy[2] = {0};
    uint16_t _joy_last[2] = {0};
    uint8_t _timer[2] = {0};

    void set(int i, int m, int t)
    {
        uint8_t b = 0;
        if ((m & (GENERIC_LEFT | GENERIC_RIGHT)) == (GENERIC_LEFT | GENERIC_RIGHT))
            b++; // bogus?
        if ((m & (GENERIC_UP | GENERIC_DOWN)) == (GENERIC_UP | GENERIC_DOWN))
            b++; // bogus?
        if (b) {
            printf("bogus:%04X\n",m);
            return;
        }

        _joy[i] = m;
        _timer[i] = t;
    }

    // make a fake hid event
    int get_hid(uint8_t* dst)
    {
        if (_timer[0] && !--_timer[0])
            _joy[0] = 0;
        if (_timer[1] && !--_timer[1])
            _joy[1] = 0;
        if ((_joy[0] != _joy_last[0]) || (_joy[1] != _joy_last[1])) {
            _joy_last[0] = _joy[0];
            _joy_last[1] = _joy[1];
            dst[0] = 0xA1;
            dst[1] = 0x42;
            dst[2] = _joy[0];
            dst[3] = _joy[0]>>8;
            dst[4] = _joy[1];
            dst[5] = _joy[1]>>8;
            return 6;           // only sent if it changes
        }
        return 0;
    }
};

//==========================================================
//==========================================================
// Apple remote NEC code
// pretty easy to adapt to any NEC remote

#ifdef APPLE_TV_CONTROLLER

// Silver apple remote, 7 Bit code
// should work with both silvers and white
#define APPLE_MENU      0x40
#define APPLE_PLAY      0x7A
#define APPLE_CENTER    0x3A
#define APPLE_RIGHT     0x60
#define APPLE_LEFT      0x10
#define APPLE_UP        0x50
#define APPLE_DOWN      0x30

#define APPLE_RELEASE   0x20 // sent after menu and play?

//    generic repeat code
#define NEC_REPEAT    0xAAAA

/*
  9ms preamble ~142
  4.5ms 1 ~71 - start
  2.25ms ~35 - repeat

  32 bits
  0 – a 562.5µs/562.5µs   9ish wide
  1 – a 562.5µs/1.6875ms  27ish wide
*/

IRState _apple;
int get_hid_apple(uint8_t* dst)
{
    if (_apple._output)
    {
        if (_apple._output != NEC_REPEAT)
            _keyDown = (_apple._output >> 8) & 0x7F;  // 7 bit code
        _apple._output = 0;

        uint16_t k = 0;
        switch (_keyDown) {
            case APPLE_UP:     k = GENERIC_UP;     break;
            case APPLE_DOWN:   k = GENERIC_DOWN;   break;
            case APPLE_LEFT:   k = GENERIC_LEFT;   break;
            case APPLE_RIGHT:  k = GENERIC_RIGHT;  break;
            case APPLE_CENTER: k = GENERIC_FIRE;   break;
            case APPLE_MENU:   k = GENERIC_RESET;  break;
            case APPLE_PLAY:   k = GENERIC_SELECT; break;
        }
        _apple.set(0,k,15); // 108ms repeat period
    }
    return _apple.get_hid(dst);
}

// NEC codes used by apple remote
void IRAM_ATTR ir_apple(uint8_t t, uint8_t v)
{
  if (!v) {
    if (t > 32)
      _apple._state = 0;
  } else {
    if (t < 32)
    {
      _apple._code <<= 1;
      if (t >= 12)
        _apple._code |= 1;
      if (++_apple._state == 32)
        _apple._output = _apple._code;    // Apple code in bits 14-8*
    } else {
        if (t > 32 && t < 40 && !_apple._state)  // Repeat 2.25ms pulse 4.5ms start
          _apple._output = NEC_REPEAT;
        _apple._state = 0;
    }
  }
}

#endif

//==========================================================
//==========================================================
//  Atari Flashback 4 wireless controllers

#ifdef FLASHBACK_CONTROLLER

// HSYNCH period is 44/315*455 or 63.55555..us
// 18 bit code 1.87khz clock
// 2.3ms zero preamble  // 36
// 0 is 0.27ms pulse    // 4
// 1 is 0.80ms pulse    // 13

// Keycodes...
// Leading bit is 1 for player 1 control..

#define PREAMBLE(_t) (_t >= 34 && _t <= 38)
#define SHORT(_t)    (_t >= 2 && _t <= 6)
#define LONG(_t)     (_t >= 11 && _t <= 15)

// Codes come in pairs 33ms apart
// Sequence repeats every 133ms
// bitmap is released if no code for 10 vbls (167ms) or 0x01 (p1) / 0x0F (p2)
// up to 12 button bits, 4 bits of csum/p1/p2 indication

//  called at every loop ~60Hz

IRState _flashback;
int get_hid_flashback(uint8_t* dst)
{
    if (_flashback._output)
    {
        uint16_t m = _flashback._output >> 4;        // 12 bits of buttons
        printf("F:%04X\n",m);
        uint8_t csum = _flashback._output & 0xF;     // csum+1 for p1, csum-1 for p2
        uint8_t s = m + (m >> 4) + (m >> 8);
        if (((s+1) & 0xF) == csum)
            _flashback.set(0,m,15);
        else if (((s-1) & 0xF) == csum)
            _flashback.set(1,m,20);
        _flashback._output = 0;
    }
    return _flashback.get_hid(dst);
}

void IRAM_ATTR ir_flashback(uint8_t t, uint8_t v)
{
  if (_flashback._state == 0)
  {
    if (PREAMBLE(t) && (v == 0))  // long 0, rising edge of start bit
    {
      _flashback._state = 1;
    }
  }
  else
  {
    if (v)
    {
      _flashback._code <<= 1;
      if (LONG(t))
      {
        _flashback._code |= 1;
      }
      else if (!SHORT(t))
      {
        _flashback._state = 0;  // framing error
        return;
      }

      if (++_flashback._state == 19)
      {
        _flashback._output = _flashback._code;
        _flashback._state = 0;
      }
    }
    else
    {
      if (!SHORT(t))
        _flashback._state = 0;  // Framing err
    }
  }
}

#endif

//==========================================================
//==========================================================
// RETCON controllers
// 75ms keyboard repeat
// Preamble is 0.80ms low, 0.5 high
// Low: 0.57ms=0,0.27,s=1, high 0.37
// 16 bits
// Preamble = 800,500/63.55555 ~ 12.6,7.87
// LOW0 = 8.97
// LOW1 = 4.25
// HIGH = 5.82

#ifdef RETCON_CONTROLLER

// number of 63.55555 cycles per bit
#define PREAMBLE_L(_t) (_t >= 12 && _t <= 14) // 12/13/14 preamble
#define PREAMBLE_H(_t) (_t >= 6 && _t <= 10)  // 8
#define LOW_0(_t)     (_t >= 8 && _t <= 10)   // 8/9/10
#define LOW_1(_t)     (_t >= 4 && _t <= 6)    // 4/5/6

// map retcon to generic
const uint16_t _jmap[] = {
  0x0400, GENERIC_UP,
  0x0200, GENERIC_DOWN,
  0x0100, GENERIC_LEFT,
  0x0080, GENERIC_RIGHT,

  0x1000, GENERIC_SELECT,
  0x0800, GENERIC_START,

  0x0020, GENERIC_FIRE_X,
  0x0040, GENERIC_FIRE_Y,
  0x0002, GENERIC_FIRE_Z,

  0x2000, GENERIC_FIRE_A,
  0x4000, GENERIC_FIRE_B,
  0x0008, GENERIC_FIRE_C,
};

IRState _retcon;
int get_hid_retcon(uint8_t* dst)
{
    if (_retcon._output)
    {
        uint16_t m = 0;
        const uint16_t* jmap = _jmap;
        int16_t i = 12;
        uint16_t k = _retcon._output;
        _retcon._output = 0;
        while (i--)
        {
            if (jmap[0] & k)
                m |= jmap[1];
            jmap += 2;
        }
        printf("R:%04X\n",m);
        _retcon.set(k >> 15,m,20);
    }
    return _retcon.get_hid(dst);
}

void IRAM_ATTR ir_retcon(uint8_t t, uint8_t v)
{
  if (_retcon._state == 0)
  {
    if (v == 0)  {   // start bit
      if (PREAMBLE_L(t))
        _retcon._state = 1;
    }
  }
  else
  {
    if (!v)
    {
      _retcon._code <<= 1;
      if (LOW_1(t))
        _retcon._code |= 1;
      if (_retcon._state++ == 16)
      {
        _retcon._output = _retcon._code;
        _retcon._state = 0;
      }
    }
  }
}

#endif


//==========================================================
//==========================================================
//  Webtv keyboard
#ifdef WEBTV_KEYBOARD

#define BAUDB   12  // Width of uart bit in HSYNCH
#define WT_PREAMBLE(_t) (_t >= 36 && _t <= 40)   // 3.25 baud
#define SHORTBIT(_t) (_t >= 9 && _t <= 13)     // 1.5ms ish

// converts webtv keyboard to common scancodes
const uint8_t _ir2scancode[128] = {
    0x00, // 00
    0x00, // 02
    0x05, // 04 B
    0x00, // 06
    0x00, // 08
    0x51, // 0A Down
    0x00, // 0C
    0x00, // 0E
    0x00, // 10
    0x50, // 12 Left
    0xE6, // 14 Right Alt
    0x38, // 16 /
    0xE2, // 18 Left Alt
    0x4F, // 1A Right
    0x2C, // 1C Space
    0x11, // 1E N
    0x32, // 20 #
    0x00, // 22
    0x22, // 24 5
    0x41, // 26 F8
    0x3B, // 28 F2
    0xE4, // 2A Right Ctrl
    0x00, // 2C
    0x2E, // 2E =
    0x3A, // 30 F1
    0x4A, // 32 Home
    0x00, // 34
    0x2D, // 36 -
    0xE0, // 38 Left Ctrl
    0x35, // 3A `
    0x42, // 3C F9
    0x23, // 3E 6
    0x00, // 40
    0x00, // 42
    0x19, // 44 V
    0x37, // 46 .
    0x06, // 48 C
    0x68, // 4A F13
    0xE5, // 4C Right Shift
    0x36, // 4E ,
    0x1B, // 50 X
    0x4D, // 52 End
    0x00, // 54
    0x00, // 56
    0x1D, // 58 Z
    0x00, // 5A
    0x28, // 5C Return
    0x10, // 5E M
    0x00, // 60
    0xE7, // 62 Right GUI
    0x09, // 64 F
    0x0F, // 66 L
    0x07, // 68 D
    0x4E, // 6A PageDown
    0x00, // 6C
    0x0E, // 6E K
    0x16, // 70 S
    0x4B, // 72 PageUp
    0x00, // 74
    0x33, // 76 ;
    0x04, // 78 A
    0x00, // 7A
    0x31, // 7C |
    0x0D, // 7E J
    0x00, // 80
    0x00, // 82
    0x17, // 84 T
    0x40, // 86 F7
    0x3C, // 88 F3
    0x00, // 8A
    0xE1, // 8C Left Shift
    0x30, // 8E ]
    0x39, // 90 CapsLock
    0x00, // 92
    0x29, // 94 Escape
    0x2F, // 96 [
    0x2B, // 98 Tab
    0x00, // 9A
    0x2A, // 9C Backspace
    0x1C, // 9E Y
    0x00, // A0
    0x00, // A2
    0x21, // A4 4
    0x26, // A6 9
    0x20, // A8 3
    0x44, // AA F11
    0x00, // AC
    0x25, // AE 8
    0x1F, // B0 2
    0x00, // B2
    0x46, // B4 PrintScreen
    0x27, // B6 0
    0x1E, // B8 1
    0x45, // BA F12
    0x43, // BC F10
    0x24, // BE 7
    0x00, // C0
    0x00, // C2
    0x0A, // C4 G
    0x00, // C6
    0x3D, // C8 F4
    0x00, // CA
    0x00, // CC
    0x00, // CE
    0x3E, // D0 F5
    0x52, // D2 Up
    0xE3, // D4 Left GUI
    0x34, // D6 '
    0x29, // D8 Escape
    0x48, // DA Pause
    0x3F, // DC F6
    0x0B, // DE H
    0x00, // E0
    0x00, // E2
    0x15, // E4 R
    0x12, // E6 O
    0x08, // E8 E
    0x00, // EA
    0x00, // EC
    0x0C, // EE I
    0x1A, // F0 W
    0x00, // F2
    0x53, // F4 Numlock
    0x13, // F6 P
    0x14, // F8 Q
    0x00, // FA
    0x00, // FC
    0x18, // FE U
};


// IR Keyboard State
uint8_t _state = 0;
uint16_t _code = 0;
uint8_t _wt_keys[6] = {0};
uint8_t _wt_expire[6] = {0};
uint8_t _wt_modifiers = 0;

static
uint8_t parity_check(uint8_t k)
{
    uint8_t c;
    uint8_t v = k;
    for (c = 0; v; c++)
      v &= v-1;
    return (c & 1) ? k : 0;
}

// make a mask from modifier keys
static
uint8_t ctrl_mask(uint8_t k)
{
    switch (k) {
        case 0x38:  return KEY_MOD_LCTRL;
        case 0x8C:  return KEY_MOD_LSHIFT;
        case 0x18:  return KEY_MOD_LALT;
        case 0xD4:  return KEY_MOD_LGUI;
        case 0x2A:  return KEY_MOD_RCTRL;
        case 0x4C:  return KEY_MOD_RSHIFT;
        case 0x14:  return KEY_MOD_RALT;
        case 0x62:  return KEY_MOD_RGUI;
    }
    return 0;
}

// update state of held keys
// produce a hid keyboard record
int get_hid_webtv(uint8_t* dst)
{
    bool dirty = false;
    uint8_t k = parity_check(_keyUp);
    _keyUp = 0;
    if (k) {
       _wt_modifiers &= ~ctrl_mask(k);
        for (int i = 0; i < 6; i++) {
            if (_wt_keys[i] == k) {
                _wt_expire[i] = 1;  // key will expire this frame
                break;
            }
        }
    }

    k = parity_check(_keyDown);
    _keyDown = 0;
    if (k) {
        _wt_modifiers |= ctrl_mask(k);

        // insert key into list of pressed keys
        int j = 0;
        for (int i = 0; i < 6; i++) {
            if ((_wt_keys[i] == 0) || (_wt_expire[i] == 0) || (_wt_keys[i] == k)) {
                j = i;
                break;
            }
            if (_wt_expire[i] < _wt_expire[j])
                j = i;
        }
        if (_wt_keys[j] != k) {
            _wt_keys[j] = k;
            dirty = true;
        }
        _wt_expire[j] = 8;  // key will be down for ~130ms or 8 frames
    }

    // generate hid keyboard events if anything was pressed or changed...
    // A1 01 mods XX k k k k k k
    dst[0] = 0xA1;
    dst[1] = 0x01;
    dst[2] = _wt_modifiers;
    dst[3] = 0;
    int j = 0;
    for (int i = 0; i < 6; i++) {
        dst[4+i] = 0;
        if (_wt_expire[i]) {
            if (!--_wt_expire[i])
                dirty = true;
        }
        if (_wt_expire[i] == 0) {
            _wt_keys[i] = 0;
        } else {
            dst[4 + j++] = _ir2scancode[_wt_keys[i] >> 1];
        }
    }
    return dirty ? 10 : 0;
}

// WebTV UART like keyboard protocol
// 3.25 bit 0 start preamble the 19 bits
// 10 bit code for keyup, keydown, all keys released etc
// 7 bit keycode + parity bit.
//

#define KEYDOWN     0x4A
#define KEYUP       0x5E
void IRAM_ATTR ir_webtv(uint8_t t, uint8_t v)
{
  if (_state == 0)
  {
    if (WT_PREAMBLE(t) && (v == 0))  // long 0, rising edge of start bit
      _state = 1;
  }
  else if (_state == 1)
  {
    _state = (SHORTBIT(t) && (v == 1)) ? 2 : 0;
  }
  else
  {
      t += BAUDB>>1;
      uint8_t bits = _state-2;
      while ((t > BAUDB) && (bits < 16))
      {
          t -= BAUDB;
          _code = (_code << 1) | v;
          bits++;
      }
      if (bits == 16)
      {
        v = t <= BAUDB;
        uint8_t md = _code >> 8;
        _code |= v;                 // Low bit of code is is parity
        if (md == KEYDOWN)
            _keyDown = _code;
        else if (md == KEYUP)
            _keyUp = _code;
        _state = 0;                 // got one
        return;
      }
      _state = bits+2;
    }
}
#endif

// called from interrupt
void IRAM_ATTR ir_event(uint8_t t, uint8_t v)
{
#ifdef WEBTV_KEYBOARD
    ir_webtv(t,v);
#endif
#ifdef RETCON_CONTROLLER
    ir_retcon(t,v);
#endif
#ifdef APPLE_TV_CONTROLLER
    ir_apple(t,v);
#endif
#ifdef FLASHBACK_CONTROLLER
    ir_flashback(t,v);
#endif
}

// called every frame from emu
int get_hid_ir(uint8_t* dst)
{
    int n = 0;
#ifdef APPLE_TV_CONTROLLER
    if (n = get_hid_apple(dst))
        return n;
#endif
#ifdef RETCON_CONTROLLER
    if (n = get_hid_retcon(dst))
        return n;
#endif
#ifdef FLASHBACK_CONTROLLER
    if (n = get_hid_flashback(dst))
        return n;
#endif
    #ifdef WEBTV_KEYBOARD
        return get_hid_webtv(dst);
    #endif
    return 0;
}
#endif
