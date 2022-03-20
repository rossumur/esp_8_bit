#pragma GCC optimize ("O2")

#include "shared.h"

/* Background drawing function */
void (*render_bg)(int line);

/* Pointer to output buffer */
uint8 *linebuf;

//Each tile takes up 8*8=64 bytes. We have 512 tiles * 4 attribs, so 2K tiles max.
#define CACHEDTILES 512
#define ALIGN_DWORD 1 //esp doesn't support unaligned word writes

int16 cachePtr[512*4];				//(tile+attr<<9) -> cache tile store index (i<<6); -1 if not cached
uint8 cacheStore[CACHEDTILES*64];	//Tile store
uint8 cacheStoreUsed[CACHEDTILES];	//Marks if a tile is used

uint8 is_vram_dirty;

int cacheKillPtr=0;
int freePtr=0;

/* Pixel look-up table */
//uint8 lut[0x10000];
#include "lut.h"

/* Attribute expansion table */
uint32 atex[4] =
{
    0x00000000,
    0x10101010,
    0x20202020,
    0x30303030,
};

/* Display sizes */
int vp_vstart;
int vp_vend;
int vp_hstart;
int vp_hend;

void render_bg_sms(int line);
void render_bg_gg(int line);
void render_obj(int line);
void palette_sync(int index);
void render_reset(void);
void render_init(void);

void vramMarkTileDirty(int index) {
	int i=index;
	while (i<0x800) {
		if (cachePtr[i]!=-1) {
			freePtr=cachePtr[i]>>6;
//			printf("Freeing cache loc %d for tile %d\n", freePtr, index);
			cacheStoreUsed[freePtr]=0;
			cachePtr[i]=-1;
		}
		i+=0x200;
	}
}

uint8 *getCache(int tile, int attr) {
    int n, i, x, y, c;
    int b0, b1, b2, b3;
    int i0, i1, i2, i3;
	int p;
	//See if we have this in cache.
	if (cachePtr[tile+(attr<<9)]!=-1) return &cacheStore[cachePtr[tile+(attr<<9)]];

	//Nope! Generate cache tile.
	//Find free cache idx first.
	do {
		i=freePtr;
		n=0;
		while (cacheStoreUsed[i] && n<CACHEDTILES) {
			i++;
			n++;
			if (i==CACHEDTILES) i=0;
		}

		if (n==CACHEDTILES) {
			//printf("Eek, tile cache overflow\n");
			//Crap, out of cache. Kill a tile.
			vramMarkTileDirty(cacheKillPtr++);
			if (cacheKillPtr>=512) cacheKillPtr=0;
			i=freePtr;
		}
	} while (cacheStoreUsed[i]);
	//Okay, somehow we have a free cache tile in i now.
	cacheStoreUsed[i]=1;
	cachePtr[tile+(attr<<9)]=i<<6;

//	printf("Generating cache loc %d for tile %d attr %d\n", i, tile, attr);
	//Calculate tile
	for(y = 0; y < 8; y += 1) {
		b0 = vdp.vram[(tile << 5) | (y << 2) | (0)];
		b1 = vdp.vram[(tile << 5) | (y << 2) | (1)];
		b2 = vdp.vram[(tile << 5) | (y << 2) | (2)];
		b3 = vdp.vram[(tile << 5) | (y << 2) | (3)];
		for(x = 0; x < 8; x += 1) {
			i0 = (b0 >> (x ^ 7)) & 1;
			i1 = (b1 >> (x ^ 7)) & 1;
			i2 = (b2 >> (x ^ 7)) & 1;
			i3 = (b3 >> (x ^ 7)) & 1;

			c = (i3 << 3 | i2 << 2 | i1 << 1 | i0);
			if (attr==0) cacheStore[(i<<6)|(y<<3)|(x)]=c;
			if (attr==1) cacheStore[(i<<6)|(y<<3)|(x^7)]=c;
			if (attr==2) cacheStore[(i<<6)|((y^7)<<3)|(x)]=c;
			if (attr==3) cacheStore[(i<<6)|((y^7)<<3)|(x^7)]=c;

		}
	}
	return &cacheStore[i<<6];
}


/* Macros to access memory 32-bits at a time (from MAME's drawgfx.c) */

#ifdef ALIGN_DWORD

