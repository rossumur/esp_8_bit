#ifndef LIBATARI800_MAIN_H_
#define LIBATARI800_MAIN_H_

#include <stdio.h>

#include "config.h"

#ifdef HAVE_SETJMP
#include <setjmp.h>
extern jmp_buf libatari800_cpu_crash;
#endif /* HAVE_SETJMP */

#include "libatari800.h"

void LIBATARI800_Frame(void);

#endif /* LIBATARI800_VIDEO_H_ */
