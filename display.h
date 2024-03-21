#pragma once

#include "config.h"

// Configure the desired display update rate. Use 120 for max performance/minimized latency, and 60/50/30/24 etc. for regular content, or to save battery.
#define TARGET_FRAME_RATE 60

#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

#define DISPLAY_NATIVE_WIDTH 240
#define DISPLAY_NATIVE_HEIGHT 240

#define InitSPIDisplay InitST7735R


// Unlike all other displays developed so far, Adafruit 1.54" 240x240 ST7789 display
// actually needs to observe the CS line toggle during execution, it cannot just be always activated.
// (ST7735R does not care about this)
// TODO: It is actually untested if ST7789VW really needs this, but does work with it, so kept for now
#define DISPLAY_NEEDS_CHIP_SELECT_SIGNAL



// The native display resolution is in portrait/landscape, but we want to display in the opposite landscape/portrait orientation?
// Compare DISPLAY_NATIVE_WIDTH <= DISPLAY_NATIVE_HEIGHT in the first test to let users toggle DISPLAY_OUTPUT_LANDSCAPE directive in config.h to flip orientation on square displays with width=height
#if ((DISPLAY_NATIVE_WIDTH <= DISPLAY_NATIVE_HEIGHT && defined(DISPLAY_OUTPUT_LANDSCAPE)) || (DISPLAY_NATIVE_WIDTH > DISPLAY_NATIVE_HEIGHT && !defined(DISPLAY_OUTPUT_LANDSCAPE)))
#define DISPLAY_SHOULD_FLIP_ORIENTATION
#endif

#define DISPLAY_WIDTH DISPLAY_NATIVE_WIDTH
#define DISPLAY_HEIGHT DISPLAY_NATIVE_HEIGHT

#ifndef DISPLAY_NATIVE_COVERED_LEFT_SIDE
#define DISPLAY_NATIVE_COVERED_LEFT_SIDE 0
#endif

#ifndef DISPLAY_NATIVE_COVERED_TOP_SIDE
#define DISPLAY_NATIVE_COVERED_TOP_SIDE 0
#endif

#ifndef DISPLAY_NATIVE_COVERED_BOTTOM_SIDE
#define DISPLAY_NATIVE_COVERED_BOTTOM_SIDE 0
#endif

#ifndef DISPLAY_NATIVE_COVERED_RIGHT_SIDE
#define DISPLAY_NATIVE_COVERED_RIGHT_SIDE 0
#endif

#define DISPLAY_COVERED_TOP_SIDE DISPLAY_NATIVE_COVERED_TOP_SIDE
#define DISPLAY_COVERED_LEFT_SIDE DISPLAY_NATIVE_COVERED_LEFT_SIDE
#define DISPLAY_COVERED_RIGHT_SIDE DISPLAY_NATIVE_COVERED_RIGHT_SIDE
#define DISPLAY_COVERED_BOTTOM_SIDE DISPLAY_NATIVE_COVERED_BOTTOM_SIDE

#define DISPLAY_DRAWABLE_WIDTH (DISPLAY_WIDTH-DISPLAY_COVERED_LEFT_SIDE-DISPLAY_COVERED_RIGHT_SIDE)
#define DISPLAY_DRAWABLE_HEIGHT (DISPLAY_HEIGHT-DISPLAY_COVERED_TOP_SIDE-DISPLAY_COVERED_BOTTOM_SIDE)

#ifndef DISPLAY_SPI_DRIVE_SETTINGS
#define DISPLAY_SPI_DRIVE_SETTINGS (1 | BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA)
#endif

#define SPI_BYTESPERPIXEL 2

void ClearScreen(void);

void TurnBacklightOn(void);
void TurnBacklightOff(void);

void DeinitSPIDisplay(void);

void InitST7735R(void);
