
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
#include "math.h"

extern "C" {
#include "atari800/libatari800.h"
#include "atari800/sound.h"
#include "atari800/akey.h"
#include "atari800/memory.h"
}


using namespace std;
string get_ext(const string& s);
std::string to_string(int i);

static
int gamma_(float v, float g)
{
    if (v < 0)
        return 0;
    v = powf(v,g);
    if (v <= 0.0031308)
        return 0;
    int c = ((1.055 * powf(v, 1.0/2.4) - 0.055)*255);
    return c < 255 ? c : 255;
}


// make_yuv_palette from RGB palette
void make_yuv_palette(const char* name, const uint32_t* rgb, int len)
{
    uint32_t pal[256*2];
    uint32_t* even = pal;
    uint32_t* odd = pal + len;

    float chroma_scale = BLANKING_LEVEL/2/256;
    //chroma_scale /= 127;  // looks a little washed out
    chroma_scale /= 80;
    for (int i = 0; i < len; i++) {
        uint8_t r = rgb[i] >> 16;
        uint8_t g = rgb[i] >> 8;
        uint8_t b = rgb[i];

        float y = 0.299 * r + 0.587*g + 0.114 * b;
        float u = -0.147407 * r - 0.289391 * g + 0.436798 * b;
        float v =  0.614777 * r - 0.514799 * g - 0.099978 * b;
        y /= 255.0;
        y = (y*(WHITE_LEVEL-BLACK_LEVEL) + BLACK_LEVEL)/256;

        uint32_t e = 0;
        uint32_t o = 0;
        for (int i = 0; i < 4; i++) {
            float p = 2*M_PI*i/4 + M_PI;
            float s = sin(p)*chroma_scale;
            float c = cos(p)*chroma_scale;
            uint8_t e0 = round(y + (s*u) + (c*v));
            uint8_t o0 = round(y + (s*u) - (c*v));
            e = (e << 8) | e0;
            o = (o << 8) | o0;
        }
        *even++ = e;
        *odd++ = o;
    }

    printf("uint32_t %s_4_phase_pal[] = {\n",name);
    for (int i = 0; i < len*2; i++) {  // start with luminance map
        printf("0x%08X,",pal[i]);
        if ((i & 7) == 7)
            printf("\n");
        if (i == (len-1)) {
            printf("//odd\n");
        }
    }
    printf("};\n");

    /*
     // don't bother with phase tables
    printf("uint8_t DRAM_ATTR %s[] = {\n",name);
    for (int i = 0; i < len*(1<<PHASE_BITS)*2; i++) {
        printf("0x%02X,",yuv[i]);
        if ((i & 15) == 15)
            printf("\n");
        if (i == (len*(1<<PHASE_BITS)-1)) {
            printf("//odd\n");
        }
    }
    printf("};\n");
     */
}

// atari pal palette cc_width==4
static void make_atari_4_phase_pal(const uint16_t* lum, const float* angle)
{
    uint32_t _pal4[256*2];
    uint32_t *even = _pal4;
    uint32_t *odd = even + 256;

    float chroma_scale = BLANKING_LEVEL/2/256;
    chroma_scale *= 1.5;
    float s,c,u,v;

    for (int cr = 0; cr < 16; cr++) {
        if (cr == 0) {
            u = v = 0;
        } else {
            u = cos(angle[16-cr]);
            v = sin(angle[16-cr]);
        }
        for (int lm = 0; lm < 16; lm++) {
            uint32_t e = 0;
            uint32_t o = 0;
            for (int i = 0; i < 4; i++) {
                float p = 2*M_PI*i/4;
                s = sin(p)*chroma_scale;
                c = cos(p)*chroma_scale;
                uint8_t e0 = round(lum[lm] + (s*u) + (c*v));
                uint8_t o0 = round(lum[lm] + (s*u) - (c*v));
                e = (e << 8) | e0;
                o = (o << 8) | o0;
            }
            *even++ = e;
            *odd++ = o;
        }
    }

    printf("uint32_t atari_4_phase_pal[] = {\n");
    for (int i = 0; i < 256*2; i++) {  // start with luminance map
        printf("0x%08X,",_pal4[i]);
        if ((i & 7) == 7)
            printf("\n");
        if (i == 255) {
            printf("//odd\n");
        }
    }
    printf("};\n");
}

static int PIN(int x)
{
    if (x < 0) return 0;
    if (x > 255) return 255;
    return x;
}

static uint32_t rgb(int r, int g, int b)
{
    return (PIN(r) << 16) | (PIN(g) << 8) | PIN(b);
}

// lifted from atari800
static void make_atari_rgb_palette(uint32_t* palette)
{
    float start_angle =  (303.0f) * M_PI / 180.0f;
    float start_saturation = 0;
    float color_diff = 26.8 * M_PI / 180.0; // color_delay
    float scaled_black_level = (double)16 / 255.0;
    float scaled_white_level = (double)235 / 255.0;
    float contrast = 0;
    float brightness = 0;

    float luma_mult[16]={
        0.6941, 0.7091, 0.7241, 0.7401,
        0.7560, 0.7741, 0.7931, 0.8121,
        0.8260, 0.8470, 0.8700, 0.8930,
        0.9160, 0.9420, 0.9690, 1.0000};

    uint32_t linear[256];
    uint16_t _lum[16];
    float _angle[16];

    for (int cr = 0; cr < 16; cr ++) {
        float angle = start_angle + (cr - 1) * color_diff;
        float saturation = (cr ? (start_saturation + 1) * 0.175f: 0.0);
        float i = cos(angle) * saturation;
        float q = sin(angle) * saturation;

        _angle[cr] = angle + 2*M_PI*33/360;

        for (int lm = 0; lm < 16; lm ++) {
            float y = (luma_mult[lm] - luma_mult[0]) / (luma_mult[15] - luma_mult[0]);
            _lum[lm] = (y*(WHITE_LEVEL-BLACK_LEVEL) + BLACK_LEVEL)/256;

            y *= contrast * 0.5 + 1;
            y += brightness * 0.5;
            y = y * (scaled_white_level - scaled_black_level) + scaled_black_level;

            float gmma = 2.35;
            float r = (y +  0.946882*i +  0.623557*q);
            float g = (y + -0.274788*i + -0.635691*q);
            float b = (y + -1.108545*i +  1.709007*q);
            linear[(cr << 4) | lm] = rgb(r*255,g*255,b*255);
            palette[(cr << 4) | lm] = (gamma_(r,gmma) << 16) | (gamma_(g,gmma) << 8) | (gamma_(b,gmma) << 0);
        }
    }
    make_atari_4_phase_pal(_lum,_angle);
}

void make_atari_rgb_palette()
{
    uint32_t _atari_pal[256];
    make_atari_rgb_palette(_atari_pal);
    for (int i = 0; i < 256; i++) {
        printf("0x%08X,",_atari_pal[i]);
        if ((i & 7) == 7)
            printf("\n");
    }
}

// derived from atari800
static void atari_palette(float start_angle = 0)
{
    float color_diff = 28.6 * M_PI / 180.0;
    int cr, lm;
    float luma_mult[16]={
        0.6941, 0.7091, 0.7241, 0.7401,
        0.7560, 0.7741, 0.7931, 0.8121,
        0.8260, 0.8470, 0.8700, 0.8930,
        0.9160, 0.9420, 0.9690, 1.0000};

    int i = 0;
    uint32_t pal[256];
    printf("const uint32_t atari_4_phase_ntsc[256] = {\n");
    for (cr = 0; cr < 16; cr ++) {
        float angle = start_angle + ((15-cr) - 1) * color_diff;

        for (lm = 0; lm < 16; lm ++) {
            //double y = luma_mult[lm]*lm*(WHITE_LEVEL-BLACK_LEVEL)/15 + BLACK_LEVEL;
            double y = lm*(WHITE_LEVEL-BLACK_LEVEL)/15 + BLACK_LEVEL;
            int p[4];
            for (int j = 0; j < 4; j++)
                p[j] = y;
            for (int j = 0; j < 4; j++)
                p[j] += sin(angle + 2*M_PI*j/4) * (cr ? BLANKING_LEVEL/2 : 0);
            uint32_t pi = 0;
            for (int j = 0; j < 4; j++)
                pi = (pi << 8) | p[j] >> 8;
            pal[i++] = pi;
            printf("0x%08X,",pi);
            if ((lm & 7) == 7)
                printf("\n");
        }
    }
    printf("};\n");

    // swizzled for ESP32
    #define P0 (color >> 16)
    #define P1 (color >> 8)
    #define P2 (color)
    #define P3 (color << 8)

    uint32_t color;
    // swizzed pattern for esp32 is 0 2 1 3
    printf("const uint32_t _atari_4_phase_esp[256] = {\n");
    for (int i = 0; i < 256; i++) {
        color = pal[i];
        color = (color & 0xFF0000FF) | ((color << 8) & 0x00FF0000) | ((color >> 8) & 0x0000FF00);
        printf("0x%08X,",color);
        if ((i & 7) == 7)
            printf("\n");
    }
    printf("};\n");
}


