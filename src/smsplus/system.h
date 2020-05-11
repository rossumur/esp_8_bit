
#ifndef _SYSTEM_H_
#define _SYSTEM_H_

//void ym2413_write(int chip, int offset, int data);
//extern OPLL *opll;

#define PALETTE_SIZE        (0x20)

/* Console / cartridge types */
#define TYPE_SMS            (0)
#define TYPE_GG             (1)

#define IS_GG               (cart.type == TYPE_GG)
#define IS_SMS              (cart.type == TYPE_SMS)

/* Macro to get offset to actual display within bitmap */
#define BMP_X_OFFSET        ((cart.type == TYPE_GG) ? 48 : 0)
#define BMP_Y_OFFSET        ((cart.type == TYPE_GG) ? 24 : 0)

#define BMP_WIDTH           (IS_GG ? 160 : 256)
#define BMP_HEIGHT          (IS_GG ? 144 : 192)

/* Mask for removing unused pixel data */
#define PIXEL_MASK          (0x1F)

/* These can be used for 'input.pad[]' */
#define INPUT_UP          (0x00000001)
#define INPUT_DOWN        (0x00000002)
#define INPUT_LEFT        (0x00000004)
#define INPUT_RIGHT       (0x00000008)
#define INPUT_BUTTON2     (0x00000010)
#define INPUT_BUTTON1     (0x00000020)

/* These can be used for 'input.system' */
#define INPUT_START       (0x00000001)    /* Game Gear only */    
#define INPUT_PAUSE       (0x00000002)    /* Master System only */
#define INPUT_SOFT_RESET  (0x00000004)    /* Master System only */
#define INPUT_HARD_RESET  (0x00000008)    /* Works for either console type */

/* User input structure */
typedef struct
{
    int pad[2];
    int system;
}t_input;

/* Sound emulation structure */
typedef struct
{
    int enabled;
    int bufsize;
    signed short *buffer[2];
    signed short *fm_buffer;        /* internal use only */
    signed short *psg_buffer[2];    /* internal use only */
    int log;
    void (*callback)(int data);
}t_snd;

/* Game image structure */
typedef struct
{
    uint8 *rom;
    uint8 pages;
    uint8 type;
}t_cart;

/* Bitmap structure */
typedef struct
{
    unsigned char *data;
    int width;
    int height;
    int pitch;
    int depth;
    struct
    {
        uint8 color[32][3];
        uint8 dirty[32];
        uint8 update;
    }pal;
}t_bitmap;

/* Global variables */
extern t_bitmap bitmap;     /* Display bitmap */
extern t_snd snd;           /* Sound streams */
extern t_cart cart;         /* Game cartridge data */
extern t_input input;       /* Controller input */

/* Function prototypes */
void emu_system_init(int rate);
void system_shutdown(void);
void system_reset(void);
void system_load_sram(void);
void system_save_state(void *fd);
void system_load_state(void *fd);
void audio_init(int rate);

#endif /* _SYSTEM_H_ */