static __inline__ uint32 read_dword(void *address)
{
    if ((uint32)address & 3)
	{
#ifdef LSB_FIRST  /* little endian version */
        return ( *((uint8 *)address) +
                (*((uint8 *)address+1) << 8)  +
                (*((uint8 *)address+2) << 16) +
                (*((uint8 *)address+3) << 24) );
#else             /* big endian version */
        return ( *((uint8 *)address+3) +
                (*((uint8 *)address+2) << 8)  +
                (*((uint8 *)address+1) << 16) +
                (*((uint8 *)address)   << 24) );
#endif
	}
	else
        return *(uint32 *)address;
}


static __inline__ void write_dword(void *address, uint32 data)
{
    if ((uint32)address & 3)
	{
#ifdef LSB_FIRST
            *((uint8 *)address) =    data;
            *((uint8 *)address+1) = (data >> 8);
            *((uint8 *)address+2) = (data >> 16);
            *((uint8 *)address+3) = (data >> 24);
#else
            *((uint8 *)address+3) =  data;
            *((uint8 *)address+2) = (data >> 8);
            *((uint8 *)address+1) = (data >> 16);
            *((uint8 *)address)   = (data >> 24);
#endif
		return;
  	}
  	else
        *(uint32 *)address = data;
}
#else
#define read_dword(address) *(uint32 *)address
#define write_dword(address,data) *(uint32 *)address=data
#endif


/****************************************************************************/


/* Initialize the rendering data */
void render_init(void)
{
#if 0
    int bx, sx, b, s, bp, bf, sf, c;

    /* Generate 64k of data for the look up table */
    for(bx = 0; bx < 0x100; bx += 1)
    {
        for(sx = 0; sx < 0x100; sx += 1)
        {
            /* Background pixel */
            b  = (bx & 0x0F);

            /* Background priority */
            bp = (bx & 0x20) ? 1 : 0;

            /* Full background pixel + priority + sprite marker */
            bf = (bx & 0x7F);

            /* Sprite pixel */
            s  = (sx & 0x0F);

            /* Full sprite pixel, w/ palette and marker bits added */
            sf = (sx & 0x0F) | 0x10 | 0x40;

            /* Overwriting a sprite pixel ? */
            if(bx & 0x40)
            {
                /* Return the input */
                c = bf;
            }
            else
            {
                /* Work out priority and transparency for both pixels */
                c = bp ? b ? bf : s ? sf : bf : s ? sf : bf;
            }

            /* Store result */
            lut[(bx << 8) | (sx)] = c;
        }
    }

#endif
    render_reset();
}

//Avoid massive lookup table (in slow EEPROM), use CPU which is fast	//Corn
inline uint8_t render_pixel(uint8 bx, uint8 sx)
{
	if (bx & 0x40)
		return (bx & 0x7F);	//Return the input
	if (!(bx & 0x20))	//bp
	{
		if (!(sx & 0x0F))	//s
			return (bx & 0x7F);	//Return the input
		return (sx & 0x0F) | 0x10 | 0x40;
	}
	if ((bx & 0x0F) || !(sx & 0x0F))	//b & s
		return (bx & 0x7F);	//Return the input
	return (sx & 0x0F) | 0x10 | 0x40;
}

/* Reset the rendering data */
void render_reset(void)
{
    int i;

    /* Clear display bitmap */
    memset(bitmap.data, 0, bitmap.pitch * bitmap.height);

    /* Clear palette */
    for(i = 0; i < PALETTE_SIZE; i += 1)
    {
        palette_sync(i);
    }

    /* Invalidate pattern cache */
	for (i=0; i<512*4; i++) cachePtr[i]=-1;
	for (i=0; i<512; i++) vramMarkTileDirty(i);

    /* Set up viewport size */
    if(IS_GG)
    {
        vp_vstart = 24;
        vp_vend   = 168;
        vp_hstart = 6;
        vp_hend   = 26;
    }
    else
    {
        vp_vstart = 0;
        vp_vend   = 192;
        vp_hstart = 0;
        vp_hend   = 32;
    }

    /* Pick render routine */
    render_bg = IS_GG ? render_bg_gg : render_bg_sms;
}


/* Draw a line of the display */
static void render_332(const uint8_t* buf, int line);
static uint32_t linebuf_[256];	//make sure it aligns to 32bit

