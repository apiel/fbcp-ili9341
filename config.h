#pragma once

#define ST7789 1
#define ST7789VW 1
#define GPIO_TFT_DATA_CONTROL 6
#define GPIO_TFT_RESET_PIN 5
#define GPIO_TFT_BACKLIGHT 13
#define SPI_BUS_CLOCK_DIVISOR 20
// #define SPI_BUS_CLOCK_DIVISOR 30
#define USE_DMA_TRANSFERS 0
#define DISPLAY_ROTATE_180_DEGREES 1

// If defined, rotates the display 180 degrees. This might not rotate the panel scan order though,
// so adding this can cause up to one vsync worth of extra display latency. It is best to avoid this and
// install the display in its natural rotation order, if possible.
// #define DISPLAY_ROTATE_180_DEGREES

// If defined, displays in landscape. Undefine to display in portrait.
#define DISPLAY_OUTPUT_LANDSCAPE

// If defined, the source video frame is scaled to fit the SPI display by stretching to fit, ignoring
// aspect ratio. Enabling this will cause e.g. 16:9 1080p source to be stretched to fully cover
// a 4:3 320x240 display. If disabled, scaling is performed preserving aspect ratio, so letterboxes or
// pillarboxes will be introduced if needed to retain proper width and height proportions.
// #define DISPLAY_BREAK_ASPECT_RATIO_WHEN_SCALING

// If defined, reverses RGB<->BGR color subpixel order. This is something that seems to be display panel
// specific, rather than display controller specific, and displays manufactured with the same controller
// can have different subpixel order (without the controller taking it automatically into account).
// If display colors come out reversed in blue vs red channels, define this to swap the two.
// #define DISPLAY_SWAP_BGR

// If defined, inverts display pixel colors (31=black, 0=white). Default is to have (0=black, 31=white)
// Pass this if the colors look like a photo negative of the actual image.
// #define DISPLAY_INVERT_COLORS

// If defined, flipping the display between portrait<->landscape is done in software, rather than
// asking the display controller to adjust its RAM write direction.
// Doing the flip in software reduces tearing, since neither the ILI9341 nor ILI9486 displays (and
// probably no other displays in existence?) allow one to adjust the direction that the scanline refresh
// cycle runs in, but the scanline refresh always runs in portrait mode in these displays. Not having
// this defined reduces CPU usage at the expense of more tearing, although it is debatable which
// effect is better - this can be subjective. Impact is around 0.5-1.0msec of extra CPU time.
// DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE disabled: diagonal tearing
// DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE enabled: traditional no-vsync tearing (tear line runs in portrait
// i.e. narrow direction)
#if !defined(SINGLE_CORE_BOARD)
#define DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE
#endif
