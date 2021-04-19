
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

#define VIDEO_PIN   26
#define AUDIO_PIN   18  // can be any pin

int _pal_ = 0;

#ifdef ESP_PLATFORM
#include "esp_types.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/ledc_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/io_mux_reg.h"
#include "rom/gpio.h"
#include "rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"



//====================================================================================================
//====================================================================================================
//
// low level HW setup of DAC/DMA/APLL/PWM
//

lldesc_t _dma_desc[4] = {0};
intr_handle_t _isr_handle;

extern "C"
void IRAM_ATTR video_isr(volatile void* buf);

// simple isr
void IRAM_ATTR i2s_intr_handler_video(void *arg)
{
    if (I2S0.int_st.out_eof)
        video_isr(((lldesc_t*)I2S0.out_eof_des_addr)->buf); // get the next line of video
    I2S0.int_clr.val = I2S0.int_st.val;                     // reset the interrupt
}

static esp_err_t start_dma(int line_width,int samples_per_cc, int ch = 1)
{
    periph_module_enable(PERIPH_I2S0_MODULE);

    // setup interrupt
    if (esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM,
        i2s_intr_handler_video, 0, &_isr_handle) != ESP_OK)
        return -1;

    // reset conf
    I2S0.conf.val = 1;
    I2S0.conf.val = 0;
    I2S0.conf.tx_right_first = 1;
    I2S0.conf.tx_mono = (ch == 2 ? 0 : 1);

    I2S0.conf2.lcd_en = 1;
    I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S0.sample_rate_conf.tx_bits_mod = 16;
    I2S0.conf_chan.tx_chan_mod = (ch == 2) ? 0 : 1;

    // Create TX DMA buffers
    for (int i = 0; i < 2; i++) {
        int n = line_width*2*ch;
        if (n >= 4092) {
            printf("DMA chunk too big:%s\n",n);
            return -1;
        }
        _dma_desc[i].buf = (uint8_t*)heap_caps_calloc(1, n, MALLOC_CAP_DMA);
        if (!_dma_desc[i].buf)
            return -1;

        _dma_desc[i].owner = 1;
        _dma_desc[i].eof = 1;
        _dma_desc[i].length = n;
        _dma_desc[i].size = n;
        _dma_desc[i].empty = (uint32_t)(i == 1 ? _dma_desc : _dma_desc+1);
    }
    I2S0.out_link.addr = (uint32_t)_dma_desc;

    //  Setup up the apll: See ref 3.2.7 Audio PLL
    //  f_xtal = (int)rtc_clk_xtal_freq_get() * 1000000;
    //  f_out = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536); // 250 < f_out < 500
    //  apll_freq = f_out/((o_div + 2) * 2)
    //  operating range of the f_out is 250 MHz ~ 500 MHz
    //  operating range of the apll_freq is 16 ~ 128 MHz.
    //  select sdm0,sdm1,sdm2 to produce nice multiples of colorburst frequencies

    //  see calc_freq() for math: (4+a)*10/((2 + b)*2) mhz
    //  up to 20mhz seems to work ok:
    //  rtc_clk_apll_enable(1,0x00,0x00,0x4,0);   // 20mhz for fancy DDS

    if (!_pal_) {
        switch (samples_per_cc) {
            case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // 10.7386363636 3x NTSC (10.7386398315mhz)
            case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // 14.3181818182 4x NTSC (14.3181864421mhz)
        }
    } else {
        rtc_clk_apll_enable(1,0x04,0xA4,0x6,1);     // 17.734476mhz ~4x PAL
    }

    I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider’s integral value.
    I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider’s numerator value.
    I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider’s denominator value
    I2S0.sample_rate_conf.tx_bck_div_num = 1;
    I2S0.clkm_conf.clka_en = 1;                 // Set this bit to enable clk_apll.
    I2S0.fifo_conf.tx_fifo_mod = (ch == 2) ? 0 : 1; // 32-bit dual or 16-bit single channel data

    dac_output_enable(DAC_CHANNEL_1);           // DAC, video on GPIO25
    dac_i2s_enable();                           // start DAC!

    I2S0.conf.tx_start = 1;                     // start DMA!
    I2S0.int_clr.val = 0xFFFFFFFF;
    I2S0.int_ena.out_eof = 1;
    I2S0.out_link.start = 1;
    return esp_intr_enable(_isr_handle);        // start interruprs!
}

