
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

#include "emu.h"
#include "media.h"

extern "C" {
#include "nofrendo/osd.h"
#include "nofrendo/event.h"
};
#include "math.h"

//#define calc_palettes	//enabling this will overwrite the palette values at boot time

using namespace std;

// https://wiki.nesdev.com/w/index.php/NTSC_video
// NES/SMS have pixel rates of 5.3693175, or 2/3 color clock
// in 3 phase mode each pixel gets 2 DAC values written, 2 color clocks = 3 nes pixels

uint32_t nes_3_phase[64] = {
    0x2C2C2C00,0x241D2400,0x221D2600,0x1F1F2700,0x1D222600,0x1D242400,0x1D262200,0x1F271F00,
    0x22261D00,0x24241D00,0x26221D00,0x271F1F00,0x261D2200,0x14141400,0x14141400,0x14141400,
    0x38383800,0x2C252C00,0x2A252E00,0x27272F00,0x252A2E00,0x252C2C00,0x252E2A00,0x272F2700,
    0x2A2E2500,0x2C2C2500,0x2E2A2500,0x2F272700,0x2E252A00,0x1F1F1F00,0x15151500,0x15151500,
    0x45454500,0x3A323A00,0x37333C00,0x35353C00,0x33373C00,0x323A3A00,0x333C3700,0x353C3500,
    0x373C3300,0x3A3A3200,0x3C373300,0x3C353500,0x3C333700,0x2B2B2B00,0x16161600,0x16161600,
    0x45454500,0x423B4200,0x403B4400,0x3D3D4500,0x3B404400,0x3B424200,0x3B444000,0x3D453D00,
    0x40443B00,0x42423B00,0x44403B00,0x453D3D00,0x443B4000,0x39393900,0x17171700,0x17171700,
};
/*uint32_t nes_4_phase[64] = {
    0x2C2C2C2C,0x241D1F26,0x221D2227,0x1F1D2426,0x1D1F2624,0x1D222722,0x1D24261F,0x1F26241D,
    0x2227221D,0x24261F1D,0x26241D1F,0x27221D22,0x261F1D24,0x14141414,0x14141414,0x14141414,
    0x38383838,0x2C25272E,0x2A252A2F,0x27252C2E,0x25272E2C,0x252A2F2A,0x252C2E27,0x272E2C25,
    0x2A2F2A25,0x2C2E2725,0x2E2C2527,0x2F2A252A,0x2E27252C,0x1F1F1F1F,0x15151515,0x15151515,
    0x45454545,0x3A33353C,0x3732373C,0x35333A3C,0x33353C3A,0x32373C37,0x333A3C35,0x353C3A33,
    0x373C3732,0x3A3C3533,0x3C3A3335,0x3C373237,0x3C35333A,0x2B2B2B2B,0x16161616,0x16161616,
    0x45454545,0x423B3D44,0x403B4045,0x3D3B4244,0x3B3D4442,0x3B404540,0x3B42443D,0x3D44423B,
    0x4045403B,0x42443D3B,0x44423B3D,0x45403B40,0x443D3B42,0x39393939,0x17171717,0x17171717,
};*/

//RGB palette from http://drag.wootest.net/misc/palgen.html -> YUV -> QAM on color carrier -> 4 phases sampled
uint32_t nes_4_phase[64] = {
	0x29292929,0x1B0F1521,0x1D0D1625,0x1C0E1624,0x16162121,0x101E281B,0x0F1F2919,0x101C2317,
	0x14171815,0x1A191516,0x1D1B1617,0x1D1B1617,0x1C17151B,0x14141414,0x14141414,0x14141414,
	0x42424242,0x34191A35,0x2F111A38,0x23152C39,0x171E3B34,0x0E28442B,0x0B304520,0x15313C20,
	0x212E2D1F,0x2D2B1B1E,0x322D191F,0x332B1921,0x36231A2D,0x14141414,0x14141414,0x14141414,
	0x5F5F5F5F,0x4F2C2649,0x41293048,0x382D3F49,0x333A564F,0x2F415745,0x2D48563C,0x2C515632,
	0x36514A2F,0x424D382D,0x4D46282F,0x553C1F39,0x58342044,0x21212121,0x14141414,0x14141414,
	0x5F5F5F5F,0x58494655,0x51464954,0x4D485055,0x4C4F5B58,0x4B525B54,0x4A555B50,0x4A595B4C,
	0x4D5A564A,0x52584F49,0x5754484B,0x5B50444F,0x5C4D4454,0x45454545,0x14141414,0x14141414
};


