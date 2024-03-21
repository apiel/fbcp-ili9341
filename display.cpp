#include "config.h"
#include "display.h"
#include "spi.h"

#include <memory.h>
#include <stdio.h>
#include <unistd.h>

void sendAddr(uint8_t cmd, uint16_t addr0, uint16_t addr1)
{
  uint8_t addr[4] = {(uint8_t)(addr0 >> 8), (uint8_t)(addr0 & 0xFF), (uint8_t)(addr1 >> 8), (uint8_t)(addr1 & 0xFF)};
  sendCmd(cmd, addr, 4);
}

void drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  sendAddr(DISPLAY_SET_CURSOR_X, (uint16_t)x, (uint16_t)x);
  sendAddr(DISPLAY_SET_CURSOR_Y, (uint16_t)y, (uint16_t)y);

  uint8_t data[SPI_BYTESPERPIXEL] = {color >> 8, color & 0xFF};
  sendCmd(DISPLAY_WRITE_PIXELS, data, 2);
}

void drawFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // sendAddr(DISPLAY_SET_CURSOR_X, x, x + w);
  // sendAddr(DISPLAY_SET_CURSOR_Y, y, y + h);

  // uint16_t size = w * h * SPI_BYTESPERPIXEL;
  // uint8_t pixels[size];
  // uint8_t pixel[SPI_BYTESPERPIXEL] = {color >> 8, color & 0xFF};
  // for (uint16_t i = 0; i < size; i += SPI_BYTESPERPIXEL)
  // {
  //   pixels[i] = pixel[0];
  //   pixels[i + 1] = pixel[1];
  // }
  // sendCmd(DISPLAY_WRITE_PIXELS, pixels, size);

  for (uint16_t i = 0; i < w; ++i)
  {
    for (uint16_t j = 0; j < h; ++j)
    {
      drawPixel(x + i, y + j, color);
    }
  }
}

void ClearScreen()
{
  // sendAddr(DISPLAY_SET_CURSOR_X, 0, DISPLAY_WIDTH - 1);
  // sendAddr(DISPLAY_SET_CURSOR_Y, 0, DISPLAY_HEIGHT - 1);

  // uint16_t size = DISPLAY_WIDTH * DISPLAY_HEIGHT * SPI_BYTESPERPIXEL;
  // uint8_t pixels[size];
  // memset(pixels, (uint8_t)0, size);
  // sendCmd(DISPLAY_WRITE_PIXELS, pixels, size);

  // for (int y = 0; y < DISPLAY_HEIGHT; ++y)
  // {
  //   SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, (DISPLAY_WIDTH - 1) >> 8, (DISPLAY_WIDTH - 1) & 0xFF);
  //   SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, (uint8_t)(y >> 8), (uint8_t)(y & 0xFF), (DISPLAY_HEIGHT - 1) >> 8, (DISPLAY_HEIGHT - 1) & 0xFF);

  //   SPITask *clearLine = AllocTask(DISPLAY_WIDTH * SPI_BYTESPERPIXEL);
  //   clearLine->cmd = DISPLAY_WRITE_PIXELS;
  //   memset(clearLine->data, 0, clearLine->size);
  //   CommitTask(clearLine);
  //   RunSPITask(clearLine);
  //   DoneTask(clearLine);
  // }

  SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, (DISPLAY_WIDTH - 1) >> 8, (DISPLAY_WIDTH - 1) & 0xFF);
  SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, 0, 0, (DISPLAY_HEIGHT - 1) >> 8, (DISPLAY_HEIGHT - 1) & 0xFF);

  SPITask *clearLine = AllocTask(DISPLAY_WIDTH * DISPLAY_HEIGHT * SPI_BYTESPERPIXEL);
  clearLine->cmd = DISPLAY_WRITE_PIXELS;
  memset(clearLine->data, 0, clearLine->size);
  CommitTask(clearLine);
  RunSPITask(clearLine);
  DoneTask(clearLine);

  SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, (DISPLAY_WIDTH - 1) >> 8, (DISPLAY_WIDTH - 1) & 0xFF);
  SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, 0, 0, (DISPLAY_HEIGHT - 1) >> 8, (DISPLAY_HEIGHT - 1) & 0xFF);
}