void video_init_hw(int line_width, int samples_per_cc)
{
    // setup apll 4x NTSC or PAL colorburst rate
    start_dma(line_width,samples_per_cc,1);

    // Now ideally we would like to use the decoupled left DAC channel to produce audio
    // But when using the APLL there appears to be some clock domain conflict that causes
    // nasty digitial spikes and dropouts. You are also limited to a single audio channel.
    // So it is back to PWM/PDM and a 1 bit DAC for us. Good news is that we can do stereo
    // if we want to and have lots of different ways of doing nice noise shaping etc.

    // PWM audio out of pin 18 -> can be anything
    // lots of other ways, PDM by hand over I2S1, spi circular buffer etc
    // but if you would like stereo the led pwm seems like a fine choice
    // needs a simple rc filter (1k->1.2k resistor & 10nf->15nf cap work fine)

    // 18 ----/\/\/\/----|------- a out
    //          1k       |
    //                  ---
    //                  --- 10nf
    //                   |
    //                   v gnd

    ledcSetup(0,2000000,7);    // 625000 khz is as fast as we go w 7 bits
    ledcAttachPin(AUDIO_PIN, 0);
    ledcWrite(0,0);

}

// send an audio sample every scanline (15720hz for ntsc, 15600hz for PAL)
inline void IRAM_ATTR audio_sample(uint8_t s)
{
    auto& reg = LEDC.channel_group[0].channel[0];
    reg.duty.duty = s << 4; // 25 bit (21.4)
    reg.conf0.sig_out_en = 1; // This is the output enable control bit for channel
    reg.conf1.duty_start = 1; // When duty_num duty_cycle and duty_scale has been configured. these register won't take effect until set duty_start. this bit is automatically cleared by hardware
    reg.conf0.clk_en = 1;
}

//  Appendix

/*
static
void calc_freq(double f)
{
    f /= 1000000;
    printf("looking for sample rate of %fmhz\n",(float)f);
    int xtal_freq = 40;
    for (int o_div = 0; o_div < 3; o_div++) {
        float f_out = 4*f*((o_div + 2)*2);          // 250 < f_out < 500
        if (f_out < 250 || f_out > 500)
            continue;
        int sdm = round((f_out/xtal_freq - 4)*65536);
        float apll_freq = 40 * (4 + (float)sdm/65536)/((o_div + 2)*2);    // 16 < apll_freq < 128 MHz
        if (apll_freq < 16 || apll_freq > 128)
            continue;
        printf("f_out:%f %d:0x%06X %fmhz %f\n",f_out,o_div,sdm,apll_freq/4,f/(apll_freq/4));
    }
    printf("\n");
}

static void freqs()
{
    calc_freq(PAL_FREQUENCY*3);
    calc_freq(PAL_FREQUENCY*4);
    calc_freq(NTSC_FREQUENCY*3);
    calc_freq(NTSC_FREQUENCY*4);
    calc_freq(20000000);
}
*/

extern "C"
void* MALLOC32(int x, const char* label)
{
    printf("MALLOC32 %d free, %d biggest, allocating %s:%d\n",
      heap_caps_get_free_size(MALLOC_CAP_32BIT),heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),label,x);
    void * r = heap_caps_malloc(x,MALLOC_CAP_32BIT);
    if (!r) {
        printf("MALLOC32 FAILED allocation of %s:%d!!!!####################\n",label,x);
        esp_restart();
    }
    else
        printf("MALLOC32 allocation of %s:%d %08X\n",label,x,r);
    return r;
}

#else

//====================================================================================================
//====================================================================================================
//  Simulator
//

#define IRAM_ATTR
#define DRAM_ATTR

void video_init_hw(int line_width, int samples_per_cc);

uint32_t xthal_get_ccount() {
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return lo;
    //return ((uint64_t)hi << 32) | lo;
}

void audio_sample(uint8_t s);

int get_hid_ir(uint8_t* buf)
{
    return 0;
}

#endif

//====================================================================================================
//====================================================================================================