// PAL yuyv table, must be in RAM
/*uint32_t _nes_yuv_4_phase_pal[] = {
    0x31313131,0x2D21202B,0x2720252D,0x21212B2C,0x1D23302A,0x1B263127,0x1C293023,0x202B2D22,
    0x262B2722,0x2C2B2122,0x2F2B1E23,0x31291F27,0x30251F2A,0x18181818,0x19191919,0x19191919,
    0x3D3D3D3D,0x34292833,0x2F282D34,0x29283334,0x252B3732,0x232E392E,0x2431382B,0x28333429,
    0x2D342F28,0x33342928,0x3732252A,0x392E232E,0x382B2431,0x24242424,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x42373540,0x3C373B40,0x36374040,0x3337433F,0x3139433B,0x323D4338,0x35414237,
    0x3B423D35,0x41413736,0x453F3238,0x473C313B,0x4639323F,0x2F2F2F2F,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x48413D45,0x42404345,0x3D3F4644,0x3B3D4543,0x3B3E4542,0x3B42453F,0x3E47463E,
    0x434A453E,0x46483E3D,0x4843393E,0x4A403842,0x4B403944,0x3E3E3E3E,0x1B1B1B1B,0x1B1B1B1B,
    //odd
    0x31313131,0x20212D2B,0x2520272D,0x2B21212C,0x30231D2A,0x31261B27,0x30291C23,0x2D2B2022,
    0x272B2622,0x212B2C22,0x1E2B2F23,0x1F293127,0x1F25302A,0x18181818,0x19191919,0x19191919,
    0x3D3D3D3D,0x28293433,0x2D282F34,0x33282934,0x372B2532,0x392E232E,0x3831242B,0x34332829,
    0x2F342D28,0x29343328,0x2532372A,0x232E392E,0x242B3831,0x24242424,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x35374240,0x3B373C40,0x40373640,0x4337333F,0x4339313B,0x433D3238,0x42413537,
    0x3D423B35,0x37414136,0x323F4538,0x313C473B,0x3239463F,0x2F2F2F2F,0x1A1A1A1A,0x1A1A1A1A,
    0x49494949,0x3D414845,0x43404245,0x463F3D44,0x453D3B43,0x453E3B42,0x45423B3F,0x46473E3E,
    0x454A433E,0x3E48463D,0x3943483E,0x38404A42,0x39404B44,0x3E3E3E3E,0x1B1B1B1B,0x1B1B1B1B,
};*/

//RGB palette from http://drag.wootest.net/misc/palgen.html -> YUV -> QAM on color carrier -> 4 phases sampled
uint32_t _nes_yuv_4_phase_pal[] = {
	0x1B1B1B1B,0x110B0E14,0x120A0F16,0x120B0F16,0x0F0F1515,0x0C131912,0x0C141911,0x0C121610,
	0x0D0F0F0E,0x11100E0F,0x12110F0F,0x12110F0F,0x120F0E11,0x0D0D0D0D,0x0D0D0D0D,0x0D0D0D0D,
	0x2C2C2C2C,0x20131321,0x1D0E1322,0x17111C23,0x12152420,0x0D1A291C,0x0C1E2916,0x111F2417,
	0x171D1C16,0x1C1B1315,0x1F1C1315,0x201C1317,0x2118131D,0x0D0D0D0D,0x0D0D0D0D,0x0D0D0D0D,
	0x3F3F3F3F,0x31201C2E,0x291E212D,0x2520292E,0x24283633,0x232B362D,0x212F3628,0x21333623,
	0x25332F22,0x2B302620,0x302C1D21,0x34271926,0x36231A2C,0x15151515,0x0D0D0D0D,0x0D0D0D0D,
	0x3F3F3F3F,0x39313038,0x35303137,0x33313537,0x34353B3A,0x33373B38,0x32383B35,0x323A3B33,
	0x343A3832,0x36393432,0x39373133,0x3A352F34,0x3B332F37,0x2E2E2E2E,0x0D0D0D0D,0x0D0D0D0D,
	
	0x1B1B1B1B,0x0E0B1114,0x0F0A1216,0x0F0B1216,0x150F0F15,0x19130C12,0x19140C11,0x16120C10,
	0x0F0F0D0E,0x0E10110F,0x0F11120F,0x0F11120F,0x0E0F1211,0x0D0D0D0D,0x0D0D0D0D,0x0D0D0D0D,
	0x2C2C2C2C,0x13132021,0x130E1D22,0x1C111723,0x24151220,0x291A0D1C,0x291E0C16,0x241F1117,
	0x1C1D1716,0x131B1C15,0x131C1F15,0x131C2017,0x1318211D,0x0D0D0D0D,0x0D0D0D0D,0x0D0D0D0D,
	0x3F3F3F3F,0x1C20312E,0x211E292D,0x2920252E,0x36282433,0x362B232D,0x362F2128,0x36332123,
	0x2F332522,0x26302B20,0x1D2C3021,0x19273426,0x1A23362C,0x15151515,0x0D0D0D0D,0x0D0D0D0D,
	0x3F3F3F3F,0x30313938,0x31303537,0x35313337,0x3B35343A,0x3B373338,0x3B383235,0x3B3A3233,
	0x383A3432,0x34393632,0x31373933,0x2F353A34,0x2F333B37,0x2E2E2E2E,0x0D0D0D0D,0x0D0D0D0D
};

