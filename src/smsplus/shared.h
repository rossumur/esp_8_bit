#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdio.h>
//#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define VERSION     "0.9.4a"

#ifdef _MSC_VER
#include <stdio.h>
#include <string.h>
#ifndef __inline__
#define __inline__ __inline
#endif
#ifndef strcasecmp
#define strcasecmp stricmp
#endif
#endif

#include "types.h"
#include "z80.h"
#include "sms.h"
#include "vdp.h"
#include "render.h"
#include "sn76496.h"
#include "system.h"

char unalChar(const char *adr);

#endif /* _SHARED_H_ */