// ntsc phase representation of a rrrgggbb pixel
// must be in RAM so VBL works
const uint32_t ntsc_RGB332[256] = {
    0x18181818,0x18171A1C,0x1A151D22,0x1B141F26,0x1D1C1A1B,0x1E1B1C20,0x20191F26,0x2119222A,
    0x23201C1F,0x241F1E24,0x251E222A,0x261D242E,0x29241F23,0x2A232128,0x2B22242E,0x2C212632,
    0x2E282127,0x2F27232C,0x31262732,0x32252936,0x342C232B,0x352B2630,0x372A2936,0x38292B3A,
    0x3A30262F,0x3B2F2833,0x3C2E2B3A,0x3D2D2E3E,0x40352834,0x41342B38,0x43332E3E,0x44323042,
    0x181B1B18,0x191A1D1C,0x1B192022,0x1C182327,0x1E1F1D1C,0x1F1E2020,0x201D2326,0x211C252B,
    0x24232020,0x25222224,0x2621252A,0x2720272F,0x29272224,0x2A262428,0x2C25282E,0x2D242A33,
    0x2F2B2428,0x302A272C,0x32292A32,0x33282C37,0x352F272C,0x362E2930,0x372D2C36,0x382C2F3B,
    0x3B332930,0x3C332B34,0x3D312F3A,0x3E30313F,0x41382C35,0x42372E39,0x4336313F,0x44353443,
    0x191E1E19,0x1A1D211D,0x1B1C2423,0x1C1B2628,0x1F22211D,0x20212321,0x21202627,0x221F292C,
    0x24262321,0x25252525,0x2724292B,0x28232B30,0x2A2A2625,0x2B292829,0x2D282B2F,0x2E272D34,
    0x302E2829,0x312E2A2D,0x322C2D33,0x332B3038,0x36332A2D,0x37322C31,0x38303037,0x392F323C,
    0x3B372D31,0x3C362F35,0x3E35323B,0x3F343440,0x423B2F36,0x423A313A,0x44393540,0x45383744,
    0x1A21221A,0x1B20241E,0x1C1F2724,0x1D1E2A29,0x1F25241E,0x20242622,0x22232A28,0x23222C2D,
    0x25292722,0x26292926,0x27272C2C,0x28262E30,0x2B2E2926,0x2C2D2B2A,0x2D2B2E30,0x2E2A3134,
    0x31322B2A,0x32312E2E,0x332F3134,0x342F3338,0x36362E2E,0x37353032,0x39343338,0x3A33363C,
    0x3C3A3032,0x3D393236,0x3E38363C,0x3F373840,0x423E3337,0x433E353B,0x453C3841,0x463B3A45,
    0x1A24251B,0x1B24271F,0x1D222B25,0x1E212D29,0x2029281F,0x21282A23,0x22262D29,0x23252F2D,
    0x262D2A23,0x272C2C27,0x282A2F2D,0x292A3231,0x2C312C27,0x2C302F2B,0x2E2F3231,0x2F2E3435,
    0x31352F2B,0x3234312F,0x34333435,0x35323739,0x3739312F,0x38383333,0x39373739,0x3A36393D,
    0x3D3D3433,0x3E3C3637,0x3F3B393D,0x403A3B41,0x43423637,0x4441383B,0x453F3C42,0x463F3E46,
    0x1B28291C,0x1C272B20,0x1D252E26,0x1E25302A,0x212C2B20,0x222B2D24,0x232A312A,0x2429332E,
    0x26302D24,0x272F3028,0x292E332E,0x2A2D3532,0x2C343028,0x2D33322C,0x2F323532,0x30313836,
    0x3238322C,0x33373430,0x34363836,0x35353A3A,0x383C3530,0x393B3734,0x3A3A3A3A,0x3B393C3E,
    0x3D403734,0x3E403938,0x403E3C3E,0x413D3F42,0x44453A38,0x45443C3C,0x46433F42,0x47424147,
    0x1C2B2C1D,0x1D2A2E21,0x1E293227,0x1F28342B,0x212F2E21,0x222E3125,0x242D342B,0x252C362F,
    0x27333125,0x28323329,0x2A31362F,0x2B303933,0x2D373329,0x2E36352D,0x2F353933,0x30343B37,
    0x333B362D,0x343B3831,0x35393B37,0x36383D3B,0x38403831,0x393F3A35,0x3B3D3E3B,0x3C3C403F,
    0x3E443A35,0x3F433D39,0x4141403F,0x42414243,0x44483D39,0x45473F3D,0x47464243,0x48454548,
    0x1C2E301E,0x1D2E3222,0x1F2C3528,0x202B382C,0x22333222,0x23323426,0x2530382C,0x262F3A30,
    0x28373526,0x2936372A,0x2A343A30,0x2B343C34,0x2E3B372A,0x2F3A392E,0x30393C34,0x31383F38,
    0x333F392E,0x343E3C32,0x363D3F38,0x373C413C,0x39433C32,0x3A423E36,0x3C41413C,0x3D404440,
    0x3F473E36,0x4046403A,0x41454440,0x42444644,0x454C413A,0x464B433E,0x47494644,0x49494949,
};