#ifdef calc_palettes
// RGB palette from http://drag.wootest.net/misc/palgen.html
static const uint8_t _nes_r[] = {
	0x46,0x00,0x00,0x02,0x35,0x57,0x5A,0x41,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x9D,0x00,0x05,0x57,0x9F,0xCC,0xCF,0xA4,0x5C,0x0B,0x00,0x00,0x00,0x00,0x00,0x00,
	0xFE,0x1F,0x53,0x98,0xFC,0xFF,0xFF,0xFF,0xC4,0x71,0x28,0x00,0x00,0x2B,0x00,0x00,
	0xFE,0x9E,0xAF,0xD0,0xFE,0xFF,0xFF,0xFF,0xE7,0xC5,0xA6,0x94,0x92,0xA7,0x00,0x00
};
static const uint8_t _nes_g[] = {
	0x46,0x06,0x06,0x06,0x03,0x00,0x00,0x00,0x02,0x14,0x1E,0x1E,0x15,0x00,0x00,0x00,
	0x9D,0x4A,0x30,0x18,0x07,0x02,0x0B,0x23,0x3F,0x58,0x66,0x67,0x5E,0x00,0x00,0x00,
	0xFF,0x9E,0x76,0x65,0x67,0x6C,0x74,0x80,0x9A,0xB3,0xC4,0xC8,0xBF,0x2B,0x00,0x00,
	0xFF,0xD5,0xC0,0xB8,0xBF,0xC0,0xC3,0xCA,0xD5,0xDF,0xE6,0xE8,0xE4,0xA7,0x00,0x00
};
static const uint8_t _nes_b[] = {
	0x46,0x5A,0x78,0x73,0x4C,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x21,0x00,0x00,0x00,
	0x9D,0xB9,0xE1,0xDA,0xA7,0x55,0x00,0x00,0x00,0x00,0x00,0x13,0x6E,0x00,0x00,0x00,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xB3,0x66,0x14,0x00,0x00,0x21,0x74,0xD0,0x2B,0x00,0x00,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xE0,0xBD,0x9C,0x8B,0x8E,0xA3,0xC5,0xEB,0xA7,0x00,0x00
};

//Makes four samples (one cycle) using QAM modulation on the color carrier for NTSC from a RGB palette
void make_ntsc_palette()
{
	float ofs = 20;		//black level
    float amp = 75;		//signal span
    float hue = M_PI;	//hue correction if any...
	float saturation = 0.8;	//Color saturation
	for (int j = 0; j < 64; j++)
	{
		float y = (0.299 * _nes_r[j] + 0.587 * _nes_g[j] + 0.114 * _nes_b[j])/255.f;
		float u = (-0.147407 * _nes_r[j] - 0.289391 * _nes_g[j] + 0.436798 * _nes_b[j])/255.f;
		float v = (0.614777 * _nes_r[j] - 0.514799 * _nes_g[j] - 0.099978 * _nes_b[j])/255.f;
		float wt = 0;
		uint32_t sampl = 0;
		for (int i = 3; i >= 0; i--)
		{
			sampl |= (uint32_t) round(ofs + amp * (y + saturation * (u * sinf(wt+hue) + v * cosf(wt+hue)))) << (i*8);
			wt += M_PI/2;
		}
		nes_4_phase[j] = sampl;
		//printf("0x%08x\n", sampl);
	}
}