/*
 6502 MPU:
       MOS Technology MCS6502A or equivalent (most NTSC 400/800 machines)
       Atari SALLY (late NTSC 400/800, all PAL 400/800, and all XL/XE)

 CPU CLOCK RATE:
       1.7897725MHz (NTSC machines)
       1.7734470MHz (PAL/SECAM machines)

 FRAME REFRESH RATE:
       59.94Hz (NTSC machines)
       49.86Hz (PAL/SECAM machines)

 MACHINE CYCLES per FRAME:
       29859 (NTSC machines) (1.7897725MHz / 59.94Hz)
       35568 (PAL/SECAM machines) (1.7734470MHz / 49.86Hz)

 SCAN LINES per FRAME
       262 (NTSC machines)
       312 (PAL/SECAM machines)

 MACHINE CYCLES per SCAN LINE
       114        (NTSC machines: 29859 cycles/frame / 262 lines/frame;
              PAL/SECAM machines: 35568 cycles/frame / 312 lines/frame)

 COLOR CLOCKS per MACHINE CYCLE
       2

 COLOR CLOCKS per SCAN LINE
       228  (2 color clocks/machine cycle * 114 machine cycles/scan line)

 MAXIMUM SCAN LINE WIDTH = "WIDE PLAYFIELD"
       176 color clocks

 MAXIMUM RESOLUTION = GRAPHICS PIXEL
       0.5 color clock

 MAXIMUM HORIZONTAL FRAME RESOLUTION
       352 pixels (176 color clocks / 0.5 color clock)

 MAXIMUM VERTICAL FRAME RESOLUTION
       240 pixels (240 scan lines per frame)
 */

typedef struct {
    int id;
    int size;
    const char* machine;
    const char* name;
} cart_info;

const cart_info _cart_info[] = {
    { 0,0,"","" },
    { 1,8,"800/XL/XE","Standard 8 KB cartridge" },
    { 2,16,"800/XL/XE","Standard 16 KB cartridge" },
    { 3,16,"800/XL/XE","OSS two chip 16 KB cartridge (034M)" },
    { 4,32,"5200","Standard 32 KB 5200 cartridge" },
    { 5,32,"800/XL/XE","DB 32 KB cartridge" },
    { 6,16,"5200","Two chip 16 KB 5200 cartridge" },
    { 7,40,"5200","Bounty Bob Strikes Back 40 KB 5200 cartridge" },
    { 8,64,"800/XL/XE","64 KB Williams cartridge" },
    { 9,64,"800/XL/XE","Express 64 KB cartridge" },
    { 10,64,"800/XL/XE","Diamond 64 KB cartridge" },
    { 11,64,"800/XL/XE","SpartaDOS X 64 KB cartridge" },
    { 12,32,"800/XL/XE","XEGS 32 KB cartridge" },
    { 13,64,"800/XL/XE","XEGS 64 KB cartridge" },
    { 14,128,"800/XL/XE","XEGS 128 KB cartridge" },
    { 15,16,"800/XL/XE","OSS one chip 16 KB cartridge" },
    { 16,16,"5200","One chip 16 KB 5200 cartridge" },
    { 17,128,"800/XL/XE","Atrax 128 KB cartridge" },
    { 18,40,"800/XL/XE","Bounty Bob Strikes Back 40 KB cartridge" },
    { 19,8,"5200","Standard 8 KB 5200 cartridge" },
    { 20,4,"5200","Standard 4 KB 5200 cartridge" },
    { 21,8,"800","Right slot 8 KB cartridge" },
    { 22,32,"800/XL/XE","32 KB Williams cartridge" },
    { 23,256,"800/XL/XE","XEGS 256 KB cartridge" },
    { 24,512,"800/XL/XE","XEGS 512 KB cartridge" },
    { 25,1024,"800/XL/XE","XEGS 1 MB cartridge" },
    { 26,16,"800/XL/XE","MegaCart 16 KB cartridge" },
    { 27,32,"800/XL/XE","MegaCart 32 KB cartridge" },
    { 28,64,"800/XL/XE","MegaCart 64 KB cartridge" },
    { 29,128,"800/XL/XE","MegaCart 128 KB cartridge" },
    { 30,256,"800/XL/XE","MegaCart 256 KB cartridge" },
    { 31,512,"800/XL/XE","MegaCart 512 KB cartridge" },
    { 32,1024,"800/XL/XE","MegaCart 1 MB cartridge" },
    { 33,32,"800/XL/XE","Switchable XEGS 32 KB cartridge" },
    { 34,64,"800/XL/XE","Switchable XEGS 64 KB cartridge" },
    { 35,128,"800/XL/XE","Switchable XEGS 128 KB cartridge" },
    { 36,256,"800/XL/XE","Switchable XEGS 256 KB cartridge" },
    { 37,512,"800/XL/XE","Switchable XEGS 512 KB cartridge" },
    { 38,1024,"800/XL/XE","Switchable XEGS 1 MB cartridge" },
    { 39,8,"800/XL/XE","Phoenix 8 KB cartridge" },
    { 40,16,"800/XL/XE","Blizzard 16 KB cartridge" },
    { 41,128,"800/XL/XE","Atarimax 128 KB Flash cartridge" },
    { 42,1024,"800/XL/XE","Atarimax 1 MB Flash cartridge" },
    { 43,128,"800/XL/XE","SpartaDOS X 128 KB cartridge" },
    { 44,8,"800/XL/XE","OSS 8 KB cartridge" },
    { 45,16,"800/XL/XE","OSS two chip 16 KB cartridge (043M)" },
    { 46,4,"800/XL/XE","Blizzard 4 KB cartridge" },
    { 47,32,"800/XL/XE","AST 32 KB cartridge" },
    { 48,64,"800/XL/XE","Atrax SDX 64 KB cartridge" },
    { 49,128,"800/XL/XE","Atrax SDX 128 KB cartridge" },
    { 50,64,"800/XL/XE","Turbosoft 64 KB cartridge" },
    { 51,128,"800/XL/XE","Turbosoft 128 KB cartridge" },
    { 52,32,"800/XL/XE","Ultracart 32 KB cartridge" },
    { 53,8,"800/XL/XE","Low bank 8 KB cartridge" },
    { 54,128,"800/XL/XE","SIC! 128 KB cartridge" },
    { 55,256,"800/XL/XE","SIC! 256 KB cartridge" },
    { 56,512,"800/XL/XE","SIC! 512 KB cartridge" },
    { 57,2,"800/XL/XE","Standard 2 KB cartridge" },
    { 58,4,"800/XL/XE","Standard 4 KB cartridge" },
    { 59,4,"800","Right slot 4 KB cartridge" },
    { 60,32,"800/XL/XE","Blizzard 32 KB cartridge" },
};

