
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
#include "smsplus/shared.h"
#include "smsplus/system.h"
#include "smsplus/sms.h"
};

using namespace std;

// used by sim, rgb332 to rgb888
const uint32_t sms_palette_rgb[0x100] = {
    0x00000000,0x00000048,0x000000B4,0x000000FF,0x00002400,0x00002448,0x000024B4,0x000024FF,
    0x00004800,0x00004848,0x000048B4,0x000048FF,0x00006C00,0x00006C48,0x00006CB4,0x00006CFF,
    0x00009000,0x00009048,0x000090B4,0x000090FF,0x0000B400,0x0000B448,0x0000B4B4,0x0000B4FF,
    0x0000D800,0x0000D848,0x0000D8B4,0x0000D8FF,0x0000FF00,0x0000FF48,0x0000FFB4,0x0000FFFF,
    0x00240000,0x00240048,0x002400B4,0x002400FF,0x00242400,0x00242448,0x002424B4,0x002424FF,
    0x00244800,0x00244848,0x002448B4,0x002448FF,0x00246C00,0x00246C48,0x00246CB4,0x00246CFF,
    0x00249000,0x00249048,0x002490B4,0x002490FF,0x0024B400,0x0024B448,0x0024B4B4,0x0024B4FF,
    0x0024D800,0x0024D848,0x0024D8B4,0x0024D8FF,0x0024FF00,0x0024FF48,0x0024FFB4,0x0024FFFF,
    0x00480000,0x00480048,0x004800B4,0x004800FF,0x00482400,0x00482448,0x004824B4,0x004824FF,
    0x00484800,0x00484848,0x004848B4,0x004848FF,0x00486C00,0x00486C48,0x00486CB4,0x00486CFF,
    0x00489000,0x00489048,0x004890B4,0x004890FF,0x0048B400,0x0048B448,0x0048B4B4,0x0048B4FF,
    0x0048D800,0x0048D848,0x0048D8B4,0x0048D8FF,0x0048FF00,0x0048FF48,0x0048FFB4,0x0048FFFF,
    0x006C0000,0x006C0048,0x006C00B4,0x006C00FF,0x006C2400,0x006C2448,0x006C24B4,0x006C24FF,
    0x006C4800,0x006C4848,0x006C48B4,0x006C48FF,0x006C6C00,0x006C6C48,0x006C6CB4,0x006C6CFF,
    0x006C9000,0x006C9048,0x006C90B4,0x006C90FF,0x006CB400,0x006CB448,0x006CB4B4,0x006CB4FF,
    0x006CD800,0x006CD848,0x006CD8B4,0x006CD8FF,0x006CFF00,0x006CFF48,0x006CFFB4,0x006CFFFF,
    0x00900000,0x00900048,0x009000B4,0x009000FF,0x00902400,0x00902448,0x009024B4,0x009024FF,
    0x00904800,0x00904848,0x009048B4,0x009048FF,0x00906C00,0x00906C48,0x00906CB4,0x00906CFF,
    0x00909000,0x00909048,0x009090B4,0x009090FF,0x0090B400,0x0090B448,0x0090B4B4,0x0090B4FF,
    0x0090D800,0x0090D848,0x0090D8B4,0x0090D8FF,0x0090FF00,0x0090FF48,0x0090FFB4,0x0090FFFF,
    0x00B40000,0x00B40048,0x00B400B4,0x00B400FF,0x00B42400,0x00B42448,0x00B424B4,0x00B424FF,
    0x00B44800,0x00B44848,0x00B448B4,0x00B448FF,0x00B46C00,0x00B46C48,0x00B46CB4,0x00B46CFF,
    0x00B49000,0x00B49048,0x00B490B4,0x00B490FF,0x00B4B400,0x00B4B448,0x00B4B4B4,0x00B4B4FF,
    0x00B4D800,0x00B4D848,0x00B4D8B4,0x00B4D8FF,0x00B4FF00,0x00B4FF48,0x00B4FFB4,0x00B4FFFF,
    0x00D80000,0x00D80048,0x00D800B4,0x00D800FF,0x00D82400,0x00D82448,0x00D824B4,0x00D824FF,
    0x00D84800,0x00D84848,0x00D848B4,0x00D848FF,0x00D86C00,0x00D86C48,0x00D86CB4,0x00D86CFF,
    0x00D89000,0x00D89048,0x00D890B4,0x00D890FF,0x00D8B400,0x00D8B448,0x00D8B4B4,0x00D8B4FF,
    0x00D8D800,0x00D8D848,0x00D8D8B4,0x00D8D8FF,0x00D8FF00,0x00D8FF48,0x00D8FFB4,0x00D8FFFF,
    0x00FF0000,0x00FF0048,0x00FF00B4,0x00FF00FF,0x00FF2400,0x00FF2448,0x00FF24B4,0x00FF24FF,
    0x00FF4800,0x00FF4848,0x00FF48B4,0x00FF48FF,0x00FF6C00,0x00FF6C48,0x00FF6CB4,0x00FF6CFF,
    0x00FF9000,0x00FF9048,0x00FF90B4,0x00FF90FF,0x00FFB400,0x00FFB448,0x00FFB4B4,0x00FFB4FF,
    0x00FFD800,0x00FFD848,0x00FFD8B4,0x00FFD8FF,0x00FFFF00,0x00FFFF48,0x00FFFFB4,0x00FFFFFF,
};  //

