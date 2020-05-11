#ifndef LIBATARI800_SOUND_H_
#define LIBATARI800_SOUND_H_

#include <stdio.h>

        /* Sound sample rate - number of frames per second: 1000..65535. */
#define LIBATARI800_SOUND_FREQ 128

        /* Number of bytes per each sample, also determines sample format:
           1 = unsigned 8-bit format.
           2 = signed 16-bit system-endian format. */
#define LIBATARI800_SOUND_SAMPLE_SIZE 130

        /* Number of audio channels: 1 = mono, 2 = stereo. */
#define LIBATARI800_SOUND_CHANNELS 132

        /* Length of the hardware audio buffer in milliseconds. */
#define LIBATARI800_SOUND_BUFFER_MS 134

        /* Size of the hardware audio buffer in frames. Computed internally,
           equals freq * buffer_ms / 1000. */
#define LIBATARI800_SOUND_BUFFER_FRAMES 136


extern UBYTE *LIBATARI800_Sound_array;


#endif /* LIBATARI800_SOUND_H_ */
