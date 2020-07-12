/****************************************************************/
/*ESP pin map*/
/****************************************************************/
#define VIDEO_PIN   25	// Both 25 and 26 are locked to video output
#define AUDIO_PIN   18  // can be any pin
#define IR_PIN      0   // TSOP4838 or equivalent on any pin if desired

//NES OR SNES classic controller (wire colors might be different, double check!)
//       ___
//DATA  |o o| NC
//LATCH |o o| NC
//CLOCK |o o/ 3V3
//GND   |o_/
//       _
//3V3   |o|
//CLOCK |o|
//LATCH |o|
//DATA  |o|
//      |-|
//NC    |o|
//NC    |o|
//GND   |o|
//       -  

//NES and SNES controllers share the same pins, therefore both types can not be used at the same time
//Only the DATA pin goes to different IO pins for the two controllers the other pins are shared
//3V3 (red) (NOT 5V!)
//GND (white)
#define NES_CTRL_ADATA 21  //    # DATA controller A	(black)
#define NES_CTRL_BDATA 17  //    # DATA controller B	(black)
#define NES_CTRL_LATCH 27  //    # LATCH	(yellow)
#define NES_CTRL_CLK 22    //    # CLOCK 	(green)

/****************************************************************/
/*Controller support*/
/****************************************************************/
#define WEBTV_KEYBOARD
#define RETCON_CONTROLLER
#define FLASHBACK_CONTROLLER
//#define APPLE_TV_CONTROLLER
#define NES_CONTROLLER	//Enable only NES OR SNES not both!
//#define SNES_CONTROLLER	//Enable only NES OR SNES not both!

/****************************************************************/
/*Video levels*/
/****************************************************************/
#define SYNC_SIZE 		 40	//Sets the size of the sync pulses nominal is 40 but lower values (20) might help in some cases
#define IRE(_x)          ((uint32_t)(((_x)+SYNC_SIZE)*255/3.3/147.5) << 8)   // 3.3V DAC output
#define SYNC_LEVEL       IRE(-SYNC_SIZE)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(0)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)