//Makes four samples (one cycle) using QAM modulation on the color carrier for PAL (even/odd lines) from a RGB palette
void make_pal_palette()
{
	float ofs = 13;		//black level
    float amp = 50;		//signal span
    float hue = M_PI;	//hue correction if any...
	float saturation = 0.6;	//Color saturation
	for (int j = 0; j < 64; j++)
	{
		float y = (0.299 * _nes_r[j] + 0.587 * _nes_g[j] + 0.114 * _nes_b[j])/255.f;
		float u = (-0.147407 * _nes_r[j] - 0.289391 * _nes_g[j] + 0.436798 * _nes_b[j])/255.f;
		float v = (0.614777 * _nes_r[j] - 0.514799 * _nes_g[j] - 0.099978 * _nes_b[j])/255.f;
		float wt = 0;
		uint32_t e_sampl = 0;
		uint32_t o_sampl = 0;
		for (int i = 3; i >= 0; i--)
		{
			e_sampl |= (uint32_t) round(ofs + amp * (y + saturation * (u * sinf(wt+hue) + v * cosf(wt+hue)))) << (i*8);
			o_sampl |= (uint32_t) round(ofs + amp * (y + saturation * (u * sinf(wt-hue) - v * cosf(wt-hue)))) << (i*8);
			wt += M_PI/2;
		}
		_nes_yuv_4_phase_pal[j] = e_sampl;
		_nes_yuv_4_phase_pal[j+64] = o_sampl;
		//printf("0x%08x  0x%08x\n", e_sampl, o_sampl);
	}
}
#endif

uint8_t* _nofrendo_rom = 0;
extern "C"
char *osd_getromdata()
{
    return (char *)_nofrendo_rom;
}

extern "C"
int nes_emulate_init(const char* path, int width, int height);

extern "C"
uint8_t** nes_emulate_frame(bool draw_flag);

static void (*nes_sound_cb)(void *buffer, int length) = 0;

extern uint32_t nes_pal[256];

const char* _nes_help[] = {
    "Keyboard:",
    "  Arrow Keys - D-Pad",
    "  Left Shift - Button A",
    "  Option     - Button B",
    "  Return     - Start",
    "  Tab        - Select",
    "",
    "Wiimote (held sideways):",
    "  +          - Start",
    "  -          - Select",
    "  + & -      - Reset",
    "  A,1        - Button A",
    "  B,2        - Button B",
    0
};

const char* _nes_ext[] = {
    "nes",
    0
};

int _audio_frequency;
extern "C"
void osd_getsoundinfo(sndinfo_t *info)
{
    info->sample_rate = _audio_frequency;
    info->bps = 8;
}

extern "C"
void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    nes_sound_cb = playfunc;
}

std::string to_string(int i);
class EmuNofrendo : public Emu {
    uint8_t** _lines;
public:
    EmuNofrendo(int ntsc) : Emu("nofrendo",256,240,ntsc,(16 | (1 << 8)),4,EMU_NES)    // audio is 16bit, 3 or 6 cc width
    {
        _lines = 0;
        _ext = _nes_ext;
        _help = _nes_help;
        _audio_frequency = audio_frequency;
		gen_palettes();
    }

    virtual void gen_palettes()
    {
#ifdef calc_palettes
		make_ntsc_palette();
        make_pal_palette();
#endif
    }

    virtual int info(const string& file, vector<string>& strs)
    {
        string ext = get_ext(file);
        uint8_t hdr[15];
        int len = Emu::head(file,hdr,sizeof(hdr));
        string name = file.substr(file.find_last_of("/") + 1);
        strs.push_back(name);
        strs.push_back(::to_string(len/1024) + "k NES Cartridge");
        strs.push_back("");
        if (hdr[0] == 'N' && hdr[1] == 'E' && hdr[2] == 'S') {
            int prg = hdr[4] * 16;
            int chr = hdr[5] * 8;
            int mapper = (hdr[6] >> 4) | (hdr[7] & 0xF0);
            char buf[64];
            sprintf(buf,"MAP:%d",mapper);
            strs.push_back(buf);
            sprintf(buf,"PRG:%dk",prg);
            strs.push_back(buf);
            sprintf(buf,"CHR:%dk",chr);
            strs.push_back(buf);
        }
        return 0;
    }