//uint32_t _atari_pal[256];
const uint32_t atari_palette_rgb[256] = {
    0x00000000,0x000F0F0F,0x001B1B1B,0x00272727,0x00333333,0x00414141,0x004F4F4F,0x005E5E5E,
    0x00686868,0x00787878,0x00898989,0x009A9A9A,0x00ABABAB,0x00BFBFBF,0x00D3D3D3,0x00EAEAEA,
    0x00001600,0x000F2100,0x001A2D00,0x00273900,0x00334500,0x00405300,0x004F6100,0x005D7000,
    0x00687A00,0x00778A17,0x00899B29,0x009AAC3B,0x00ABBD4C,0x00BED160,0x00D2E574,0x00E9FC8B,
    0x001C0000,0x00271300,0x00331F00,0x003F2B00,0x004B3700,0x00594500,0x00675300,0x00756100,
    0x00806C12,0x008F7C22,0x00A18D34,0x00B29E45,0x00C3AF56,0x00D6C36A,0x00EAD77E,0x00FFEE96,
    0x002F0000,0x003A0000,0x00460F00,0x00521C00,0x005E2800,0x006C3600,0x007A4416,0x00885224,
    0x00925D2F,0x00A26D3F,0x00B37E50,0x00C48F62,0x00D6A073,0x00E9B487,0x00FDC89B,0x00FFDFB2,
    0x00390000,0x00440000,0x0050000A,0x005C0F17,0x00681B23,0x00752931,0x0084373F,0x0092464E,
    0x009C5058,0x00AC6068,0x00BD7179,0x00CE838A,0x00DF949C,0x00F2A7AF,0x00FFBBC3,0x00FFD2DA,
    0x00370020,0x0043002C,0x004E0037,0x005A0044,0x00661350,0x0074215D,0x0082306C,0x00903E7A,
    0x009B4984,0x00AA5994,0x00BC6AA5,0x00CD7BB6,0x00DE8CC7,0x00F1A0DB,0x00FFB4EF,0x00FFCBFF,
    0x002B0047,0x00360052,0x0042005E,0x004E006A,0x005A1276,0x00672083,0x00762F92,0x00843DA0,
    0x008E48AA,0x009E58BA,0x00AF69CB,0x00C07ADC,0x00D18CED,0x00E59FFF,0x00F9B3FF,0x00FFCAFF,
    0x0016005F,0x0021006A,0x002D0076,0x00390C82,0x0045198D,0x0053279B,0x006135A9,0x006F44B7,
    0x007A4EC2,0x008A5ED1,0x009B6FE2,0x00AC81F3,0x00BD92FF,0x00D0A5FF,0x00E4B9FF,0x00FBD0FF,
    0x00000063,0x0000006F,0x00140C7A,0x00201886,0x002C2592,0x003A329F,0x004841AE,0x00574FBC,
    0x00615AC6,0x00716AD6,0x00827BE7,0x00948CF8,0x00A59DFF,0x00B8B1FF,0x00CCC5FF,0x00E3DCFF,
    0x00000054,0x00000F5F,0x00001B6A,0x00002776,0x00153382,0x00234190,0x0031509E,0x00405EAC,
    0x004A68B6,0x005A78C6,0x006B89D7,0x007D9BE8,0x008EACF9,0x00A1BFFF,0x00B5D3FF,0x00CCEAFF,
    0x00001332,0x00001E3E,0x00002A49,0x00003655,0x00004261,0x0012506F,0x00205E7D,0x002F6D8B,
    0x00397796,0x004987A6,0x005B98B7,0x006CA9C8,0x007DBAD9,0x0091CEEC,0x00A5E2FF,0x00BCF9FF,
    0x00001F00,0x00002A12,0x0000351E,0x0000422A,0x00004E36,0x000B5B44,0x00196A53,0x00287861,
    0x0033826B,0x0043927B,0x0054A38C,0x0065B49E,0x0077C6AF,0x008AD9C2,0x009EEDD6,0x00B5FFED,
    0x00002400,0x00003000,0x00003B00,0x00004700,0x0000530A,0x00106118,0x001E6F27,0x002D7E35,
    0x00378840,0x00479850,0x0059A961,0x006ABA72,0x007BCB84,0x008FDE97,0x00A3F2AB,0x00BAFFC2,
    0x00002300,0x00002F00,0x00003A00,0x00004600,0x00115200,0x001F6000,0x002E6E00,0x003C7C12,
    0x0047871C,0x0057972D,0x0068A83E,0x0079B94F,0x008ACA61,0x009EDD74,0x00B2F189,0x00C9FFA0,
    0x00001B00,0x00002700,0x000F3200,0x001C3E00,0x00284A00,0x00365800,0x00446600,0x00527500,
    0x005D7F00,0x006D8F19,0x007EA02B,0x008FB13D,0x00A0C24E,0x00B4D662,0x00C8EA76,0x00DFFF8D,
    0x00110E00,0x001D1A00,0x00292500,0x00353100,0x00413D00,0x004F4B00,0x005D5A00,0x006B6800,
    0x0076720B,0x0085821B,0x0097932D,0x00A8A43E,0x00B9B650,0x00CCC963,0x00E0DD77,0x00F7F48F,
};

// swizzed ntsc palette in RAM
const uint32_t atari_4_phase_ntsc[256] = {
    0x18181818,0x1A1A1A1A,0x1C1C1C1C,0x1F1F1F1F,0x21212121,0x24242424,0x27272727,0x2A2A2A2A,
    0x2D2D2D2D,0x30303030,0x34343434,0x38383838,0x3B3B3B3B,0x40404040,0x44444444,0x49494949,
    0x1A15210E,0x1C182410,0x1E1A2612,0x211D2915,0x231F2B18,0x26222E1A,0x2925311D,0x2C283420,
    0x2F2B3723,0x322E3A27,0x36323E2A,0x3A36412E,0x3D394532,0x423D4936,0x46424E3A,0x4B46523F,
    0x151A210E,0x171D2310,0x191F2613,0x1C222815,0x1E242B18,0x21272E1B,0x242A311D,0x272D3420,
    0x2A303724,0x2E333A27,0x31373D2A,0x353A412E,0x393E4532,0x3D424936,0x41474D3A,0x464B523F,
    0x101F1F10,0x13212113,0x15232315,0x18262618,0x1A28281A,0x1D2B2B1D,0x202E2E20,0x23313123,
    0x26343426,0x29383729,0x2D3B3B2D,0x303F3F31,0x34434234,0x38474738,0x3D4B4B3D,0x41505041,
    0x0E211A15,0x10231D17,0x13261F19,0x1528221C,0x182B241F,0x1B2E2721,0x1D312A24,0x20342D27,
    0x2337302A,0x273A332E,0x2A3E3731,0x2E413A35,0x32453E39,0x3649423D,0x3A4D4741,0x3F524B46,
    0x0E21151A,0x1024181C,0x12261A1E,0x15291D21,0x182B1F24,0x1A2E2226,0x1D312529,0x2034282C,
    0x23372B2F,0x273A2E33,0x2A3E3236,0x2E41353A,0x3245393E,0x36493D42,0x3A4E4246,0x3F52464B,
    0x101F111E,0x12211320,0x15241623,0x17261825,0x1A291B28,0x1D2C1E2B,0x202F202E,0x23322331,
    0x26352634,0x29382A37,0x2C3B2D3B,0x303F313E,0x34433542,0x38473946,0x3C4B3D4A,0x4150424F,
    0x141B0E21,0x161D1023,0x19201326,0x1B221528,0x1E25182B,0x21281B2E,0x242A1E30,0x272E2133,
    0x2A312436,0x2D34273A,0x30372B3D,0x343B2E41,0x383F3245,0x3C433649,0x40473A4D,0x454C3F52,
    0x19160E21,0x1B181024,0x1E1B1226,0x201D1529,0x2320172B,0x26231A2E,0x29261D31,0x2C292034,
    0x2F2C2337,0x322F273A,0x35322A3E,0x39362E41,0x3D3A3245,0x413E3649,0x45423A4E,0x4A473F52,
    0x1E11101F,0x20141222,0x22161424,0x25191727,0x271B1929,0x2A1E1C2C,0x2D211F2F,0x30242232,
    0x33272535,0x362A2838,0x3A2E2C3C,0x3E323040,0x41353343,0x46393847,0x4A3E3C4C,0x4F424150,
    0x210E131C,0x2311161E,0x25131820,0x28161B23,0x2A181D26,0x2D1B2028,0x301E232B,0x3321262E,
    0x36242931,0x3A272C35,0x3D2B3038,0x412E333C,0x45323740,0x49363B44,0x4D3B4048,0x523F444D,
    0x210E1817,0x24101B19,0x26121D1B,0x29151F1E,0x2B172221,0x2E1A2523,0x311D2826,0x34202B29,
    0x37232E2C,0x3A263130,0x3E2A3533,0x422E3837,0x45313C3B,0x4936403F,0x4E3A4543,0x523F4948,
    0x200F1D12,0x22111F14,0x25142217,0x27162419,0x2A19271C,0x2D1C2A1F,0x2F1F2C22,0x32222F25,
    0x35253328,0x3928362B,0x3C2C392F,0x402F3D32,0x44334136,0x4837453A,0x4C3B493E,0x51404E43,
    0x1C13200F,0x1F152311,0x21172513,0x241A2816,0x261D2A19,0x291F2D1B,0x2C22301E,0x2F253321,
    0x32283624,0x352C3928,0x392F3D2B,0x3C33402F,0x40374433,0x443B4837,0x493F4D3B,0x4D445140,
    0x1818220E,0x1A1A2410,0x1C1C2612,0x1F1F2915,0x21212B17,0x24242E1A,0x2727311D,0x2A2A3420,
    0x2D2D3723,0x30303A26,0x34343E2A,0x3838422E,0x3B3B4531,0x40404A36,0x44444E3A,0x4949533F,
    0x131C200F,0x151F2311,0x17212513,0x1A242816,0x1D262A19,0x1F292D1B,0x222C301E,0x252F3321,
    0x28323624,0x2C353928,0x2F393D2B,0x333C402F,0x37404433,0x3B444837,0x3F494D3B,0x444D5140,
};
uint32_t *atari_4_phase_ntsc_ram = 0;

