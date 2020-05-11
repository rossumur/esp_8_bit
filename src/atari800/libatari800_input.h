#ifndef LIBATARI800_INPUT_H_
#define LIBATARI800_INPUT_H_

#include "atari.h"
#include "libatari800.h"

#define LIBATARI800_FLAG_DELTA_MOUSE 0
#define LIBATARI800_FLAG_DIRECT_MOUSE 1

extern input_template_t *LIBATARI800_Input_array;

int LIBATARI800_Input_Initialise(int *argc, char *argv[]);

void LIBATARI800_Mouse(void);

#endif /* LIBATARI800_INPUT_H_ */