    void pad(int pressed, int index)
    {
        event_t e = event_get(index);
        e(pressed);
    }

    enum {
        event_joypad1_up_ = 1,
        event_joypad1_down_ = 2,
        event_joypad1_left_ = 4,
        event_joypad1_right_ = 8,
        event_joypad1_start_ = 16,
        event_joypad1_select_ = 32,
        event_joypad1_a_ = 64,
        event_joypad1_b_ = 128,

        event_soft_reset_ = 256,
        event_hard_reset_ = 512
    };

    const int _nes_1[11] = {
        event_joypad1_up,
        event_joypad1_down,
        event_joypad1_left,
        event_joypad1_right,
        event_joypad1_start,
        event_joypad1_select,
        event_joypad1_a,
        event_joypad1_b,

        event_soft_reset,
        event_hard_reset,
        0
    };

    const int _nes_2[9] = {
        event_joypad2_up,
        event_joypad2_down,
        event_joypad2_left,
        event_joypad2_right,
        event_joypad2_start,
        event_joypad2_select,
        event_joypad2_a,
        event_joypad2_b,
        0
    };

    // Rotated 90%
    const uint32_t _common_nes[16] = {
        0,  // msb
        0,
        0,
        event_joypad1_start_,       // PLUS
        event_joypad1_left_,        // UP
        event_joypad1_right_,       // DOWN
        event_joypad1_up_,          // RIGHT
        event_joypad1_down_,        // LEFT

        0, // HOME
        0,
        0,
        event_joypad1_select_,  // MINUS
        event_joypad1_a_,      // A
        event_joypad1_b_,      // B
        event_joypad1_b_,      // ONE
        event_joypad1_a_,      // TWO
    };

    const uint32_t _classic_nes[16] = {
        event_joypad1_right_,    // RIGHT
        event_joypad1_down_,     // DOWN
        0,                       // LEFT_TOP
        event_joypad1_select_,    // MINUS
        0,                        // HOME
        event_joypad1_start_,     // PLUS
        0,                    // RIGHT_TOP
        0,

        0,                  // LOWER_LEFT
        event_joypad1_b_,   // B
        0,                  // Y
        event_joypad1_a_,   // A
        0,                  // X
        0,                  // LOWER_RIGHT
        event_joypad1_left_, // LEFT
        event_joypad1_up_   // UP
    };

    const uint32_t _generic_nes[16] = {
        0,                  // GENERIC_OTHER   0x8000
        0,                  // GENERIC_FIRE_X  0x4000  // RETCON
        0,                  // GENERIC_FIRE_Y  0x2000
        0,                  // GENERIC_FIRE_Z  0x1000

        event_joypad1_a_,      //GENERIC_FIRE_A  0x0800
        event_joypad1_b_,      //GENERIC_FIRE_B  0x0400
        0,                      //GENERIC_FIRE_C  0x0200
        0,                      //GENERIC_RESET   0x0100     // ATARI FLASHBACK

        event_joypad1_start_,   //GENERIC_START   0x0080
        event_joypad1_select_,  //GENERIC_SELECT  0x0040
        event_joypad1_a_,      //GENERIC_FIRE    0x0020
        event_joypad1_right_,  //GENERIC_RIGHT   0x0010

        event_joypad1_left_,   //GENERIC_LEFT    0x0008
        event_joypad1_down_,   //GENERIC_DOWN    0x0004
        event_joypad1_up_,      //GENERIC_UP      0x0002
        0,                      //GENERIC_MENU    0x0001
    };