// ntsc phase representation of a rrrgggbb pixel
// must be in RAM so VBL works
uint32_t sms_4_phase[256] = {
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
uint32_t _sms_4_phase_pal[] = {
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
uint8_t* _smsplus_rom;

void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);

#define RGB_TO_YIQ( r, g, b, y, i ) (\
    (y = (r) * 0.299f + (g) * 0.587f + (b) * 0.114f),\
    (i = (r) * 0.596f - (g) * 0.275f - (b) * 0.321f),\
    ((r) * 0.212f - (g) * 0.523f + (b) * 0.311f)\
)

// signed uv difference signals scaled to 4 bits
int PIN4(float uv)
{
    int p = uv < 0 ? -(int)(-uv/24) : (int)uv/24;
    if (p < -7)
        p = -7;
    if (p > 7)
        p = 7;
    printf("%d\n",p);
    return p & 0XF;
}

// generate ntsc phase tables from RGB palette
// also generate pal yuyv palette
// https://segaretro.org/Palette#Game_Gear_palette

void make_yuv_palette(const char* name, const uint32_t* pal, int len);

static void gen_ntsc_pal_tables()
{
    uint32_t rgb[256];
    float yuv_rotation = 2*M_PI*33/360; // 33 degree rotation

    printf("uint32_t sms_4_phase[256] = {\n");
    int cc_width = 4;
    const uint8_t _p[8] = {0,36,72,108,144,180,216,255};
    for (int i = 0; i < 256; i++) {
        int r = i >> 5;
        int g = (i >> 2) & 0x7;
        int b = (i & 0x3) << 1;
        b |= (b >> 2);
        r = _p[r];
        g = _p[g];
        b = _p[b];
        rgb[i] = (r << 16) | (g << 8) | b;

        float y, ii, q = RGB_TO_YIQ( r, g, b, y, ii );
        float phase = atan2(ii,q);
        float phase_yuv = phase + yuv_rotation;
        float saturation = sqrt(ii*ii + q*q)/127;    // <== this is somewhat ad hoc TODO
        float offset = 2*M_PI*2/3;  // 270 deg

        /*
        // yuv 2 ways: rotation of YIQ or derived from RGB
        uint8_t u = 0.493*(b - y)*63;
        uint8_t v = 0.877*(r - y)*63;
        float uu = cos(phase_yuv)*saturation*63;
        float vv = sin(phase_yuv)*saturation*63;
        uint8_t p0 = atan2(0.436798*uu,0.614777*vv)*256/(2*M_PI) + 192;
        uint8_t p1 = atan2(0.436798*uu,-0.614777*vv)*256/(2*M_PI) + 192;
        int ci = (p0 << 24) | (p1 << 16);
        if (!mono)
            ci |= (int)(saturation + 0.5);
         */

        y /= 255;   // TODO?
        y = y*(WHITE_LEVEL-BLACK_LEVEL) + BLACK_LEVEL;
        int p[4] = {0};
        for (int j = 0; j < cc_width; j++)
            p[j] = y;   // 0x1D is really black, really BLANKING_LEVEL

        if ((r == g) && (g == b)) {
            //
        } else {
            if (cc_width == 4) {
                for (int j = 0; j < 4; j++)
                    p[j] += sin(phase + offset + 2*M_PI*j/4)*BLANKING_LEVEL/2*saturation;
            } else {
                for (int j = 0; j < 3; j++)
                    p[j] += sin(phase + offset + 2*M_PI*j/3)*BLANKING_LEVEL/2*saturation;
            }
        }

        uint32_t pi = 0;
        for (int j = 0; j < 4; j++)
            pi = (pi << 8) | p[j] >> 8;
        printf("0x%08X,",pi);
        if ((i & 7) == 7)
            printf("\n");
    }
    printf("};\n");

    make_yuv_palette("_sms",rgb,256);
}