void render_line(int line)
{
    /* Ensure we're within the viewport range */
    if((line < vp_vstart) || (line >= vp_vend)) return;
   // if (line >= 192)
   //     return;

    /* Point to current line in output buffer */
    //linebuf = &bitmap.data[(line * bitmap.pitch)];
    linebuf = (uint8_t*)linebuf_;

    /* Blank line */
    if( (!(vdp.reg[1] & 0x40)) || (((vdp.reg[2] & 1) == 0) && (IS_SMS)))
    {
        memset(linebuf + (vp_hstart << 3), BACKDROP_COLOR, BMP_WIDTH);
    }
    else
    {
        /* Draw background */
        render_bg(line);

        /* Draw sprites */
        render_obj(line);

        /* Blank leftmost column of display */
        if(vdp.reg[0] & 0x20)
        {
            memset(linebuf, BACKDROP_COLOR, 8);
        }
    }
    render_332(linebuf,line);   // convert to 332 truecolor
}


/* Draw the Master System background */
void render_bg_sms(int line)
{
    int locked = 0;
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10)) ? 0 : (0x100 - vdp.reg[8]);
    int column = vp_hstart;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    int shift = (hscroll & 7);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - shift];
	uint8 *ctp;

    /* Draw first column (clipped) */
    if(shift)
    {
        int x, c, a;

        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;

        for(x = shift; x < 8; x += 1)
        {
			ctp=getCache((attr&0x1ff), (attr>>9)&3);
            c = ctp[(v_row) | (x)];
            linebuf[(0 - shift) + (x)  ] = ((c) | (a));
        }

        column += 1;
    }

    /* Draw a line of the background */
    for(; column < vp_hend; column += 1)
    {
        /* Stop vertical scrolling for leftmost eight columns */
        if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
        {
            locked = 1;
            v_row = (line & 7) << 3;
            nt = (uint16 *)&vdp.vram[((vdp.reg[2] << 10) & 0x3800) + ((line >> 3) << 6)];
        }

        /* Get name table attribute word */
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        /* Expand priority and palette bits */
        atex_mask = atex[(attr >> 11) & 3];

        /* Point to a line of pattern data in cache */
		ctp=getCache((attr&0x1ff), (attr>>9)&3);
        cache_ptr = (uint32 *)&ctp[(v_row)];
        
        /* Copy the left half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

        /* Copy the right half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }

    /* Draw last column (clipped) */
    if(shift)
    {
        int x, c, a;

        char *p = &linebuf[(0 - shift)+(column << 3)];

        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;

        for(x = 0; x < shift; x += 1)
        {
			ctp=getCache((attr&0x1ff), (attr>>9)&3);
            c = ctp[(v_row) | (x)];
            p[x] = ((c) | (a));
        }
    }
}


/* Draw the Game Gear background */
void render_bg_gg(int line)
{
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = (0x100 - vdp.reg[8]);
    int column;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - (hscroll & 7)];
	uint8_t *ctp;

    /* Draw a line of the background */
    for(column = vp_hstart; column <= vp_hend; column += 1)
    {
        /* Get name table attribute word */
        attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        /* Expand priority and palette bits */
        atex_mask = atex[(attr >> 11) & 3];

        /* Point to a line of pattern data in cache */
		ctp=getCache((attr&0x1ff), (attr>>9)&3);
        cache_ptr = (uint32 *)&ctp[(v_row)];

        /* Copy the left half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

        /* Copy the right half, adding the attribute bits in */
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }
}