const uint32_t atari_4_phase_pal[] = {
    0x18181818,0x1B1B1B1B,0x1E1E1E1E,0x21212121,0x25252525,0x28282828,0x2B2B2B2B,0x2E2E2E2E,
    0x32323232,0x35353535,0x38383838,0x3B3B3B3B,0x3F3F3F3F,0x42424242,0x45454545,0x49494949,
    0x16271A09,0x192A1D0C,0x1C2D200F,0x1F302312,0x23342716,0x26372A19,0x293A2D1C,0x2C3D301F,
    0x30413423,0x33443726,0x36473A29,0x394A3D2C,0x3D4E4130,0x40514433,0x43544736,0x47584B3A,
    0x0F24210C,0x1227240F,0x152A2712,0x182D2A15,0x1C312E19,0x1F34311C,0x2237341F,0x253A3722,
    0x293E3B26,0x2C413E29,0x2F44412C,0x3247442F,0x364B4833,0x394E4B36,0x3C514E39,0x4055523D,
    0x0B1F2511,0x0E222814,0x11252B17,0x14282E1A,0x182C321E,0x1B2F3521,0x1E323824,0x21353B27,
    0x25393F2B,0x283C422E,0x2B3F4531,0x2E424834,0x32464C38,0x35494F3B,0x384C523E,0x3C505642,
    0x09182718,0x0C1B2A1B,0x0F1E2D1E,0x12213021,0x16253425,0x19283728,0x1C2B3A2B,0x1F2E3D2E,
    0x23324132,0x26354435,0x29384738,0x2C3B4A3B,0x303F4E3F,0x33425142,0x36455445,0x3A495849,
    0x0B11251F,0x0E142822,0x11172B25,0x141A2E28,0x181E322C,0x1B21352F,0x1E243832,0x21273B35,
    0x252B3F39,0x282E423C,0x2B31453F,0x2E344842,0x32384C46,0x353B4F49,0x383E524C,0x3C425650,
    0x0F0C2124,0x120F2427,0x1512272A,0x18152A2D,0x1C192E31,0x1F1C3134,0x221F3437,0x2522373A,
    0x29263B3E,0x2C293E41,0x2F2C4144,0x322F4447,0x3633484B,0x39364B4E,0x3C394E51,0x403D5255,
    0x15091B27,0x180C1E2A,0x1B0F212D,0x1E122430,0x22162834,0x25192B37,0x281C2E3A,0x2B1F313D,
    0x2F233541,0x32263844,0x35293B47,0x382C3E4A,0x3C30424E,0x3F334551,0x42364854,0x463A4C58,
    0x1C0A1426,0x1F0D1729,0x22101A2C,0x25131D2F,0x29172133,0x2C1A2436,0x2F1D2739,0x32202A3C,
    0x36242E40,0x39273143,0x3C2A3446,0x3F2D3749,0x43313B4D,0x46343E50,0x49374153,0x4D3B4557,
    0x220D0E23,0x25101126,0x28131429,0x2B16172C,0x2F1A1B30,0x321D1E33,0x35202136,0x38232439,
    0x3C27283D,0x3F2A2B40,0x422D2E43,0x45303146,0x4934354A,0x4C37384D,0x4F3A3B50,0x533E3F54,
    0x26130A1D,0x29160D20,0x2C191023,0x2F1C1326,0x3320172A,0x36231A2D,0x39261D30,0x3C292033,
    0x402D2437,0x4330273A,0x46332A3D,0x49362D40,0x4D3A3144,0x503D3447,0x5340374A,0x57443B4E,
    0x271A0916,0x2A1D0C19,0x2D200F1C,0x3023121F,0x34271623,0x372A1926,0x3A2D1C29,0x3D301F2C,
    0x41342330,0x44372633,0x473A2936,0x4A3D2C39,0x4E41303D,0x51443340,0x54473643,0x584B3A47,
    0x24200C10,0x27230F13,0x2A261216,0x2D291519,0x312D191D,0x34301C20,0x37331F23,0x3A362226,
    0x3E3A262A,0x413D292D,0x44402C30,0x47432F33,0x4B473337,0x4E4A363A,0x514D393D,0x55513D41,
    0x1F25110B,0x2228140E,0x252B1711,0x282E1A14,0x2C321E18,0x2F35211B,0x3238241E,0x353B2721,
    0x393F2B25,0x3C422E28,0x3F45312B,0x4248342E,0x464C3832,0x494F3B35,0x4C523E38,0x5056423C,
    0x19271709,0x1C2A1A0C,0x1F2D1D0F,0x22302012,0x26342416,0x29372719,0x2C3A2A1C,0x2F3D2D1F,
    0x33413123,0x36443426,0x39473729,0x3C4A3A2C,0x404E3E30,0x43514133,0x46544436,0x4A58483A,
    0x12261E0A,0x1529210D,0x182C2410,0x1B2F2713,0x1F332B17,0x22362E1A,0x2539311D,0x283C3420,
    0x2C403824,0x2F433B27,0x32463E2A,0x3549412D,0x394D4531,0x3C504834,0x3F534B37,0x43574F3B,
    //odd
    0x18181818,0x1B1B1B1B,0x1E1E1E1E,0x21212121,0x25252525,0x28282828,0x2B2B2B2B,0x2E2E2E2E,
    0x32323232,0x35353535,0x38383838,0x3B3B3B3B,0x3F3F3F3F,0x42424242,0x45454545,0x49494949,
    0x1A271609,0x1D2A190C,0x202D1C0F,0x23301F12,0x27342316,0x2A372619,0x2D3A291C,0x303D2C1F,
    0x34413023,0x37443326,0x3A473629,0x3D4A392C,0x414E3D30,0x44514033,0x47544336,0x4B58473A,
    0x21240F0C,0x2427120F,0x272A1512,0x2A2D1815,0x2E311C19,0x31341F1C,0x3437221F,0x373A2522,
    0x3B3E2926,0x3E412C29,0x41442F2C,0x4447322F,0x484B3633,0x4B4E3936,0x4E513C39,0x5255403D,
    0x251F0B11,0x28220E14,0x2B251117,0x2E28141A,0x322C181E,0x352F1B21,0x38321E24,0x3B352127,
    0x3F39252B,0x423C282E,0x453F2B31,0x48422E34,0x4C463238,0x4F49353B,0x524C383E,0x56503C42,
    0x27180918,0x2A1B0C1B,0x2D1E0F1E,0x30211221,0x34251625,0x37281928,0x3A2B1C2B,0x3D2E1F2E,
    0x41322332,0x44352635,0x47382938,0x4A3B2C3B,0x4E3F303F,0x51423342,0x54453645,0x58493A49,
    0x25110B1F,0x28140E22,0x2B171125,0x2E1A1428,0x321E182C,0x35211B2F,0x38241E32,0x3B272135,
    0x3F2B2539,0x422E283C,0x45312B3F,0x48342E42,0x4C383246,0x4F3B3549,0x523E384C,0x56423C50,
    0x210C0F24,0x240F1227,0x2712152A,0x2A15182D,0x2E191C31,0x311C1F34,0x341F2237,0x3722253A,
    0x3B26293E,0x3E292C41,0x412C2F44,0x442F3247,0x4833364B,0x4B36394E,0x4E393C51,0x523D4055,
    0x1B091527,0x1E0C182A,0x210F1B2D,0x24121E30,0x28162234,0x2B192537,0x2E1C283A,0x311F2B3D,
    0x35232F41,0x38263244,0x3B293547,0x3E2C384A,0x42303C4E,0x45333F51,0x48364254,0x4C3A4658,
    0x140A1C26,0x170D1F29,0x1A10222C,0x1D13252F,0x21172933,0x241A2C36,0x271D2F39,0x2A20323C,
    0x2E243640,0x31273943,0x342A3C46,0x372D3F49,0x3B31434D,0x3E344650,0x41374953,0x453B4D57,
    0x0E0D2223,0x11102526,0x14132829,0x17162B2C,0x1B1A2F30,0x1E1D3233,0x21203536,0x24233839,
    0x28273C3D,0x2B2A3F40,0x2E2D4243,0x31304546,0x3534494A,0x38374C4D,0x3B3A4F50,0x3F3E5354,
    0x0A13261D,0x0D162920,0x10192C23,0x131C2F26,0x1720332A,0x1A23362D,0x1D263930,0x20293C33,
    0x242D4037,0x2730433A,0x2A33463D,0x2D364940,0x313A4D44,0x343D5047,0x3740534A,0x3B44574E,
    0x091A2716,0x0C1D2A19,0x0F202D1C,0x1223301F,0x16273423,0x192A3726,0x1C2D3A29,0x1F303D2C,
    0x23344130,0x26374433,0x293A4736,0x2C3D4A39,0x30414E3D,0x33445140,0x36475443,0x3A4B5847,
    0x0C202410,0x0F232713,0x12262A16,0x15292D19,0x192D311D,0x1C303420,0x1F333723,0x22363A26,
    0x263A3E2A,0x293D412D,0x2C404430,0x2F434733,0x33474B37,0x364A4E3A,0x394D513D,0x3D515541,
    0x11251F0B,0x1428220E,0x172B2511,0x1A2E2814,0x1E322C18,0x21352F1B,0x2438321E,0x273B3521,
    0x2B3F3925,0x2E423C28,0x31453F2B,0x3448422E,0x384C4632,0x3B4F4935,0x3E524C38,0x4256503C,
    0x17271909,0x1A2A1C0C,0x1D2D1F0F,0x20302212,0x24342616,0x27372919,0x2A3A2C1C,0x2D3D2F1F,
    0x31413323,0x34443626,0x37473929,0x3A4A3C2C,0x3E4E4030,0x41514333,0x44544636,0x48584A3A,
    0x1E26120A,0x2129150D,0x242C1810,0x272F1B13,0x2B331F17,0x2E36221A,0x3139251D,0x343C2820,
    0x38402C24,0x3B432F27,0x3E46322A,0x4149352D,0x454D3931,0x48503C34,0x4B533F37,0x4F57433B,
};
uint32_t *atari_4_phase_pal_ram = 0;