static void gen_rgb_palette()
{
    const uint8_t _p[8] = {0,36,72,108,144,180,216,255};
    for (int i = 0; i < 256; i++) {
        int r = _p[i >> 5];
        int g = _p[(i >> 2) & 0x7];
        int b = _p[((i & 0x3) << 1)];
        uint32_t p = (r << 16) | (g << 8) | b;
        printf("0x%08X,",p);
        if ((i & 7) == 7)
            printf("\n");
    }
}

// big mem req
//uint8_t sms_videodata[256*240] = {0};   // larger than it needs to be at 60k
uint8_t* sms_videodata = 0;               // dynamically allocated
uint8_t sms_sram[0x8000];                 // 32k

const char* _sms_ext[] = {
    "sms",
    "gg",
    0
};

const char* _sms_help[] = {
    "Keyboard:",
    "  Arrow Keys - D-Pad",
    "  Left Shift - Button 1",
    "  Option     - Button 2",
    "  Return     - Start",
    "  Tab        - Select",
    "",
    "Wiimote (held sideways):",
    "  +          - Start",
    "  -          - Pause",
    "  + & -      - Reset",
    "  A,1        - Button 1",
    "  B,2        - Button 2",
    0
};

// https://www.smspower.org/Homebrew/Index
std::string to_string(int i);
class EmuSMSPlus : public Emu {
    uint8_t** _lines;
public:
    EmuSMSPlus(int ntsc) : Emu("smsplus",256,240,ntsc,(16 | (1 << 8)),4,EMU_SMS)    // audio is 16bit
    {
        _lines = 0;
        cart.rom = 0;
        _ext = _sms_ext;
        _help = _sms_help;
    }

    virtual void gen_palettes()
    {
        gen_rgb_palette();
        gen_ntsc_pal_tables();
    }

    virtual int info(const string& file, vector<string>& strs)
    {
        string ext = get_ext(file);
        uint8_t hdr[48];
        int len = Emu::head(file,hdr,sizeof(hdr));
        string name = file.substr(file.find_last_of("/") + 1);
        strs.push_back(name);
        strs.push_back("");
        if (ext == "gg")
            strs.push_back(::to_string((len + 0x3FF)/1024) + "k Sega Game Gear");
        else if (ext == "sms")
            strs.push_back(::to_string((len + 0x3FF)/1024) + "k Sega Master System");
        else
            strs.push_back(::to_string(len) + " bytes");
        return 0;
    }

