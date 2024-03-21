#pragma once

#include "config.h"

#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

#define DISPLAY_SPI_DRIVE_SETTINGS (1 | BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA)

#define SPI_BYTESPERPIXEL 2

void TurnBacklightOn(void);
void TurnBacklightOff(void);

void DeinitSPIDisplay(void);

void InitSPIDisplay(void);