/*
extern "C"
void Sound_Callback(UBYTE *buffer, unsigned int size);
*/

//input_template_t *LIBATARI800_Input_array;
//input_template_t _atari_input = {0};

#define POKEY_DIV_64      28            /* divisor for 1.79MHz clock to 64 kHz */
#define POKEY_DIV_15      114            /* divisor for 1.79MHz clock to 15 kHz */
#define POKEYSND_FREQ_17_EXACT     1789790    /* exact 1.79 MHz clock freq */
#define POKEYSND_FREQ_17_APPROX    1787520    /* approximate 1.79 MHz clock freq */

/*
 F1                   Built in user interface -2
 F2                   Option key
 F3                   Select key
 F4                   Start key
 F5                   Reset key ("warm reset")
 Shift+F5             Reboot ("cold reset")
 F6                   Help key (XL/XE only)
 F7                   Break key / Pause during DOS file booting (**)

 case SDL_SCANCODE_F1: return AKEY_UI;
 case SDL_SCANCODE_F2: return AKEY_OPTION;
 case SDL_SCANCODE_F3: return AKEY_SELECT;
 case SDL_SCANCODE_F4: return AKEY_START;
 case SDL_SCANCODE_F5: return AKEY_WARMSTART;
 case SDL_SCANCODE_F6: return AKEY_HELP;
 case SDL_SCANCODE_F7: return AKEY_BREAK;
*/