    void init_screen()
    {
        printf("init_screen\n");
        sms_videodata = new uint8_t[256*240];
        bitmap.data = sms_videodata + 24*256;
        bitmap.width = 256;
        bitmap.height = 192;
        bitmap.pitch = 256;
        bitmap.depth = 8;
        sms.dummy = sms_videodata;
        sms.sram = sms_sram;

        // center on 240?
        _lines = new uint8_t*[240];
        const uint8_t* s = sms_videodata;
        for (int y = 0; y < 240; y++) {
            _lines[y] = (uint8_t*)s;
            s += 256;
        }
        clear_screen();
    }

    void clear_screen()
    {
        if (sms_videodata)
            memset(sms_videodata,0,256*240);
    }

    virtual int insert(const std::string& path, int flags, int disk_index)
    {
        if (!_lines)
            init_screen();
        else
            clear_screen();

        uint8_t buf[16];
        int len = head(path,buf,sizeof(buf));
        if (len <= 0)
            return -1;
        unmap_file(_smsplus_rom);
        _smsplus_rom = map_file(path.c_str(),len);

        cart.pages = ((len + 0x3FFF)/0x4000);
        cart.rom = _smsplus_rom;
        cart.type = get_ext(path) == "sms" ? TYPE_SMS : TYPE_GG;

        emu_system_init(audio_frequency);
        sms_init();
        return 0;
    }

    void pad(int i, int pressed, int mask)
    {
        input.pad[i] = pressed ? (input.pad[i] | mask) : (input.pad[i] & ~mask);
    }

    void system(int pressed, int mask)
    {
        input.system = pressed ? (input.system | mask) : (input.system & ~mask);
    }

    const uint32_t _common_sms[16] = {
        0,  // msb
        0,
        0,
        INPUT_START<<8, // PLUS
        INPUT_LEFT,       // UP
        INPUT_RIGHT,     // DOWN
        INPUT_UP,    // RIGHT
        INPUT_DOWN,     // LEFT

        0, // HOME
        0,
        0,
        INPUT_PAUSE<<8, // MINUS
        INPUT_BUTTON1,  // A
        INPUT_BUTTON2,  // B
        INPUT_BUTTON1, // ONE
        INPUT_BUTTON2, // TWO
    };

    const uint32_t _classic_sms[16] = {
        INPUT_RIGHT,    // RIGHT
        INPUT_DOWN,     // DOWN
        0,              // LEFT_TOP
        INPUT_PAUSE<<8, // MINUS
        0,              // HOME
        INPUT_START<<8, // PLUS
        0,              // RIGHT_TOP
        0,

        0,              // LOWER_LEFT
        INPUT_BUTTON2,  // B
        0,              // Y
        INPUT_BUTTON1,  // A
        0,              // X
        0,              // LOWER_RIGHT
        INPUT_LEFT,     // LEFT
        INPUT_UP        // UP
    };

    const uint32_t _generic_sms[16] = {
        0,                  // GENERIC_OTHER   0x8000
        0,                  // GENERIC_FIRE_X  0x4000  // RETCON
        0,                  // GENERIC_FIRE_Y  0x2000
        0,                  // GENERIC_FIRE_Z  0x1000

        INPUT_BUTTON1,      //GENERIC_FIRE_A  0x0800
        INPUT_BUTTON2,      //GENERIC_FIRE_B  0x0400
        0,                  //GENERIC_FIRE_C  0x0200
        0,                  //GENERIC_RESET   0x0100     // ATARI FLASHBACK

        INPUT_START<<8,     //GENERIC_START   0x0080
        INPUT_PAUSE<<8,     //GENERIC_SELECT  0x0040
        INPUT_BUTTON1,      //GENERIC_FIRE    0x0020
        INPUT_RIGHT,        //GENERIC_RIGHT   0x0010

        INPUT_LEFT,         //GENERIC_LEFT    0x0008
        INPUT_DOWN,         //GENERIC_DOWN    0x0004
        INPUT_UP,           //GENERIC_UP      0x0002
        0,                  //GENERIC_MENU    0x0001
    };

