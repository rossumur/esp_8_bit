#ifndef ROMS_ALTIRRA_5200_OS_H_
#define ROMS_ALTIRRA_5200_OS_H_

#include "config.h"
#include "atari.h"

extern UBYTE const ROM_altirra_5200_os[
#if EMUOS_ALTIRRA
                                       0x800
#else
                                       0x400 /* charset only */
#endif
                                      ];

#endif /* ROMS_ALTIRRA_5200_OS_H_ */