    // raw HID data. handle WII/IR mappings
    virtual void hid(const uint8_t* d, int len)
    {
        if (d[0] != 0x32 && d[0] != 0x42)
            return;
        bool ir = *d++ == 0x42;

        for (int i = 0; i < 2; i++) {
            uint32_t p;
            if (ir) {
                int m = d[0] + (d[1] << 8);
                p = generic_map(m,_generic_nes);
                d += 2;
            } else
                p = wii_map(i,_common_nes,_classic_nes);

            // reset on select + start held at the same time
            if ((p & event_joypad1_select_) && (p & event_joypad1_start_))
                pad(1,event_soft_reset);

            const int* m = i ? _nes_2 : _nes_1;
            for (int e = 0; m[e]; e++)
            {
                pad((p & 1),m[e]);
                p >>= 1;
            }
        }
    }

    /*
     Return - Joypad 1 Start
     Tab - Joypad 1 Select
     */
    virtual void key(int keycode, int pressed, int mods)
    {
        switch (keycode) {
            case 82: pad(pressed,event_joypad1_up); break;
            case 81: pad(pressed,event_joypad1_down); break;
            case 80: pad(pressed,event_joypad1_left); break;
            case 79: pad(pressed,event_joypad1_right); break;

            case 21: pad(pressed,event_soft_reset); break; // soft reset - 'r'
            case 23: pad(pressed,event_hard_reset); break; // hard reset - 't'

            case 61: pad(pressed,event_joypad1_start); break; // F4
            case 62: pad(pressed,((KEY_MOD_LSHIFT|KEY_MOD_RSHIFT) & mods) ? event_hard_reset : event_soft_reset); break; // F5

            case 40: pad(pressed,event_joypad1_start); break; // return
            case 43: pad(pressed,event_joypad1_select); break; // tab

            case 225: pad(pressed,event_joypad1_a); break; // left shift key event_joypad1_a
            case 226: pad(pressed,event_joypad1_b); break; // option key event_joypad1_b

            //case 59: system(pressed,INPUT_PAUSE); break; // F2
            //case 61: system(pressed,INPUT_START); break; // F4
            //case 62: system(pressed,((KEY_MOD_LSHIFT|KEY_MOD_RSHIFT) & mods) ? INPUT_HARD_RESET : INPUT_SOFT_RESET); break; // F5
        }
    }

    virtual int insert(const std::string& path, int flags, int disk_index)
    {
        unmap_file(_nofrendo_rom);
        _nofrendo_rom = 0;
        printf("nofrendo inserting %s\n",path.c_str());

        uint8_t h[16];
        int len = head(path,h,sizeof(h));
        if (len < 0) {
            printf("nofrendo can't open %s\n",path.c_str());
            return -1;
        }

        printf("nofrendo %s is %d bytes\n",path.c_str(),len);
        _nofrendo_rom = map_file(path.c_str(),len);
        if (!_nofrendo_rom) {
            printf("nofrendo can't map %s\n",path.c_str());
            return -1;
        }

        nes_emulate_init(path.c_str(),width,height);
        _lines = nes_emulate_frame(true);   // first frame!
        return 0;
    }

    virtual int update()
    {
        if (_nofrendo_rom)
            _lines = nes_emulate_frame(true);
        return 0;
    }

    virtual uint8_t** video_buffer()
    {
        return _lines;
    }
    
    virtual int audio_buffer(int16_t* b, int len)
    {
        int n = frame_sample_count();
        if (nes_sound_cb) {
            nes_sound_cb(b,n);  // 8 bit unsigned
            uint8_t* b8 = (uint8_t*)b;
            for (int i = n-1; i >= 0; i--)
                b[i] = (b8[i] ^ 0x80) << 8;  // turn it back into signed 16
        }
        else
            memset(b,0,2*n);
        return n;
    }

    virtual const uint32_t* ntsc_palette() { return cc_width == 3 ? nes_3_phase : nes_4_phase; };
    virtual const uint32_t* pal_palette() { return _nes_yuv_4_phase_pal; };
    virtual const uint32_t* rgb_palette() { return nes_pal; };

    virtual int make_default_media(const string& path)
    {
        unpack((path + "/sokoban.nes").c_str(),sokoban_nes,sizeof(sokoban_nes));
        unpack((path + "/chase.nes").c_str(),chase_nes,sizeof(chase_nes));
        unpack((path + "/tokumaru_raycast.nes").c_str(),tokumaru_raycast_nes,sizeof(tokumaru_raycast_nes));
        return 0;
    }
};

Emu* NewNofrendo(int ntsc)
{
    return new EmuNofrendo(ntsc);
}


