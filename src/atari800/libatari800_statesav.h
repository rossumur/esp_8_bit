#ifndef LIBATARI800_STATESAV_H_
#define LIBATARI800_STATESAV_H_

#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "atari.h"
#include "statesav.h"
#include "libatari800.h"

extern UBYTE *LIBATARI800_StateSav_buffer;
extern statesav_tags_t *LIBATARI800_StateSav_tags;

void LIBATARI800_StateSave(UBYTE *buffer, statesav_tags_t *tags);
void LIBATARI800_StateLoad(UBYTE *buffer);

#endif /* LIBATARI800_STATESAV_H_ */