// hid/sdl scancode to atari scancode
const int16_t _scancode_to_atari[512] = {
     -1, -1, -1, -1, 63, 21, 18, 58, 42, 56, 61, 57, 13,  1,  5,  0,
     37, 35,  8, 10, 47, 40, 62, 45, 11, 16, 46, 22, 43, 23, 31, 30,
     26, 24, 29, 27, 51, 53, 48, 50, 12, 28, 52, 44, 33, 14, 15,112,
    114, 70, -1,  2,115, -1, 32, 34, 38, 60, -1,-12,-11,-10, -2, 17,
     -5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

    // shift
     -1, -1, -1, -1,127, 85, 82,122,106,120,125,121, 77, 65, 69, 64,
    101, 99, 72, 74,111,104,126,109, 75, 80,110, 86,107, 87, 95,117,
     90, 88, 93, 71, 91,  7,112,114, 76, 92,116,108, 97, 78,  6,112,
    114, 79, -1, 66, 94, -1, 54, 55,102,124, -1, -1, -1, -1, -3, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

    // ctrl
     -1, -1, -1, -1,191,149,146,186,170,184,189,185,141,129,133,128,
    165,163,136,138,175,168,190,173,139,144,174,150,171,151,159,158,
    154,152,157,155,179,181,176,178,140,156,180,172,161,142,143, -1,
     -1, -1, -1,130, -1, -1,160,162,166, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,135,
    134,143,142, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

    // shift ctrl
     -1, -1, -1, -1,255,213,210,250,234,248,253,249,205,193,197,192,
    229,227,200,202,239,232,254,237,203,208,238,214,235,215,223,222,
    218,216,221,219,243,245,240,242,204,220,244,236,225,206,207, -1,
     -1, -1, -1,194, -1, -1,224,226,230, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

#define INPUT_STICK_BACK        0x02
#define INPUT_STICK_LEFT        0x04
#define INPUT_STICK_RIGHT       0x08
#define INPUT_STICK_FORWARD     0x01
#define INPUT_TRIGGER           0x10
#define CONSOLE_START           0x01
#define CONSOLE_SELECT          0x02
#define CONSOLE_OPTION          0x04

extern int INPUT_key_code;
extern int INPUT_key_consol;
int console_keys = 7;

int _joy[4] = {0};
int _trig[4] = {0};

extern "C" void CARTRIDGE_Remove();
extern "C" void CASSETTE_Remove();

extern ULONG *Screen_atari;
#define Screen_WIDTH  384
#define Screen_HEIGHT 240

const char* _atari_help[] = {
    "1 or 2 inserts .atr into drive #",
    "0 removes .atr from drive",
    "Shift + Enter enables Basic",
    "",
    "Keyboard Mappings:",
    "  Arrow Keys - Joystick",
    "  Left Shift - Fire",
    "  F2         - Option",
    "  F3         - Select",
    "  F4         - Start",
    "  F5         - Warm Reset",
    "  Shift+F5   - Cold Reset",
    "  F6         - Help (XL/XE)",
    "  F7         - Break",
    "",
    "Wiimote (held sideways):",
    "  D Pad      - Joystick",
    "  +          - Start",
    "  -          - Select",
    "  + & -      - Warm Reset",
    "  A,B,1,2    - Trigger",
    "",
    "5200 Keyboard Mappings:",
    "  S key      - Start",
    "  P key      - Pause",
    "  R key      - Reset",
    "",
    0
};

// does not support atx. sorry
const char* _atari_ext[] = {
    "atr",
    "rom",
    "cas",
    "bin",
    "xex",
    "com",
    "bas",
    "car",
    0
};

//========================================================================================
//========================================================================================

class File {
public:
    FILE *_fd;
    size_t _len;

    static int le16(const void* d)
    {
        const uint8_t* b = (const uint8_t*)d;
        return b[0] | (b[1] << 8);
    }

    static int le32(const void* d)
    {
        const uint8_t* b = (const uint8_t*)d;
        return b[0] | (b[1] << 8) | (b[2] << 8) | (b[3] << 8);
    }

    File(const string& name) : _len(0) {
        _fd = fopen(name.c_str(),"rb");
        if (_fd) {
            fseek(_fd,0,SEEK_END);
            _len = ftell(_fd);
        }
    }

    virtual ~File()
    {
        if (_fd)
            fclose(_fd);
    }

    int read(void* dst, int offset, int len)
    {
        if (!_fd)
            return -1;
        fseek(_fd,offset,SEEK_SET);
        return (int)fread(dst,1,len,_fd);
    }
};

class AtariDisk : public File {
public:
    int _type;
    int _size;
    int _secsize;

    AtariDisk(const string& name) : File(name),_type(-1)
    {
        uint8_t hdr[48];
        read(hdr,0,sizeof(hdr));
        if (hdr[0] == 'A' && hdr[1] == 'T' && hdr[2] == '8' && hdr[3] == 'X') {
            if (vapi(le32(hdr + 28)) == 0)
                _type = 1;
        } else if (le16(hdr) == 0x296) {
            _type = 0;
            _size = le16(hdr + 2);
            _secsize = le16(hdr + 4);
            _size += hdr[6] << 16;       // high part of size
            _size <<= 4;                 //
        }
    }

    class Track {
    public:
        int offset;
        int size;
        int type;
        int tracknum;
        int sectorcount;
        int headersize;
        int sectorlistsize;
        int16_t toff[18];
    };
    vector<Track> _tracks;

    // http://whizzosoftware.com/sio2arduino/vapi.html
    int vapi(int trackoffset)
    {
        _type = 1;
        _secsize = 128;
        while (trackoffset > 0 && trackoffset < _len) {
            uint8_t t[32+8];
            if (read(t,trackoffset,sizeof(t)) != sizeof(t))
                return -1;
            Track track;
            track.offset = trackoffset;
            track.size = le32(t);
            track.type = le16(t+4);
            track.tracknum = t[8];
            track.sectorcount = le16(t+10);
            track.headersize = le32(t+20);
            track.sectorlistsize = le32(t+32) - 8;
            for (int i = 0; i < 16; i++)
                track.toff[i] = 0;
            uint8_t sl[256];
            read(sl,trackoffset+32+8,track.sectorlistsize);
            for (int i = 0; i < track.sectorlistsize; i += 8) {
                /*
                int num = sl[i+0];
                int stat = sl[i+1];
                int pos = le16(sl+i+2);
                int data = le32(sl+i+4);
                printf("%d:%d %02X %d %d\n",track.tracknum,num,stat,pos,data);
                 */
                int dat = le32(sl+i+4);
                if (dat && sl[i])
                    track.toff[sl[i]-1] = dat;
            }
            _tracks.push_back(track);
            trackoffset += track.size;
        }
        _size = 720*_secsize;
        return 0;
    }

    int sectoroffset(int n)
    {
        if (n <= 3)
            return 16 + n*0x80;
        return 16 + 3*0x80 + (n-3)*_secsize;
    }

    int readsector(int n, uint8_t* dst)
    {
        if (_type == 0)
            return read(dst,sectoroffset(n),_secsize);

        if (_type == 1) {
            int track = n/18;
            for (int i = 0; i < _tracks.size(); i++) {
                if (_tracks[i].tracknum == track) {
                    auto& t = _tracks[i];
                    int dat = t.toff[n % 18];
                    if (!dat) {
                        memset(dst,0,_secsize);
                        return _secsize;
                    }
                    return read(dst,t.offset + dat,_secsize);
                }
            }
            return -1;
        }
        return -1;
    }

    // http://atari.kensclassics.org/dos.htm
    // note: indexes are 1 based in the docs
    void dir(vector<string>& s)
    {
        s.push_back("");
        uint8_t a[256];
        if (readsector(359,a) != _secsize)
            return;
        int free = le16(a+3);
        //if (free < 0 || free > 707)
        //    return;
        for (int n = 360; n < 368; n++) {
            if (readsector(n,a) != _secsize)
                return;
            for (int i = 0; i < _secsize; i += 16)
            {
                int flags = a[i];
                int total = le16(a+i+1);
                if (!total || !(flags & 0x40))  // not an ordinary file?
                    break;
                //int start = le16(a+i+3)-1;
                string name = string((char*)a+i+5,8+3);
                if (flags & 0x80)
                    ;   // deleted
                else {
                    string t = ::to_string(total);
                    while (t.length() < 3)
                        t = "0" + t;
                    s.push_back((flags & 0x20 ? "* " :"  ") + name + " " + t);
                }
            }
        };
        s.push_back(::to_string(free) + " free sectors");
    }
};

//========================================================================================
//========================================================================================

static
int get_info(const string& file, vector<string>& strs)
{
    string ext = get_ext(file);
    uint8_t hdr[48];
    int len = Emu::head(file,hdr,sizeof(hdr));
    string name = file.substr(file.find_last_of("/") + 1);
    strs.push_back(name);
    strs.push_back("");

    // carts
    if (ext == "car" || ext == "bin" || ext == "rom") {
        if (hdr[0] == 'C' && hdr[1] == 'A' && hdr[2] == 'R' && hdr[3] == 'T' && hdr[7] <= 60)
        {
            strs.push_back(_cart_info[hdr[7]].name);
            strs.push_back(_cart_info[hdr[7]].machine);
        } else {
            strs.push_back(::to_string(len/1024) + "k Cartridge");
        }
        return 0;
    }

    if (ext == "xex") {
        strs.push_back(::to_string((len + 0x3FF)/1024) + "k Executable");
        int offset = 0;
        File f(file);
        while (offset < len) {
            uint16_t start = 0xFFFF;
            uint16_t end = 0xFFFF;
            uint16_t addr = 0xFFFF;
            while (start == 0xFFFF) {
                if (f.read(&start,offset,2) != 2)
                    break;
                offset += 2;
            }
            if (f.read(&end,offset,2) != 2)
                break;
            char buf[64];
            if (start == 0x2E0 || start == 0x2E2) {
                f.read(&addr,offset+2,2);
                sprintf(buf,"%04X at %04X:%04X %s:%04X",end+1-start,start,end,start == 0x2E0 ? "run":"ini",addr);
            } else {
                sprintf(buf,"%04X at %04X:%04X",end+1-start,start,end);
            }
            strs.push_back(buf);
            offset += 2 + end+1 - start;
        }
        return 0;
    }

    if (ext == "atr" || ext == "atx") {
        strs.push_back(::to_string((len + 0x3FF)/1024) + "k Disk Image");
        AtariDisk disk(file);
        disk.dir(strs);
        return 0;
    }
    strs.push_back(::to_string(len) + " bytes");
    return 0;
}

//========================================================================================
//========================================================================================

extern uint8_t* under_atarixl_os;  // allocate in 32 bit mem
extern uint8_t* under_cart809F;
extern uint8_t* under_cartA0BF;

Sound_setup_t Sound_desired = {
    15720,
    1,  // 8 bit
    1,  // 1 channel
    0,
    0
};

class EmuAtari800 : public Emu {
    uint8_t** _lines;
public:
    EmuAtari800(int ntsc) : Emu("atari800",384,240,ntsc,(16 | (1 << 8)),4,EMU_ATARI)
    {
        _lines = 0;
        _ext = _atari_ext;
        _help = _atari_help;
        Sound_desired.freq = audio_frequency;
    }

    virtual void gen_palettes()
    {
        make_atari_rgb_palette();
        atari_palette();
    }

    virtual int info(const string& file, vector<string>& strs)
    {
        get_info(file,strs);
        uint8_t* data;
        int len;
        if (load(file+".cfg",&data,&len) == 0) {
            strs.push_back("");
            strs.push_back(".cfg file:");
            string s = string((const char*)data,len);
            s.erase(s.find_last_not_of(" \n\r\t")+1);
            strs.push_back(s);
            delete [] data;
        }
        return 0;
    }

    void trig(int i, int pressed)
    {
        _trig[i] = pressed ? 1 : 0;
    }

    void joy(int i, int pressed, int mask)
    {
        _joy[i] = pressed ? (_joy[i] | mask) : (_joy[i] & ~mask);
    }

    void console(int pressed, int mask)
    {
        console_keys = pressed ? (console_keys & ~mask) : (console_keys | mask);
    }

    // wiimote is rotated 90%
    const uint32_t _common_atari[16] = {
        0,  // msb
        0,
        0,
        CONSOLE_START<<8,       // PLUS
        INPUT_STICK_LEFT,       // UP
        INPUT_STICK_RIGHT,      // DOWN
        INPUT_STICK_FORWARD,    // RIGHT
        INPUT_STICK_BACK,       // LEFT

        0, // HOME
        0,
        0,
        CONSOLE_SELECT<<8,  // MINUS
        INPUT_TRIGGER,      // A
        INPUT_TRIGGER,      // B
        INPUT_TRIGGER,      // ONE
        INPUT_TRIGGER,      // TWO
    };

    const uint32_t _classic_atari[16] = {
        INPUT_STICK_RIGHT,    // RIGHT
        INPUT_STICK_BACK,     // DOWN
        0,                    // LEFT_TOP
        CONSOLE_SELECT<<8,    // MINUS
        0,                    // HOME
        CONSOLE_START<<8,     // PLUS
        0,                    // RIGHT_TOP
        0,

        0,              // LOWER_LEFT
        INPUT_TRIGGER,  // B
        INPUT_TRIGGER,  // Y
        INPUT_TRIGGER,  // A
        INPUT_TRIGGER,  // X
        0,              // LOWER_RIGHT
        INPUT_STICK_LEFT,     // LEFT
        INPUT_STICK_FORWARD   // UP
    };

    const uint32_t _generic_atari[16] = {
        0,                  // GENERIC_OTHER   0x8000
        0,                  // GENERIC_FIRE_X  0x4000  // RETCON
        0,                  // GENERIC_FIRE_Y  0x2000
        0,                  // GENERIC_FIRE_Z  0x1000

        INPUT_TRIGGER,      //GENERIC_FIRE_A  0x0800
        INPUT_TRIGGER,      //GENERIC_FIRE_B  0x0400
        INPUT_TRIGGER,      //GENERIC_FIRE_C  0x0200
        0,                  //GENERIC_RESET   0x0100     // ATARI FLASHBACK

        CONSOLE_START<<8,   //GENERIC_START   0x0080
        CONSOLE_SELECT<<8,  //GENERIC_SELECT  0x0040
        INPUT_TRIGGER,      //GENERIC_FIRE    0x0020
        INPUT_STICK_RIGHT,  //GENERIC_RIGHT   0x0010

        INPUT_STICK_LEFT,   //GENERIC_LEFT    0x0008
        INPUT_STICK_BACK,   //GENERIC_DOWN    0x0004
        INPUT_STICK_FORWARD,//GENERIC_UP      0x0002
        0,                  //GENERIC_MENU    0x0001
    };

    // raw HID data. handle WII mappings
    virtual void hid(const uint8_t* d, int len)
    {
        if (d[0] != 0x32 && d[0] != 0x42)
            return;
        bool ir = *d++ == 0x42;
        int reset = 0;
        for (int i = 0; i < 4; i++) {
            uint32_t p;
            if (ir) {
                uint16_t m =  i < 2 ? (d[0] + (d[1] << 8)) : 0;
                p = generic_map(m,_generic_atari);
                d += 2;
            } else
                p = wii_map(i,_common_atari,_classic_atari);
            _joy[i] = p & 0x0F;
            _trig[i] = (p >> 4) & 1;
            if (i == 0)
                INPUT_key_consol = (p >> 8) ^ 7;
            if ((p & (CONSOLE_SELECT<<8)) && (p & (CONSOLE_START<<8)))
                reset++;
        }

        if (reset)
            INPUT_key_code = AKEY_WARMSTART;
        else
            INPUT_key_code = -1;

        if (Atari800_machine_type == Atari800_MACHINE_5200) {
            if (reset)
                INPUT_key_code = AKEY_5200_RESET;
            else if (INPUT_key_consol & 1)
                INPUT_key_code = AKEY_5200_START;
            else if (INPUT_key_consol & 2)
                INPUT_key_code = AKEY_5200_PAUSE;
        }
    }

    // https://www.atariarchives.org/c3ba/page004.php
    virtual void key(int keycode, int pressed, int mods)
    {
        INPUT_key_code = -1;

        // map arrow keys to joy0
        switch (keycode) {
            case 82: joy(0,pressed,INPUT_STICK_FORWARD); break;
            case 81: joy(0,pressed,INPUT_STICK_BACK); break;
            case 80: joy(0,pressed,INPUT_STICK_LEFT); break;
            case 79: joy(0,pressed,INPUT_STICK_RIGHT); break;
            case 225: trig(0,pressed); break; // left shift key
        }

        if (keycode >= 128)
            return;

        if (Atari800_machine_type == Atari800_MACHINE_5200) {
            switch (keycode) {
                case 61: keycode =  AKEY_5200_START; break; // F4
                case 62: keycode =  AKEY_5200_RESET; break; // F5

                case 22: keycode =  AKEY_5200_START; break; // 's'
                case 19: keycode =  AKEY_5200_PAUSE; break; // 'p'
                case 21: keycode =  AKEY_5200_RESET; break; // 'r'

                case 30: keycode =  AKEY_5200_1; break; // 1
                case 31: keycode =  AKEY_5200_2; break; // 2
                case 32: keycode =  (KEY_MOD_SHIFT & mods) ? AKEY_5200_HASH : AKEY_5200_3; break; // 3
                case 33: keycode =  AKEY_5200_4; break; // 4
                case 34: keycode =  AKEY_5200_5; break; // 5
                case 35: keycode =  AKEY_5200_6; break; // 6
                case 36: keycode =  AKEY_5200_7; break; // 7
                case 37: keycode =  (KEY_MOD_SHIFT & mods) ? AKEY_5200_ASTERISK : AKEY_5200_8; break; // 8
                case 38: keycode =  AKEY_5200_9; break; // 9
                case 39: keycode =  AKEY_5200_0; break; // 0

                case 46: keycode = AKEY_5200_HASH; break;   // =
                default: keycode = AKEY_NONE;
            }
        } else {
            if ((KEY_MOD_CTRL & mods) && (KEY_MOD_SHIFT & mods)) {
                keycode += 128*3;
            } else if (KEY_MOD_CTRL & mods) {
                keycode += 128*2;
            } else if (KEY_MOD_SHIFT & mods) {
                keycode += 128*1;
            } else {
            }
            keycode = _scancode_to_atari[keycode];

            // handle console_keys state
            switch (keycode) {
                case AKEY_OPTION: console_keys = pressed ? (console_keys & ~4) : (console_keys | 4); break;
                case AKEY_SELECT: console_keys = pressed ? (console_keys & ~2) : (console_keys | 2); break;
                case AKEY_START: console_keys = pressed ? (console_keys & ~1) : (console_keys | 1); break;
            }
            INPUT_key_consol = console_keys;
        }

        if (pressed)
            INPUT_key_code = keycode;
    }

    // allocate most of the big stuff in 32 bit  mem
    void init_screen()
    {
        Screen_atari = (ULONG*)MALLOC32(Screen_WIDTH*Screen_HEIGHT,"Screen_atari");    // 32 bit access plz
        MEMORY_mem = (uint8_t*)MALLOC32(64*1024 + 4,"MEMORY_mem");
        _lines = (uint8_t**)MALLOC32(height*sizeof(uint8_t*),"_lines");
        const uint8_t* s = (uint8_t*)Screen_atari;
        for (int y = 0; y < height; y++) {
            _lines[y] = (uint8_t*)s;
            s += width;
        }
        under_atarixl_os = (uint8_t*)MALLOC32(16*1024,"under_atarixl_os");
        under_cart809F = (uint8_t*)MALLOC32(8*1024,"under_cart809F");
        under_cartA0BF = (uint8_t*)MALLOC32(8*1024,"under_cartA0BF");
        clear_screen();
    }

    void clear_screen()
    {
        int i = Screen_WIDTH*Screen_HEIGHT/4;
        while (i--)
            Screen_atari[i] = 0;
    }

    int parse_cfg(const string& str, vector<string>& s, vector<char*>& argv)
    {
        string w;
        for (size_t i = 0; i < str.length(); i++)
        {
            char c = str[i];
            if (isspace(c)) {
                if (w.length())
                    s.push_back(w);
                w.clear();
            } else if(c == '\"' ){
                i++;
                while(i < str.length() && str[i] != '\"') {
                    w += str[i];
                    i++;
                }
            } else {
                w += c;
            }
        }
        if (w.length())
            s.push_back(w);
        for (size_t i = 0; i < s.size(); i++)
            argv.push_back((char*)s[i].c_str());
        return (int)s.size();
    }

    // https://github.com/dmlloyd/atari800/blob/master/DOC/USAGE
    // -nobasic              Turn off Atari BASIC ROM
    // -basic
    // -boottape

    string tv_standard()
    {
        return standard ? " -ntsc" : " -pal";
    }

    string patch_cfg(const string& cfg, int mods)
    {
        string c = cfg;
        if (c.find("-ntsc") == -1 && c.find("-pal") == -1)
            c += tv_standard();
        if (c.find("-basic") == -1 && c.find("-nobasic") == -1)
            c += (mods & 2) ? " -basic" : " -nobasic"; // insert on shift key
        return c;
    }

    string get_cfg(const string& path)
    {
        // read extension. figure out what kind of file we have
        string ext = get_ext(path);

        // open based on config settings if present
        {
            int len;
            uint8_t* data;
            if (load(path+".cfg",&data,&len) == 0) {
                string cfg((const char*)data,len);
                delete data;
                return cfg + " \"" + path + "\"";
            }
        }

        // guess type
        int cart_type = 0;
        uint8_t data[64];
        int len = head(path,data,sizeof(data));
        switch (len) {
            case 0x2000: cart_type = 1; break;  // probably
            case 0x4000: cart_type = 2; break;  // probably
            case 0x8000: cart_type = 4; break;  // probably
            default:
                if (ext == "cas")
                    return "-boottape \"" + path + "\"";
        }

        switch (cart_type) {
            case 1: return "-atari -cart-type 1 -cart \"" + path + "\"";
            case 2: return "-atari -cart-type 2 -cart \"" + path + "\"";
            case 4: return "-5200 -cart-type 4 -cart \"" + path + "\"";
        }

        // no idea. just go with defaults of xl
        string host = path.substr(0,path.find_last_of("/"));
        return "-xl \"" + path + "\"" + " -H1 \"" + host + "\"";
    }

    //  default media is basic/dos
    virtual int make_default_media(const string& path)
    {
        unpack((path + "/dos20.atr").c_str(),dos20_atr,sizeof(dos20_atr));
        unpack((path + "/balls_forever.xex").c_str(),balls_forever_xex,sizeof(balls_forever_xex));
        unpack((path + "/paperweight.xex").c_str(),paperweight_xex,sizeof(paperweight_xex));
        unpack((path + "/boink.xex").c_str(),boink_xex,sizeof(boink_xex));
        unpack((path + "/more.xex").c_str(),more_xex,sizeof(more_xex));
        unpack((path + "/callisto.xex").c_str(),callisto_xex,sizeof(callisto_xex));
        unpack((path + "/janes_program.xex").c_str(),janes_program_xex,sizeof(janes_program_xex));
        unpack((path + "/numen_rubik.atr").c_str(),numen_rubik_atr,sizeof(numen_rubik_atr));
        unpack((path + "/atari_robot.xex").c_str(),atari_robot_xex,sizeof(atari_robot_xex));
        unpack((path + "/callisto.xex").c_str(),callisto_xex,sizeof(callisto_xex));
        unpack((path + "/maze.xex").c_str(),maze_xex,sizeof(maze_xex));
        unpack((path + "/mini_zork.atr").c_str(),mini_zork_atr,sizeof(mini_zork_atr));
        unpack((path + "/gtia_blast.xex").c_str(),gtia_blast_xex,sizeof(gtia_blast_xex));
        unpack((path + "/runner_bear.xex").c_str(),runner_bear_xex,sizeof(runner_bear_xex));
        unpack((path + "/yoomp_nt.xex").c_str(),yoomp_nt_xex,sizeof(yoomp_nt_xex));
        unpack((path + "/raymaze_2000_ntsc.xex").c_str(),yoomp_nt_xex,sizeof(yoomp_nt_xex));
        unpack((path + "/gravity_worms.atr").c_str(),gravity_worms_atr,sizeof(gravity_worms_atr));
        unpack((path + "/wasteland.atr").c_str(),wasteland_atr,sizeof(wasteland_atr));
        unpack((path + "/star_raiders_II.atr").c_str(),star_raiders_II_atr,sizeof(star_raiders_II_atr));
        return 0;
    }

    virtual int insert(const std::string& path, int flags, int disk_index)
    {
        if (!_lines)
            init_screen();

        // just insert a disk
        if (((flags & 1) == 0) && (get_ext(path) == "atr")) {
            printf("inserting %s into drive %d\n",path.c_str(),disk_index+1);
            return libatari800_mount_disk_image(disk_index+1,path.c_str(),false);
        }

        // restarting
        clear_screen();
        CARTRIDGE_Remove();
        CASSETTE_Remove();

        vector<string> s;
        vector<char*> argv;
        s.push_back("atari800");
        string cfg = get_cfg(path);
        cfg = patch_cfg(cfg,flags);
        int argc = parse_cfg(cfg,s,argv);
        libatari800_init(argc,&argv[0]);
        return 0;
    }

    virtual int update()
    {
        return libatari800_next_frame(NULL);
    }

    virtual uint8_t** video_buffer()
    {
        return _lines;
    }

    virtual int audio_buffer(int16_t* b, int len)
    {
        int n = frame_sample_count();
        Sound_Callback((uint8_t*)b,n);      // in bytes
        uint8_t* b8 = (uint8_t*)b;
        for (int i = n-1; i >= 0; i--) {
            int s16 = (b8[i] ^ 0x80) << 8;
            b[i] = s16;
        }
        return n;
    }

    virtual const uint32_t* ntsc_palette()
    {
      if (!atari_4_phase_ntsc_ram) {
            atari_4_phase_ntsc_ram = new uint32_t[256];
            memcpy(atari_4_phase_ntsc_ram,atari_4_phase_ntsc,256*4);  // copy into ram as we are tight on static mem
      }
      return atari_4_phase_ntsc_ram;
    };

    virtual const uint32_t* pal_palette()
    {
        if (!atari_4_phase_pal_ram)
        {
            atari_4_phase_pal_ram = new uint32_t[512];
            memcpy(atari_4_phase_pal_ram,atari_4_phase_pal,512*4);  // copy into ram as we are tight on static mem
        }
        return atari_4_phase_pal_ram;
     }
    virtual const uint32_t* rgb_palette()   { return atari_palette_rgb; };
};

extern "C"
int LIBATARI800_Input_Initialise(int *argc, char *argv[])
{
    return TRUE;
}

extern "C"
int PLATFORM_Keyboard(void)
{
    return INPUT_key_code;
}

extern "C"
int PLATFORM_PORT(int num)
{
    if (num == 0)
        return (_joy[0] | (_joy[1] << 4)) ^ 0xFF;  // sense is inverted
    if (num == 1)
        return (_joy[2] | (_joy[3] << 4)) ^ 0xFF;  // sense is inverted
    return 0xff;
}

extern "C"
int PLATFORM_TRIG(int num)
{
    if (num < 0 || num >= 4)
        return 1;
    return _trig[num] ^ 1;
}

extern "C"
void LIBATARI800_Mouse(void)
{
#if 0
    int mouse_mode;

    input_template_t *input = LIBATARI800_Input_array;

    mouse_mode = input->mouse_mode;

    if (mouse_mode == LIBATARI800_FLAG_DIRECT_MOUSE) {
        int potx, poty;

        potx = input->mousex;
        poty = input->mousey;
        if(potx < 0) potx = 0;
        if(poty < 0) poty = 0;
        if(potx > 227) potx = 227;
        if(poty > 227) poty = 227;
        POKEY_POT_input[INPUT_mouse_port << 1] = 227 - potx;
        POKEY_POT_input[(INPUT_mouse_port << 1) + 1] = 227 - poty;
    }
    else {
        INPUT_mouse_delta_x = input->mousex;
        INPUT_mouse_delta_y = input->mousey;
    }

    INPUT_mouse_buttons = input->mouse_buttons;
#endif
}

Emu* NewAtari800(int ntsc)
{
    return new EmuAtari800(ntsc);
}