    // raw HID data. handle WII/IR mappings
    virtual void hid(const uint8_t* d, int len)
    {
        if (d[0] != 0x32 && d[0] != 0x42)
            return;
        bool ir = *d++ == 0x42;
        int reset = 0;
        for (int i = 0; i < 2; i++) {
            uint32_t p;
            if (ir) {
                int m = d[0] + (d[1] << 8);
                p = generic_map(m,_generic_sms);
                d += 2;
            } else
                p = wii_map(i,_common_sms,_classic_sms);
            input.pad[i] = p & 0xFF;
            if (i == 0)
                input.system = p >> 8;
            if ((p & (INPUT_PAUSE << 8)) && (p & (INPUT_START << 8)))   // start + pause == reset
                reset++;
        }
        if (reset)
            input.system = INPUT_SOFT_RESET;
    }

    virtual void key(int keycode, int pressed, int mods)
    {
        input.pad[0] = input.pad[1] = 0;
        input.system = 0;

        // joy 0
        switch (keycode) {
            case 82: pad(0,pressed,INPUT_UP); break;
            case 81: pad(0,pressed,INPUT_DOWN); break;
            case 80: pad(0,pressed,INPUT_LEFT); break;
            case 79: pad(0,pressed,INPUT_RIGHT); break;

            case 225: pad(0,pressed,INPUT_BUTTON1); break; // left shift key INPUT_BUTTON1
            case 226: pad(0,pressed,INPUT_BUTTON2); break; // option key INPUT_BUTTON2

            case 21: system(pressed,INPUT_SOFT_RESET); break; // soft reset - 'r'
            case 23: system(pressed,INPUT_HARD_RESET); break; // hard reset - 't'
            case 40: system(pressed,INPUT_START); break; // return
            case 43: system(pressed,INPUT_PAUSE); break; // tab

            case 59: system(pressed,INPUT_PAUSE); break; // F2
            case 61: system(pressed,INPUT_START); break; // F4
            case 62: system(pressed,((KEY_MOD_LSHIFT|KEY_MOD_RSHIFT) & mods) ? INPUT_HARD_RESET : INPUT_SOFT_RESET); break; // F5
        }
    }
            
    virtual int update()
    {
        if (_smsplus_rom)
            sms_frame(0);
        return 0;
    }

    virtual uint8_t** video_buffer()
    {
        return _lines;
    }

    static int16_t S16(int i)
    {
        return (i << 1) ^ 0x8000;
    }

    int _lp;
    virtual int audio_buffer(int16_t* b, int len)
    {
        int n = frame_sample_count();
        for (int i = 0; i < n; i++) {
            /*
            *b++ = S16(snd.buffer[0][i]);        // produces +ve only samples 0..32767
            *b++ = S16(snd.buffer[1][i]);
            */
            int s = (snd.buffer[0][i] + snd.buffer[1][i]) >> 1;
            _lp = (_lp*31 + s) >> 5;    // lo pass
            s -= _lp;                   // signed
            if (s < -32767) s = -32767; // clip
            if (s > 32767) s = 32767;
            *b++ = s;                   // centered signed 1 channel
        }
        return n;
    }

    virtual const uint32_t* ntsc_palette() { return sms_4_phase; };
    virtual const uint32_t* pal_palette() { return _sms_4_phase_pal; };
    virtual const uint32_t* rgb_palette() { return sms_palette_rgb; };

    virtual int make_default_media(const string& path)
    {
        unpack((path + "/ftrack.gg").c_str(),ftrack_gg,sizeof(ftrack_gg));
        unpack((path + "/baraburuu.sms").c_str(),baraburuu_sms,sizeof(baraburuu_sms));
        unpack((path + "/nanowars8k.sms").c_str(),nanowars8k_sms,sizeof(nanowars8k_sms));
        return 0;
    }
};

Emu* NewSMSPlus(int ntsc)
{
    return new EmuSMSPlus(ntsc);
}


