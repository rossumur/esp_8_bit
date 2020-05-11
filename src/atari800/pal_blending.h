#ifndef PAL_BLENDING_H_
#define PAL_BLENDING_H_

#include "atari.h"

/* Updates blitter lookup tables according to the current display pixel
   format. Call after changing host video mode or after adjusting colours. */
void PAL_BLENDING_UpdateLookup(void);

/* Blit without scaling to a 16-BPP screen. */
void PAL_BLENDING_Blit16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd);
/* Blit without scaling to a 32-BPP screen. */
void PAL_BLENDING_Blit32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd);

/* Blit with scaling to a 16-BPP screen. */
void PAL_BLENDING_BlitScaled16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd);
/* Blit with scaling to a 32-BPP screen. */
void PAL_BLENDING_BlitScaled32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd);

#endif /* PAL_BLENDING_H_ */