// PAL yuyv palette, must be in RAM
const uint32_t pal_yuyv[] = {
    0x18181818,0x1A16191E,0x1E121A26,0x21101A2C,0x1E1D1A1B,0x211B1A20,0x25171B29,0x27151C2E,
    0x25231B1E,0x27201C23,0x2B1D1D2B,0x2E1A1E31,0x2B281D20,0x2E261E26,0x31221F2E,0x34202034,
    0x322D1F23,0x342B2029,0x38282131,0x3A252137,0x38332126,0x3A30212B,0x3E2D2234,0x412A2339,
    0x3E382229,0x4136232E,0x44322436,0x4730253C,0x453E242C,0x483C2531,0x4B382639,0x4E36273F,
    0x171B1D19,0x1A181E1F,0x1D151F27,0x20121F2D,0x1E201F1C,0x201E1F22,0x241A202A,0x26182130,
    0x2425201F,0x27232124,0x2A20222D,0x2D1D2332,0x2A2B2222,0x2D282327,0x3125242F,0x33222435,
    0x31302424,0x332E242A,0x372A2632,0x3A282638,0x37362627,0x3A33262D,0x3D302735,0x402D283B,
    0x3E3B272A,0x4039282F,0x44352938,0x46332A3D,0x4441292D,0x473E2A32,0x4B3B2B3B,0x4D382C40,
    0x171D221B,0x191B2220,0x1D182329,0x1F15242E,0x1D23231E,0x1F202423,0x231D252B,0x261A2631,
    0x23282520,0x26262626,0x2A22272E,0x2C202834,0x2A2E2723,0x2C2B2829,0x30282931,0x33252937,
    0x30332926,0x3331292B,0x362D2A34,0x392B2B39,0x36382A29,0x39362B2E,0x3D322C36,0x3F302D3C,
    0x3D3E2C2B,0x3F3B2D31,0x43382E39,0x46352F3F,0x44432E2E,0x46412F34,0x4A3E303C,0x4D3B3042,
    0x1620271C,0x181E2722,0x1C1A282A,0x1F182930,0x1C26281F,0x1F232924,0x22202A2D,0x251D2B32,
    0x232B2A22,0x25292B27,0x29252C30,0x2B232C35,0x29302C24,0x2C2E2C2A,0x2F2A2D32,0x32282E38,
    0x2F362D27,0x32332E2D,0x36302F35,0x382D303B,0x363B2F2A,0x38393030,0x3C353138,0x3F33323E,
    0x3C40312D,0x3F3E3232,0x423A333B,0x45383340,0x43463330,0x46443435,0x4940353E,0x4C3E3543,
    0x15232B1E,0x18212C23,0x1B1D2D2B,0x1E1B2E31,0x1C282D20,0x1E262E26,0x22222F2E,0x24202F34,
    0x222E2F23,0x242B3029,0x28283131,0x2B253137,0x28333126,0x2B31312B,0x2F2D3234,0x312B3339,
    0x2F383229,0x3136332E,0x35323436,0x3730353C,0x353E342B,0x383B3531,0x3B383639,0x3E35363F,
    0x3B43362E,0x3E413634,0x423D373C,0x443B3842,0x42493831,0x45473837,0x4943393F,0x4B413A45,
    0x1526301F,0x17233125,0x1B20322D,0x1D1D3333,0x1B2B3222,0x1D293327,0x21253430,0x24233435,
    0x21303425,0x242E342A,0x272A3532,0x2A283638,0x28363527,0x2A33362D,0x2E303735,0x302D383B,
    0x2E3B372A,0x30393830,0x34353938,0x37333A3E,0x3440392D,0x373E3A32,0x3B3B3B3B,0x3D383B40,
    0x3B463B30,0x3D433B35,0x41403C3D,0x443D3D43,0x424C3D33,0x44493D38,0x48463E40,0x4A433F46,
    0x14283520,0x16263626,0x1A23372E,0x1D203734,0x1A2E3723,0x1D2B3729,0x20283831,0x23253937,
    0x21333826,0x2331392B,0x272D3A34,0x292B3B39,0x27383A29,0x29363B2E,0x2D333C36,0x30303D3C,
    0x2D3E3C2B,0x303B3D31,0x34383E39,0x36363E3F,0x34433E2E,0x36413E34,0x3A3D3F3C,0x3C3B4042,
    0x3A493F31,0x3D464036,0x4043413F,0x43404244,0x414E4134,0x434C4239,0x47484342,0x4A464447,
    0x132B3A22,0x16293B27,0x19253C30,0x1C233D35,0x19313C25,0x1C2E3D2A,0x202B3E32,0x22283E38,
    0x20363E27,0x22343E2D,0x26303F35,0x292E403B,0x263B3F2A,0x29394030,0x2C364138,0x2F33423E,
    0x2D41412D,0x2F3E4232,0x333B433B,0x35384440,0x33464330,0x35444435,0x3940453E,0x3C3E4543,
    0x394C4533,0x3C494538,0x40464640,0x42434746,0x40514735,0x434F473B,0x464B4843,0x49494949,
    //odd
    0x18181818,0x19161A1E,0x1A121E26,0x1A10212C,0x1A1D1E1B,0x1A1B2120,0x1B172529,0x1C15272E,
    0x1B23251E,0x1C202723,0x1D1D2B2B,0x1E1A2E31,0x1D282B20,0x1E262E26,0x1F22312E,0x20203434,
    0x1F2D3223,0x202B3429,0x21283831,0x21253A37,0x21333826,0x21303A2B,0x222D3E34,0x232A4139,
    0x22383E29,0x2336412E,0x24324436,0x2530473C,0x243E452C,0x253C4831,0x26384B39,0x27364E3F,
    0x1D1B1719,0x1E181A1F,0x1F151D27,0x1F12202D,0x1F201E1C,0x1F1E2022,0x201A242A,0x21182630,
    0x2025241F,0x21232724,0x22202A2D,0x231D2D32,0x222B2A22,0x23282D27,0x2425312F,0x24223335,
    0x24303124,0x242E332A,0x262A3732,0x26283A38,0x26363727,0x26333A2D,0x27303D35,0x282D403B,
    0x273B3E2A,0x2839402F,0x29354438,0x2A33463D,0x2941442D,0x2A3E4732,0x2B3B4B3B,0x2C384D40,
    0x221D171B,0x221B1920,0x23181D29,0x24151F2E,0x23231D1E,0x24201F23,0x251D232B,0x261A2631,
    0x25282320,0x26262626,0x27222A2E,0x28202C34,0x272E2A23,0x282B2C29,0x29283031,0x29253337,
    0x29333026,0x2931332B,0x2A2D3634,0x2B2B3939,0x2A383629,0x2B36392E,0x2C323D36,0x2D303F3C,
    0x2C3E3D2B,0x2D3B3F31,0x2E384339,0x2F35463F,0x2E43442E,0x2F414634,0x303E4A3C,0x303B4D42,
    0x2720161C,0x271E1822,0x281A1C2A,0x29181F30,0x28261C1F,0x29231F24,0x2A20222D,0x2B1D2532,
    0x2A2B2322,0x2B292527,0x2C252930,0x2C232B35,0x2C302924,0x2C2E2C2A,0x2D2A2F32,0x2E283238,
    0x2D362F27,0x2E33322D,0x2F303635,0x302D383B,0x2F3B362A,0x30393830,0x31353C38,0x32333F3E,
    0x31403C2D,0x323E3F32,0x333A423B,0x33384540,0x33464330,0x34444635,0x3540493E,0x353E4C43,
    0x2B23151E,0x2C211823,0x2D1D1B2B,0x2E1B1E31,0x2D281C20,0x2E261E26,0x2F22222E,0x2F202434,
    0x2F2E2223,0x302B2429,0x31282831,0x31252B37,0x31332826,0x31312B2B,0x322D2F34,0x332B3139,
    0x32382F29,0x3336312E,0x34323536,0x3530373C,0x343E352B,0x353B3831,0x36383B39,0x36353E3F,
    0x36433B2E,0x36413E34,0x373D423C,0x383B4442,0x38494231,0x38474537,0x3943493F,0x3A414B45,
    0x3026151F,0x31231725,0x32201B2D,0x331D1D33,0x322B1B22,0x33291D27,0x34252130,0x34232435,
    0x34302125,0x342E242A,0x352A2732,0x36282A38,0x35362827,0x36332A2D,0x37302E35,0x382D303B,
    0x373B2E2A,0x38393030,0x39353438,0x3A33373E,0x3940342D,0x3A3E3732,0x3B3B3B3B,0x3B383D40,
    0x3B463B30,0x3B433D35,0x3C40413D,0x3D3D4443,0x3D4C4233,0x3D494438,0x3E464840,0x3F434A46,
    0x35281420,0x36261626,0x37231A2E,0x37201D34,0x372E1A23,0x372B1D29,0x38282031,0x39252337,
    0x38332126,0x3931232B,0x3A2D2734,0x3B2B2939,0x3A382729,0x3B36292E,0x3C332D36,0x3D30303C,
    0x3C3E2D2B,0x3D3B3031,0x3E383439,0x3E36363F,0x3E43342E,0x3E413634,0x3F3D3A3C,0x403B3C42,
    0x3F493A31,0x40463D36,0x4143403F,0x42404344,0x414E4134,0x424C4339,0x43484742,0x44464A47,
    0x3A2B1322,0x3B291627,0x3C251930,0x3D231C35,0x3C311925,0x3D2E1C2A,0x3E2B2032,0x3E282238,
    0x3E362027,0x3E34222D,0x3F302635,0x402E293B,0x3F3B262A,0x40392930,0x41362C38,0x42332F3E,
    0x41412D2D,0x423E2F32,0x433B333B,0x44383540,0x43463330,0x44443535,0x4540393E,0x453E3C43,
    0x454C3933,0x45493C38,0x46464040,0x47434246,0x47514035,0x474F433B,0x484B4643,0x49494949,
};