/* Draw sprites */
void render_obj(int line)
{
    int i;
	uint8_t *ctp;

    /* Sprite count for current line (8 max.) */
    int count = 0;

    /* Sprite dimensions */
    int width = 8;
    int height = (vdp.reg[1] & 0x02) ? 16 : 8;

    /* Pointer to sprite attribute table */
    uint8 *st = (uint8 *)&vdp.vram[vdp.satb];

    /* Adjust dimensions for double size sprites */
    if(vdp.reg[1] & 0x01)
    {
        width *= 2;
        height *= 2;
    }

    /* Draw sprites in front-to-back order */
    for(i = 0; i < 64; i += 1)
    {
        /* Sprite Y position */
        int yp = st[i];

        /* End of sprite list marker? */
        if(yp == 208) return;

        /* Actual Y position is +1 */
        yp += 1;

        /* Wrap Y coordinate for sprites > 240 */
        if(yp > 240) yp -= 256;

        /* Check if sprite falls on current line */
        if((line >= yp) && (line < (yp + height)))
        {
            uint8 *linebuf_ptr;

            /* Width of sprite */
            int start = 0;
            int end = width;

            /* Sprite X position */
            int xp = st[0x80 + (i << 1)];

            /* Pattern name */
            int n = st[0x81 + (i << 1)];

            /* Bump sprite count */
            count += 1;

            /* Too many sprites on this line ? */
            if((vdp.limit) && (count == 9)) return;

            /* X position shift */
            if(vdp.reg[0] & 0x08) xp -= 8;

            /* Add MSB of pattern name */
            if(vdp.reg[6] & 0x04) n |= 0x0100;

            /* Mask LSB for 8x16 sprites */
            if(vdp.reg[1] & 0x02) n &= 0x01FE;

            /* Point to offset in line buffer */
            linebuf_ptr = (uint8 *)&linebuf[xp];

            /* Clip sprites on left edge */
            if(xp < 0)
            {
                start = (0 - xp);
            }

            /* Clip sprites on right edge */
            if((xp + width) > 256)        
            {
                end = (256 - xp);
            }

            /* Draw double size sprite */
            if(vdp.reg[1] & 0x01)
            {
                int x;
				ctp=getCache((n&0x1ff)+((line - yp) >> 3), (n>>9)&3);
                uint8 *cache_ptr = (uint8 *)&ctp[(((line - yp) >> 1) << 3)];

                /* Draw sprite line */
                for(x = start; x < end; x += 1)
                {
                    /* Source pixel from cache */
                    uint8 sp = cache_ptr[(x >> 1)];
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8 bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = render_pixel(bg ,sp);	//linebuf_ptr[x] = lut[(bg << 8) | (sp)];
    
                        /* Set sprite collision flag */
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            }
            else /* Regular size sprite (8x8 / 8x16) */
            {
                int x;
				ctp=getCache((n&0x1ff)+((line - yp) >> 3), (n>>9)&3);
                uint8 *cache_ptr = (uint8 *)&ctp[((line - yp) << 3)&0x38];

                /* Draw sprite line */
                for(x = start; x < end; x += 1)
                {
                    /* Source pixel from cache */
                    uint8 sp = cache_ptr[x];
    
                    /* Only draw opaque sprite pixels */
                    if(sp)
                    {
                        /* Background pixel from line buffer */
                        uint8 bg = linebuf_ptr[x];
    
                        /* Look up result */
                        linebuf_ptr[x] = render_pixel(bg ,sp);	//linebuf_ptr[x] = lut[(bg << 8) | (sp)];
    
                        /* Set sprite collision flag */
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            }
        }
    }
}


uint8_t cramd[0x20] = {0};  // in 3:3:2 r:g:b
void palette_sync(int index)
{
    int r, g, b;
    // https://segaretro.org/Palette#Game_Gear_palette
    if(IS_GG)
    {
        r = ((vdp.cram[(index << 1) | 0] >> 1) & 7) << 5;   // 9 bits
        g = ((vdp.cram[(index << 1) | 0] >> 5) & 7) << 2;
        b = ((vdp.cram[(index << 1) | 1] >> 2) & 3) >> 0;
    }
    else
    {
        r = ((vdp.cram[index] >> 0) & 3) << 6;              // 6 bits
        g = ((vdp.cram[index] >> 2) & 3) << 3;
        b = ((vdp.cram[index] >> 4) & 3) << 0;
    }
	cramd[index] = r | g | b; // rrrgggbb     // 8 bit true color
}

static void render_332(const uint8_t* buf, int line)
{
    uint8_t* dst = bitmap.data + 256*line;
#if 0
    for(int i = 0; i < 256; i++)
        dst[i] = cramd[buf[i] & 0x1F];   // 666
#endif
    buf += vp_hstart*8;
    dst += vp_hstart*8;
    int n = (vp_hend-vp_hstart)*2;
    const uint32_t* s = (const uint32_t*)buf;
    uint32_t* d = (uint32_t*)dst;
    for(int i = 0; i < n; i++) {
        uint32_t b = *s++;
        *d++ = cramd[b & 0x1F] | (cramd[(b >> 8) & 0x1F] << 8) | (cramd[(b >> 16) & 0x1F] << 16) | (cramd[(b >> 24) & 0x1F] << 24);
    }
}




