
/* Copyright (c) 2020, Peter Barrett
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

#ifndef emu_hpp
#define emu_hpp

#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "dirent.h"

#include <string>
#include <vector>
#include <map>

#include "hid_server/hid_server.h"
#include "../config.h"

#define EMU_ATARI 1
#define EMU_NES 2
#define EMU_NES6 4
#define EMU_SMS 3

extern "C"
void* MALLOC32(int size, const char* name);

class Emu {
public:

    // hid modifiers
    #define KEY_MOD_LCTRL  0x01
    #define KEY_MOD_LSHIFT 0x02
    #define KEY_MOD_LALT   0x04
    #define KEY_MOD_LGUI   0x08
    #define KEY_MOD_RCTRL  0x10
    #define KEY_MOD_RSHIFT 0x20
    #define KEY_MOD_RALT   0x40
    #define KEY_MOD_RGUI   0x80

    #define KEY_MOD_CTRL   (KEY_MOD_LCTRL|KEY_MOD_RCTRL)
    #define KEY_MOD_SHIFT  (KEY_MOD_LSHIFT|KEY_MOD_RSHIFT)
    #define KEY_MOD_ALT    (KEY_MOD_LALT|KEY_MOD_RALT)
    #define KEY_MOD_GUI    (KEY_MOD_LGUI|KEY_MOD_RGUI)

    // generic joystick, ir for Atari Flashback and RETCON
    #define GENERIC_OTHER   0x8000

    #define GENERIC_FIRE_X  0x4000  // RETCON
    #define GENERIC_FIRE_Y  0x2000
    #define GENERIC_FIRE_Z  0x1000
    #define GENERIC_FIRE_A  0x0800
    #define GENERIC_FIRE_B  0x0400
    #define GENERIC_FIRE_C  0x0200

    #define GENERIC_RESET   0x0100     // ATARI FLASHBACK
    #define GENERIC_START   0x0080
    #define GENERIC_SELECT  0x0040
    #define GENERIC_FIRE    0x0020
    #define GENERIC_RIGHT   0x0010
    #define GENERIC_LEFT    0x0008
    #define GENERIC_DOWN    0x0004
    #define GENERIC_UP      0x0002
    #define GENERIC_MENU    0x0001

    std::string name;
    const char** _ext;
    const char** _help;
    uint8_t** _lines;

    int width;
    int height;
    int standard; // ntsc = 1

    int audio_frequency;
    int audio_frame_samples;
    int audio_fraction;
    int audio_format;

    int cc_width;           // number of samples per color clock
    int flavor;             // color flavor (cleaner?);

    Emu(const char* n, int w, int h, int standard, int aformat, int cc, int flavor);
    virtual ~Emu();

    virtual void gen_palettes() = 0;

    int frame_sample_count();   // # of audio samples for next frame (standard dependent)

    virtual int make_default_media(const std::string& path) = 0;

    virtual int insert(const std::string& path, int flags = 1, int disk_index = 0) = 0;
    static int load(const std::string& path, uint8_t** data, int* len);
    static int head(const std::string& path, uint8_t* data, int len);
    virtual int info(const std::string& file, std::vector<std::string>& strs) { return -1; };

    virtual void hid(const uint8_t* d, int len) {};
    virtual void key(int keycode, int pressed, int mod) {};

    virtual int update() = 0;
    virtual uint8_t** video_buffer() = 0;
    virtual int audio_buffer(int16_t* b, int max_len) = 0;

    virtual const uint32_t* ntsc_palette() { return NULL; };
    virtual const uint32_t* pal_palette() { return NULL; };
    virtual const uint32_t* rgb_palette() { return NULL; };
    virtual const uint32_t* composite_palette();
};

void gui_start(Emu* emu, const char* path);
void gui_hid(const uint8_t* hid, int len);  // Parse HID event
void gui_update();
void gui_key(int keycode, int pressed, int mod);

extern "C"
void gui_msg(const char* msg);         // temporarily display a msg

// for loading carts
std::string get_ext(const std::string& s);
extern "C" uint8_t* map_file(const char* path, int len);
extern "C" void unmap_file(uint8_t* ptr);
extern "C" FILE* mkfile(const char* path);
extern "C" int unpack(const char* dst_path, const uint8_t* d, int len);

void audio_write_16(const int16_t* s, int len, int channels);
int get_hid_ir(uint8_t* dst);
uint32_t generic_map(uint32_t m, const uint32_t* target);

Emu* NewAtari800(int ntsc = 1);
Emu* NewNofrendo(int ntsc = 1);
Emu* NewSMSPlus(int ntsc = 1);

#endif /* emu_hpp */