//====================================================================================================
//====================================================================================================

uint32_t cpu_ticks()
{
  return xthal_get_ccount();
}

uint32_t us() {
    return cpu_ticks()/240;
}

// Color clock frequency is 315/88 (3.57954545455)
// DAC_MHZ is 315/11 or 8x color clock
// 455/2 color clocks per line, round up to maintain phase
// HSYNCH period is 44/315*455 or 63.55555..us
// Field period is 262*44/315*455 or 16651.5555us

#define IRE(_x)          ((uint32_t)(((_x)+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)


#define P0 (color >> 16)
#define P1 (color >> 8)
#define P2 (color)
#define P3 (color << 8)

uint8_t** _lines; // filled in by emulator
volatile int _line_counter = 0;
volatile int _frame_counter = 0;

int _active_lines;
int _line_count;

int _line_width;
int _samples_per_cc;
const uint32_t* _palette;

float _sample_rate;

int _hsync;
int _hsync_long;
int _hsync_short;
int _burst_start;
int _burst_width;
int _active_start;

int16_t* _burst0 = 0; // pal bursts
int16_t* _burst1 = 0;

static int usec(float us)
{
    uint32_t r = (uint32_t)(us*_sample_rate);
    return ((r + _samples_per_cc)/(_samples_per_cc << 1))*(_samples_per_cc << 1);  // multiple of color clock, word align
}