void drawStuff()
{
  for (int y = 0; y < DISPLAY_HEIGHT; ++y)
  {
    int x = DISPLAY_HEIGHT - y - 1;
    drawPixel(x, y, 0xFF00FF);
  }

  drawFillRect(20, 40, 10, 10, 0xFF00FF);
}

void InitST7735R()
{
  printf("Resetting display at reset GPIO pin %d\n", GPIO_TFT_RESET_PIN);
  SET_GPIO_MODE(GPIO_TFT_RESET_PIN, 1);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  CLEAR_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);

  // Do the initialization with a very low SPI bus speed, so that it will succeed even if the bus speed chosen by the user is too high.
  spi->clk = 34;
  __sync_synchronize();

  BEGIN_SPI_COMMUNICATION();
  {
    // usleep(120*1000);
    SPI_TRANSFER(0x11 /*Sleep Out*/);
    usleep(120 * 1000);
    SPI_TRANSFER(0x3A /*COLMOD: Pixel Format Set*/, 0x05 /*16bpp*/);
    usleep(20 * 1000);

#define MADCTL_BGR_PIXEL_ORDER (1 << 3)
#define MADCTL_ROW_COLUMN_EXCHANGE (1 << 5)
#define MADCTL_COLUMN_ADDRESS_ORDER_SWAP (1 << 6)
#define MADCTL_ROW_ADDRESS_ORDER_SWAP (1 << 7)
#define MADCTL_ROTATE_180_DEGREES (MADCTL_COLUMN_ADDRESS_ORDER_SWAP | MADCTL_ROW_ADDRESS_ORDER_SWAP)

    uint8_t madctl = 0;
    madctl |= MADCTL_ROW_ADDRESS_ORDER_SWAP;
    madctl ^= MADCTL_ROTATE_180_DEGREES;

    SPI_TRANSFER(0x36 /*MADCTL: Memory Access Control*/, madctl);
    usleep(10 * 1000);

    SPI_TRANSFER(0xBA /*DGMEN: Enable Gamma*/, 0x04);
    bool invertColors = true;

    if (invertColors)
      SPI_TRANSFER(0x21 /*Display Inversion On*/);
    else
      SPI_TRANSFER(0x20 /*Display Inversion Off*/);

    SPI_TRANSFER(0x13 /*NORON: Partial off (normal)*/);
    usleep(10 * 1000);

    // The ST7789 controller is actually a unit with 320x240 graphics memory area, but only 240x240 portion
    // of it is displayed. Therefore if we wanted to swap row address mode above, writes to Y=0...239 range will actually land in
    // memory in row addresses Y = 319-(0...239) = 319...80 range. To view this range, we must scroll the view by +80 units in Y
    // direction so that contents of Y=80...319 is displayed instead of Y=0...239.
    if ((madctl & MADCTL_ROW_ADDRESS_ORDER_SWAP))
      SPI_TRANSFER(0x37 /*VSCSAD: Vertical Scroll Start Address of RAM*/, 0, 320 - DISPLAY_WIDTH);

    SPI_TRANSFER(/*Display ON*/ 0x29);
    usleep(100 * 1000);

    ClearScreen();
  }

  // And speed up to the desired operation speed finally after init is done.
  usleep(10 * 1000); // Delay a bit before restoring CLK, or otherwise this has been observed to cause the display not init if done back to back after the clear operation above.
  spi->clk = SPI_BUS_CLOCK_DIVISOR;

  printf("draw stuff\n");
  drawStuff();
}

void DeinitSPIDisplay()
{
  // ClearScreen();
}
