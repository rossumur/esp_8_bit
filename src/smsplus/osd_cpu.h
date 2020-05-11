
#ifndef OSD_CPU_H
#define OSD_CPU_H

#define LSB_FIRST ////

typedef unsigned char						UINT8;
typedef unsigned short						UINT16;
typedef unsigned int						UINT32;

typedef signed char 						INT8;
typedef signed short						INT16;
typedef signed int							INT32;

typedef union {
#ifdef LSB_FIRST
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
#else
	struct { UINT8 h3,h2,h,l; } b;
	struct { UINT16 h,l; } w;
#endif
	UINT32 d;
}	PAIR;

#endif	/* defined OSD_CPU_H */