#define NTSC_COLOR_CLOCKS_PER_SCANLINE 228       // really 227.5 for NTSC but want to avoid half phase fiddling for now
#define NTSC_FREQUENCY (315000000.0/88)
#define NTSC_LINES 262

#define PAL_COLOR_CLOCKS_PER_SCANLINE 284        // really 283.75 ?
#define PAL_FREQUENCY 4433618.75
#define PAL_LINES 312

void pal_init();

void video_init(int samples_per_cc, int ntsc)
{
    _samples_per_cc = samples_per_cc;

    if (ntsc) {
        _sample_rate = 315.0/88 * samples_per_cc;   // DAC rate
        _line_width = NTSC_COLOR_CLOCKS_PER_SCANLINE*samples_per_cc;
        _line_count = NTSC_LINES;
        _hsync_long = usec(63.555-4.7);
        _active_start = usec(samples_per_cc == 4 ? 10 : 10.5);
        _hsync = usec(4.7);
        _palette = ntsc_RGB332;
        _pal_ = 0;
    } else {
        pal_init();
        _palette = pal_yuyv;
        _pal_ = 1;
    }

    _active_lines = 240;
    video_init_hw(_line_width,_samples_per_cc);    // init the hardware
}

//===================================================================================================
//===================================================================================================
// PAL

void pal_init()
{
    int cc_width = 4;
    _sample_rate = PAL_FREQUENCY*cc_width/1000000.0;       // DAC rate in mhz
    _line_width = PAL_COLOR_CLOCKS_PER_SCANLINE*cc_width;
    _line_count = PAL_LINES;
    _hsync_short = usec(2);
    _hsync_long = usec(30);
    _hsync = usec(4.7);
    _burst_start = usec(5.6);
    _burst_width = (int)(10*cc_width + 4) & 0xFFFE;
    _active_start = usec(10.4);

    // make colorburst tables for even and odd lines
    _burst0 = new int16_t[_burst_width];
    _burst1 = new int16_t[_burst_width];
    float phase = 2*M_PI/2;
    for (int i = 0; i < _burst_width; i++)
    {
        _burst0[i] = BLANKING_LEVEL + sin(phase + 3*M_PI/4) * BLANKING_LEVEL/1.5;
        _burst1[i] = BLANKING_LEVEL + sin(phase - 3*M_PI/4) * BLANKING_LEVEL/1.5;
        phase += 2*M_PI/cc_width;
    }
}

void IRAM_ATTR blit_pal(uint8_t* src, uint16_t* dst)
{
    uint32_t c,color;
    bool even = _line_counter & 1;
    const uint32_t* p = even ? _palette : _palette + 256;
    int left = 0;
    int right = 256;
    uint8_t mask = 0xFF;
    uint8_t c0,c1,c2,c3,c4;
    uint8_t y1,y2,y3;

    // 192 of 288 color clocks wide: roughly correct aspect ratio
    dst += 88;

    // 4 pixels over 3 color clocks, 12 samples
    // do the blitting
    for (int i = left; i < right; i += 4) {
        c = *((uint32_t*)(src+i));
        color = p[c & mask];
        dst[0^1] = P0;
        dst[1^1] = P1;
        dst[2^1] = P2;
        color = p[(c >> 8) & mask];
        dst[3^1] = P3;
        dst[4^1] = P0;
        dst[5^1] = P1;
        color = p[(c >> 16) & mask];
        dst[6^1] = P2;
        dst[7^1] = P3;
        dst[8^1] = P0;
        color = p[(c >> 24) & mask];
        dst[9^1] = P1;
        dst[10^1] = P2;
        dst[11^1] = P3;
        dst += 12;
    }
}

void IRAM_ATTR burst_pal(uint16_t* line)
{
    line += _burst_start;
    int16_t* b = (_line_counter & 1) ? _burst0 : _burst1;
    for (int i = 0; i < _burst_width; i += 2) {
        line[i^1] = b[i];
        line[(i+1)^1] = b[i+1];
    }
}

//===================================================================================================
//===================================================================================================
// ntsc tables
// AA AA                // 2 pixels, 1 color clock - atari
// AA AB BB             // 3 pixels, 2 color clocks - nes
// AAA ABB BBC CCC      // 4 pixels, 3 color clocks - sms

// cc == 3 gives 684 samples per line, 3 samples per cc, 3 pixels for 2 cc
// cc == 4 gives 912 samples per line, 4 samples per cc, 2 pixels per cc

#ifdef PERF
#define BEGIN_TIMING()  uint32_t t = cpu_ticks()
#define END_TIMING() t = cpu_ticks() - t; _blit_ticks_min = min(_blit_ticks_min,t); _blit_ticks_max = max(_blit_ticks_max,t);
#define ISR_BEGIN() uint32_t t = cpu_ticks()
#define ISR_END() t = cpu_ticks() - t;_isr_us += (t+120)/240;
uint32_t _blit_ticks_min = 0;
uint32_t _blit_ticks_max = 0;
uint32_t _isr_us = 0;
#else
#define BEGIN_TIMING()
#define END_TIMING()
#define ISR_BEGIN()
#define ISR_END()
#endif

// draw a line of game in NTSC
void IRAM_ATTR blit(uint8_t* src, uint16_t* dst)
{
    uint32_t* d = (uint32_t*)dst;
    const uint32_t* p = _palette;
    uint32_t color,c;
    uint32_t mask = 0xFF;
    int i;

    BEGIN_TIMING();
    if (_pal_) {
        blit_pal(src,dst);
        END_TIMING();
        return;
    }

    // AAA ABB BBC CCC
    // 4 pixels, 3 color clocks, 4 samples per cc
    // each pixel gets 3 samples, 192 color clocks wide
    for (i = 0; i < 256; i += 4) {
        c = *((uint32_t*)(src+i));
        color = p[c & mask];
        dst[0^1] = P0;
        dst[1^1] = P1;
        dst[2^1] = P2;
        color = p[(c >> 8) & mask];
        dst[3^1] = P3;
        dst[4^1] = P0;
        dst[5^1] = P1;
        color = p[(c >> 16) & mask];
        dst[6^1] = P2;
        dst[7^1] = P3;
        dst[8^1] = P0;
        color = p[(c >> 24) & mask];
        dst[9^1] = P1;
        dst[10^1] = P2;
        dst[11^1] = P3;
        dst += 12;
    }

    END_TIMING();
}

void IRAM_ATTR burst(uint16_t* line)
{
    if (_pal_) {
        burst_pal(line);
        return;
    }

    int i,phase;
    switch (_samples_per_cc) {
        case 4:
            // 4 samples per color clock
            for (i = _hsync; i < _hsync + (4*10); i += 4) {
                line[i+1] = BLANKING_LEVEL;
                line[i+0] = BLANKING_LEVEL + BLANKING_LEVEL/2;
                line[i+3] = BLANKING_LEVEL;
                line[i+2] = BLANKING_LEVEL - BLANKING_LEVEL/2;
            }
            break;
        case 3:
            // 3 samples per color clock
            phase = 0.866025*BLANKING_LEVEL/2;
            for (i = _hsync; i < _hsync + (3*10); i += 6) {
                line[i+1] = BLANKING_LEVEL;
                line[i+0] = BLANKING_LEVEL + phase;
                line[i+3] = BLANKING_LEVEL - phase;
                line[i+2] = BLANKING_LEVEL;
                line[i+5] = BLANKING_LEVEL + phase;
                line[i+4] = BLANKING_LEVEL - phase;
            }
            break;
    }
}

void IRAM_ATTR sync(uint16_t* line, int syncwidth)
{
    for (int i = 0; i < syncwidth; i++)
        line[i] = SYNC_LEVEL;
}

void IRAM_ATTR blanking(uint16_t* line, bool vbl)
{
    int syncwidth = vbl ? _hsync_long : _hsync;
    sync(line,syncwidth);
    for (int i = syncwidth; i < _line_width; i++)
        line[i] = BLANKING_LEVEL;
    if (!vbl)
        burst(line);    // no burst during vbl
}

// Fancy pal non-interlace
// http://martin.hinner.info/vga/pal.html
void IRAM_ATTR pal_sync2(uint16_t* line, int width, int swidth)
{
    swidth = swidth ? _hsync_long : _hsync_short;
    int i;
    for (i = 0; i < swidth; i++)
        line[i] = SYNC_LEVEL;
    for (; i < width; i++)
        line[i] = BLANKING_LEVEL;
}

uint8_t DRAM_ATTR _sync_type[8] = {0,0,0,3,3,2,0,0};
void IRAM_ATTR pal_sync(uint16_t* line, int i)
{
    uint8_t t = _sync_type[i-304];
    pal_sync2(line,_line_width/2, t & 2);
    pal_sync2(line+_line_width/2,_line_width/2, t & 1);
}

//  audio is buffered as 6 bit unsigned samples
uint8_t _audio_buffer[1024];
uint32_t _audio_r = 0;
uint32_t _audio_w = 0;
void audio_write_16(const int16_t* s, int len, int channels)
{
    int b;
    while (len--) {
        if (_audio_w == (_audio_r + sizeof(_audio_buffer)))
            break;
        if (channels == 2) {
            b = (s[0] + s[1]) >> 9;
            s += 2;
        } else
            b = *s++ >> 8;
        if (b < -32) b = -32;
        if (b > 31) b = 31;
        _audio_buffer[_audio_w++ & (sizeof(_audio_buffer)-1)] = b + 32;
    }
}

// test pattern, must be ram
uint8_t _sin64[64] = {
    0x20,0x22,0x25,0x28,0x2B,0x2E,0x30,0x33,
    0x35,0x37,0x38,0x3A,0x3B,0x3C,0x3D,0x3D,
    0x3D,0x3D,0x3D,0x3C,0x3B,0x3A,0x38,0x37,
    0x35,0x33,0x30,0x2E,0x2B,0x28,0x25,0x22,
    0x20,0x1D,0x1A,0x17,0x14,0x11,0x0F,0x0C,
    0x0A,0x08,0x07,0x05,0x04,0x03,0x02,0x02,
    0x02,0x02,0x02,0x03,0x04,0x05,0x07,0x08,
    0x0A,0x0C,0x0F,0x11,0x14,0x17,0x1A,0x1D,
};
uint8_t _x;

// test the fancy DAC
void IRAM_ATTR test_wave(volatile void* vbuf, int t = 1)
{
    uint16_t* buf = (uint16_t*)vbuf;
    int n = _line_width;
    switch (t) {
        case 0: // f/64 sinewave
            for (int i = 0; i < n; i += 2) {
                buf[0^1] = GRAY_LEVEL + (_sin64[_x++ & 0x3F] << 8);
                buf[1^1] = GRAY_LEVEL + (_sin64[_x++ & 0x3F] << 8);
                buf += 2;
            }
            break;
        case 1: // fast square wave
            for (int i = 0; i < n; i += 2) {
                buf[0^1] = GRAY_LEVEL - (0x10 << 8);
                buf[1^1] = GRAY_LEVEL + (0x10 << 8);
                buf += 2;
            }
            break;
    }
}

// Wait for blanking before starting drawing
// avoids tearing in our unsynchonized world
#ifdef ESP_PLATFORM
void video_sync()
{
  if (!_lines)
    return;
  int n = 0;
  if (_pal_) {
    if (_line_counter < _active_lines)
      n = (_active_lines - _line_counter)*1000/15600;
  } else {
    if (_line_counter < _active_lines)
      n = (_active_lines - _line_counter)*1000/15720;
  }
  vTaskDelay(n+1);
}
#endif

// Workhorse ISR handles audio and video updates
extern "C"
void IRAM_ATTR video_isr(volatile void* vbuf)
{
    if (!_lines)
        return;

    ISR_BEGIN();

    uint8_t s = _audio_r < _audio_w ? _audio_buffer[_audio_r++ & (sizeof(_audio_buffer)-1)] : 0x20;
    audio_sample(s);
    //audio_sample(_sin64[_x++ & 0x3F]);

    int i = _line_counter++;
    uint16_t* buf = (uint16_t*)vbuf;
    if (_pal_) {
        // pal
        if (i < 32) {
            blanking(buf,false);                // pre render/black 0-32
        } else if (i < _active_lines + 32) {    // active video 32-272
            sync(buf,_hsync);
            burst(buf);
            blit(_lines[i-32],buf + _active_start);
        } else if (i < 304) {                   // post render/black 272-304
            if (i < 272)                        // slight optimization here, once you have 2 blanking buffers
                blanking(buf,false);
        } else {
            pal_sync(buf,i);                    // 8 lines of sync 304-312
        }
    } else {
        // ntsc
        if (i < _active_lines) {                // active video
            sync(buf,_hsync);
            burst(buf);
            blit(_lines[i],buf + _active_start);

        } else if (i < (_active_lines + 5)) {   // post render/black
            blanking(buf,false);

        } else if (i < (_active_lines + 8)) {   // vsync
            blanking(buf,true);

        } else {                                // pre render/black
            blanking(buf,false);
        }
    }

    if (_line_counter == _line_count) {
        _line_counter = 0;                      // frame is done
        _frame_counter++;
    }

    ISR_END();
}
