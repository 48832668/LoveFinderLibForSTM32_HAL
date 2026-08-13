/**
 * @file st7735.cpp
 * @brief ST7735 LCD Driver Implementation - C++17
 */
#include "stm32f4xx_hal.h"
#include "main.h"
#include "st7735.hpp"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint8_t DELAY = 0x80;
}

// 前向声明
static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
static void ST7735_WriteData_DMA(uint8_t *buff, size_t buff_size);

// based on Adafruit ST7735 library for Arduino
static const uint8_t
    init_cmds1[] = {           // Init for 7735R, part 1 (red or green tab)
        15,                    // 15 commands in list:
        ST7735_SWRESET, DELAY, //  1: Software reset, 0 args, w/delay
        150,                   //     150 ms delay
        ST7735_SLPOUT, DELAY,  //  2: Out of sleep mode, 0 args, w/delay
        255,                   //     500 ms delay
        ST7735_FRMCTR1, 3,     //  3: Frame rate ctrl - normal mode, 3 args:
        0x01, 0x2C, 0x2D,      //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
        ST7735_FRMCTR2, 3,     //  4: Frame rate control - idle mode, 3 args:
        0x01, 0x2C, 0x2D,      //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
        ST7735_FRMCTR3, 6,     //  5: Frame rate ctrl - partial mode, 6 args:
        0x01, 0x2C, 0x2D,      //     Dot inversion mode
        0x01, 0x2C, 0x2D,      //     Line inversion mode
        ST7735_INVCTR, 1,      //  6: Display inversion ctrl, 1 arg, no delay:
        0x07,                  //     No inversion
        ST7735_PWCTR1, 3,      //  7: Power control, 3 args, no delay:
        0xA2,
        0x02,             //     -4.6V
        0x84,             //     AUTO mode
        ST7735_PWCTR2, 1, //  8: Power control, 1 arg, no delay:
        0xC5,             //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
        ST7735_PWCTR3, 2, //  9: Power control, 2 args, no delay:
        0x0A,             //     Opamp current small
        0x00,             //     Boost frequency
        ST7735_PWCTR4, 2, // 10: Power control, 2 args, no delay:
        0x8A,             //     BCLK/2, Opamp current small & Medium low
        0x2A,
        ST7735_PWCTR5, 2, // 11: Power control, 2 args, no delay:
        0x8A, 0xEE,
        ST7735_VMCTR1, 1, // 12: Power control, 1 arg, no delay:
        0x0E,
        ST7735_INVOFF, 0, // 13: Don't invert display, no args, no delay
        ST7735_MADCTL, 1, // 14: Memory access control (directions), 1 arg:
        ST7735_ROTATION,  //     row addr/col addr, bottom to top refresh
        ST7735_COLMOD, 1, // 15: set color mode, 1 arg, no delay:
        0x05},            //     16-bit color

    init_cmds2[] = {      // Init for 7735S, part 2 (160x80 display)
        3,                //  3 commands in list:
        ST7735_CASET, 4,  //  1: Column addr set, 4 args, no delay:
        0x00, 0x00,       //     XSTART = 0
        0x00, 0x4F,       //     XEND = 79
        ST7735_RASET, 4,  //  2: Row addr set, 4 args, no delay:
        0x00, 0x00,       //     XSTART = 0
        0x00, 0x9F,       //     XEND = 159
        ST7735_INVOFF, 1}, //  3: Invert colors 此处我修改为INVOFF

    init_cmds3[] = {                                                                                                         // Init for 7735R, part 3 (red or green tab)
        4,                                                                                                                   //  4 commands in list:
        ST7735_GMCTRP1, 16,                                                                                                  //  1: Gamma Adjustments (pos. polarity), 16 args, no delay:
        0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, ST7735_GMCTRN1, 16,  //  2: Gamma Adjustments (neg. polarity), 16 args, no delay:
        0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, ST7735_NORON, DELAY, //  3: Normal display on, no args, w/delay
        10,                                                                                                                  //     10 ms delay
        ST7735_DISPON, DELAY,                                                                                                //  4: Main screen turn on, no args w/delay
        100};                                                                                                                //     100 ms delay

static void ST7735_Select()
{
  HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);
}

void ST7735_Unselect()
{
  HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_SET);
}

static void ST7735_Reset()
{
  HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_RESET);
  HAL_Delay(5);
  HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_SET);
}

static void ST7735_WriteCommand(uint8_t cmd)
{
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&ST7735_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7735_WriteData(uint8_t *buff, size_t buff_size)
{
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit(&ST7735_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
}

/*============================================================================
 * DMA传输函数 - 高性能刷屏
 *============================================================================*/

// DMA传输完成标志
static volatile uint8_t dma_transfer_complete = 1;

// DMA传输数据（阻塞式，等待完成）
static void ST7735_WriteData_DMA(uint8_t *buff, size_t buff_size)
{
  dma_transfer_complete = 0;
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, buff, buff_size);
  
  // 等待DMA传输完成
  while (!dma_transfer_complete);
}

// SPI DMA传输完成回调
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1)
  {
    dma_transfer_complete = 1;
  }
}

static void ST7735_ExecuteCommandList(const uint8_t *addr)
{
  uint8_t numCommands, numArgs;
  uint16_t ms;

  numCommands = *addr++;
  while (numCommands--)
  {
    uint8_t cmd = *addr++;
    ST7735_WriteCommand(cmd);

    numArgs = *addr++;
    // If high bit set, delay follows args
    ms = numArgs & DELAY;
    numArgs &= ~DELAY;
    if (numArgs)
    {
      ST7735_WriteData((uint8_t *)addr, numArgs);
      addr += numArgs;
    }

    if (ms)
    {
      ms = *addr++;
      if (ms == 255)
        ms = 500;
      HAL_Delay(ms);
    }
  }
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
  // column address set
  ST7735_WriteCommand(ST7735_CASET);
  uint8_t data[] = {0x00, static_cast<uint8_t>(x0 + ST7735_XSTART), 0x00, static_cast<uint8_t>(x1 + ST7735_XSTART)};
  ST7735_WriteData(data, sizeof(data));

  // row address set
  ST7735_WriteCommand(ST7735_RASET);
  data[1] = static_cast<uint8_t>(y0 + ST7735_YSTART);
  data[3] = static_cast<uint8_t>(y1 + ST7735_YSTART);
  ST7735_WriteData(data, sizeof(data));

  // write to RAM
  ST7735_WriteCommand(ST7735_RAMWR);
}

void ST7735_Init()
{
  ST7735_Select();
  ST7735_Reset();
  ST7735_ExecuteCommandList(init_cmds1);
  ST7735_ExecuteCommandList(init_cmds2);
  ST7735_ExecuteCommandList(init_cmds3);
  ST7735_Unselect();
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;

  ST7735_Select();

  ST7735_SetAddressWindow(x, y, x + 1, y + 1);
  uint8_t data[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  ST7735_WriteData(data, sizeof(data));

  ST7735_Unselect();
}

/*============================================================================
 * 字符渲染 — 双模式字形解析
 *
 * 模式 A (传统): font.data != nullptr -> 整套字库, 固定索引 (ch-32)*height
 * 模式 B (字符级编译): font.data == nullptr -> font_get_glyph 查表,
 *                      未编译的字符返回 nullptr, 自动跳过(保持其他字符不受影响)
 *==========================================================================*/

static const uint16_t* ST7735_ResolveGlyph(const FontDef& font, uint32_t ch, bool* isWide)
{
  // 中文子集字体: ch 是 Unicode 码点(>=128), 走宽字符通道
  if (ch > 127) {
    *isWide = true;
    return font_get_glyph_unicode(font, static_cast<uint16_t>(ch));
  }

  // ASCII 通道
  *isWide = false;
  if (ch < 32 || ch > 126) return nullptr;

  if (font.data != nullptr) {
    // 模式 A: 整套字库
    return &font.data[(ch - 32) * font.height];
  }
  // 模式 B: 字符级编译
  return font_get_glyph(font, static_cast<uint8_t>(ch));
}

static void ST7735_WriteChar(uint16_t x, uint16_t y, uint32_t ch, const FontDef& font, uint16_t color, uint16_t bgcolor)
{
  bool isWide = false;
  const uint16_t* glyph = ST7735_ResolveGlyph(font, ch, &isWide);
  if (!glyph) return;   // 未编译字符 -> 跳过 (模式 B 的核心行为)

  uint8_t charW = (isWide && font.width == 16) ? 16 : font.width;
  uint32_t i, j;

  ST7735_SetAddressWindow(x, y, x + charW - 1, y + font.height - 1);

  for (i = 0; i < font.height; i++)
  {
    uint16_t b = glyph[i];
    for (j = 0; j < charW; j++)
    {
      uint8_t data[2];
      if ((b << j) & 0x8000)
      {
        data[0] = static_cast<uint8_t>(color >> 8);
        data[1] = static_cast<uint8_t>(color & 0xFF);
      }
      else
      {
        data[0] = static_cast<uint8_t>(bgcolor >> 8);
        data[1] = static_cast<uint8_t>(bgcolor & 0xFF);
      }
      ST7735_WriteData(data, sizeof(data));
    }
  }
}

/*
Simpler (and probably slower) implementation:

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color) {
    uint32_t i, b, j;

    for(i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for(j = 0; j < font.width; j++) {
            if((b << j) & 0x8000)  {
                ST7735_DrawPixel(x + j, y + i, color);
            }
        }
    }
}
*/

/*============================================================================
 * UTF-8 解码 — 支持中文字符串
 *==========================================================================*/
static uint32_t ST7735_UTF8_Decode(const char* str, uint32_t* consumed)
{
  uint8_t c0 = static_cast<uint8_t>(str[0]);
  if (c0 < 0x80) { *consumed = 1; return c0; }                 // 1字节: ASCII
  if ((c0 & 0xE0) == 0xC0) {                                   // 2字节
    *consumed = 2;
    return static_cast<uint32_t>((c0 & 0x1F) << 6) |
           (static_cast<uint8_t>(str[1]) & 0x3F);
  }
  if ((c0 & 0xF0) == 0xE0) {                                   // 3字节: 汉字
    *consumed = 3;
    return static_cast<uint32_t>((c0 & 0x0F) << 12) |
           ((static_cast<uint8_t>(str[1]) & 0x3F) << 6) |
           (static_cast<uint8_t>(str[2]) & 0x3F);
  }
  *consumed = 1;                                               // 无效序列
  return c0;
}

void ST7735_WriteString(uint16_t x, uint16_t y, const char *str, const FontDef& font, uint16_t color, uint16_t bgcolor)
{
  ST7735_Select();

  while (*str)
  {
    uint32_t consumed = 0;
    uint32_t ch = ST7735_UTF8_Decode(str, &consumed);
    bool isWide = (ch > 127);

    uint8_t charW = (isWide && font.width == 16) ? 16 : font.width;

    if (x + charW >= ST7735_WIDTH)
    {
      x = 0;
      y += font.height;
      if (y + font.height >= ST7735_HEIGHT)
      {
        break;
      }

      if (*str == ' ')
      {
        // skip spaces in the beginning of the new line
        str += consumed;
        continue;
      }
    }

    ST7735_WriteChar(x, y, ch, font, color, bgcolor);
    x += charW;
    str += consumed;
  }

  ST7735_Unselect();
}

/*============================================================================
 * UTF-8 显式入口 — 与 ST7735_WriteString 行为一致, 语义更清晰
 *==========================================================================*/
void ST7735_WriteStringUTF8(uint16_t x, uint16_t y, const char *str, const FontDef& font, uint16_t color, uint16_t bgcolor)
{
  ST7735_WriteString(x, y, str, font, color, bgcolor);
}

void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  uint8_t data[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  for (y = h; y > 0; y--)
  {
    for (x = w; x > 0; x--)
    {
      HAL_SPI_Transmit(&ST7735_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
    }
  }

  ST7735_Unselect();
}

void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  // Prepare whole line in a single buffer
  uint8_t pixel[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  uint8_t *line = static_cast<uint8_t*>(malloc(w * sizeof(pixel)));
  for (x = 0; x < w; ++x)
    memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  for (y = h; y > 0; y--)
    HAL_SPI_Transmit(&ST7735_SPI_PORT, line, w * sizeof(pixel), HAL_MAX_DELAY);

  free(line);
  ST7735_Unselect();
}

// DMA版本 - 最高性能
void ST7735_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  // clipping
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    w = ST7735_WIDTH - x;
  if ((y + h - 1) >= ST7735_HEIGHT)
    h = ST7735_HEIGHT - y;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  // 准备整行数据缓冲区
  uint8_t pixel[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color & 0xFF)};
  uint8_t *line = static_cast<uint8_t*>(malloc(w * sizeof(pixel)));
  for (x = 0; x < w; ++x)
    memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

  HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
  
  // 使用DMA传输每一行
  for (y = h; y > 0; y--)
  {
    ST7735_WriteData_DMA(line, w * sizeof(pixel));
  }

  free(line);
  ST7735_Unselect();
}

// DMA整屏填充 - 最快速度
void ST7735_FillScreen_DMA(uint16_t color)
{
  ST7735_FillRectangle_DMA(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

// DMA图像绘制 - 高速图像传输
void ST7735_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    return;
  if ((y + h - 1) >= ST7735_HEIGHT)
    return;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
  
  // 使用DMA传输整个图像
  ST7735_WriteData_DMA((uint8_t *)data, sizeof(uint16_t) * w * h);
  
  ST7735_Unselect();
}

void ST7735_FillScreen(uint16_t color)
{
  ST7735_FillRectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_FillScreenFast(uint16_t color)
{
  ST7735_FillRectangleFast(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    return;
  if ((x + w - 1) >= ST7735_WIDTH)
    return;
  if ((y + h - 1) >= ST7735_HEIGHT)
    return;

  ST7735_Select();
  ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
  ST7735_WriteData((uint8_t *)data, sizeof(uint16_t) * w * h);
  ST7735_Unselect();
}

void ST7735_InvertColors(bool invert)
{
  ST7735_Select();
  ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
  ST7735_Unselect();
}

void ST7735_SetGamma(uint8_t gamma)
{
  ST7735_Select();
  ST7735_WriteCommand(ST7735_GAMSET);
  ST7735_WriteData(&gamma, sizeof(gamma));
  ST7735_Unselect();
}

void ST7735_Print(uint16_t x, uint16_t y, const FontDef& font, uint16_t color, uint16_t bgcolor, const char *format, ...)
{
  char temp[256];
  va_list ap;
  va_start(ap, format);
  vsprintf(temp, format, ap);
  va_end(ap);
  ST7735_WriteString(x, y, temp, font, color, bgcolor);
}

/*============================================================================
 * 图标绘制函数
 *============================================================================*/

void ST7735_DrawIcon(uint16_t x, uint16_t y, IconIndex icon)
{
  if (icon >= ICON_COUNT) return;
  if ((x + ICON_WIDTH > ST7735_WIDTH) || (y + ICON_HEIGHT > ST7735_HEIGHT)) return;
  
  ST7735_DrawImage(x, y, ICON_WIDTH, ICON_HEIGHT, Icons[icon].data);
}

/*============================================================================
 * 辅助图形绘制函数
 *============================================================================*/

void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
  if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
  
  ST7735_FillRectangle(x, y, w, 1, color);
}

void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
  if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
  if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
  
  ST7735_FillRectangle(x, y, 1, h, color);
}

void ST7735_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  ST7735_DrawHLine(x, y, w, color);
  ST7735_DrawHLine(x, y + h - 1, w, color);
  ST7735_DrawVLine(x, y, h, color);
  ST7735_DrawVLine(x + w - 1, y, h, color);
}

void ST7735_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color)
{
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  
  // Draw straight lines
  ST7735_DrawHLine(x + r, y, w - 2 * r, color);
  ST7735_DrawHLine(x + r, y + h - 1, w - 2 * r, color);
  ST7735_DrawVLine(x, y + r, h - 2 * r, color);
  ST7735_DrawVLine(x + w - 1, y + r, h - 2 * r, color);
  
  // Draw corners (simple approximation)
  int16_t i;
  for (i = 0; i <= r; i++)
  {
    int16_t dx = (int16_t)(r * 0.7f);
    if (i < r)
    {
      // Top-left corner
      ST7735_DrawPixel(x + r - i, y + r - dx, color);
      ST7735_DrawPixel(x + r - dx, y + r - i, color);
      // Top-right corner
      ST7735_DrawPixel(x + w - 1 - r + i, y + r - dx, color);
      ST7735_DrawPixel(x + w - 1 - r + dx, y + r - i, color);
      // Bottom-left corner
      ST7735_DrawPixel(x + r - i, y + h - 1 - r + dx, color);
      ST7735_DrawPixel(x + r - dx, y + h - 1 - r + i, color);
      // Bottom-right corner
      ST7735_DrawPixel(x + w - 1 - r + i, y + h - 1 - r + dx, color);
      ST7735_DrawPixel(x + w - 1 - r + dx, y + h - 1 - r + i, color);
    }
  }
}

void ST7735_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color)
{
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  
  // Fill main rectangle
  ST7735_FillRectangle(x + r, y, w - 2 * r, h, color);
  ST7735_FillRectangle(x, y + r, r, h - 2 * r, color);
  ST7735_FillRectangle(x + w - r, y + r, r, h - 2 * r, color);
  
  // Fill corners
  int16_t i, j;
  for (i = 0; i < r; i++)
  {
    for (j = 0; j < r; j++)
    {
      if ((i - r) * (i - r) + (j - r) * (j - r) <= r * r)
      {
        // Top-left
        ST7735_DrawPixel(x + i, y + j, color);
        // Top-right
        ST7735_DrawPixel(x + w - 1 - i, y + j, color);
        // Bottom-left
        ST7735_DrawPixel(x + i, y + h - 1 - j, color);
        // Bottom-right
        ST7735_DrawPixel(x + w - 1 - i, y + h - 1 - j, color);
      }
    }
  }
}

/*============================================================================
 * 美化显示函数 - 带图标的数值显示
 *============================================================================*/

void ST7735_DrawValueWithIcon(uint16_t x, uint16_t y, IconIndex icon, 
                               const char *value, const char *unit,
                               uint16_t valueColor, uint16_t unitColor)
{
  // Draw icon
  ST7735_DrawIcon(x, y, icon);
  
  // Draw value
  ST7735_WriteString(x + ICON_WIDTH + 2, y + 1, value, Font_7x10, valueColor, ST7735_BLACK);
  
  // Draw unit (smaller, after value)
  uint8_t valueLen = 0;
  while (value[valueLen]) valueLen++;
  ST7735_WriteString(x + ICON_WIDTH + 2 + valueLen * 7, y + 1, unit, Font_7x10, unitColor, ST7735_BLACK);
}

/*============================================================================
 * 扩展图形原语 — 空心 (Hollow) + 实心 (Filled)
 *==========================================================================*/

// 内部: 带裁剪的实心水平线 (坐标可为负, 自动裁剪到屏幕内)
static void ST7735_FillHLineClipped(int16_t x0, int16_t x1, int16_t y, uint16_t color)
{
  if (y < 0 || y >= (int16_t)ST7735_HEIGHT) return;
  if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
  if (x1 < 0 || x0 >= (int16_t)ST7735_WIDTH) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= (int16_t)ST7735_WIDTH) x1 = ST7735_WIDTH - 1;
  ST7735_FillRectangle((uint16_t)x0, (uint16_t)y, (uint16_t)(x1 - x0 + 1), 1, color);
}

// Bresenham 直线 (空心)
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = dx - dy;

  for (;;)
  {
    ST7735_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx)  { err += dx; y0 += sy; }
  }
}

// 中点圆算法 (空心)
void ST7735_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  if (r <= 0) return;
  int16_t x = 0;
  int16_t y = r;
  int16_t d = 3 - 2 * r;

  while (x <= y)
  {
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), color);
    ST7735_DrawPixel((uint16_t)(x0 + y), (uint16_t)(y0 + x), color);
    ST7735_DrawPixel((uint16_t)(x0 - y), (uint16_t)(y0 + x), color);
    ST7735_DrawPixel((uint16_t)(x0 + y), (uint16_t)(y0 - x), color);
    ST7735_DrawPixel((uint16_t)(x0 - y), (uint16_t)(y0 - x), color);

    if (d < 0)
      d += 4 * x + 6;
    else
    {
      d += 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

// 实心圆 (水平线填充)
void ST7735_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  if (r <= 0) return;
  int16_t x = 0;
  int16_t y = r;
  int16_t d = 3 - 2 * r;

  while (x <= y)
  {
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 + y, color);
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 - y, color);
    ST7735_FillHLineClipped(x0 - y, x0 + y, y0 + x, color);
    ST7735_FillHLineClipped(x0 - y, x0 + y, y0 - x, color);

    if (d < 0)
      d += 4 * x + 6;
    else
    {
      d += 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

// 中点椭圆算法 (空心)
void ST7735_DrawEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color)
{
  if (rx <= 0 || ry <= 0) return;
  int16_t x = 0;
  int16_t y = ry;
  int32_t rx2 = (int32_t)rx * rx;
  int32_t ry2 = (int32_t)ry * ry;
  int32_t twoRx2 = 2 * rx2;
  int32_t twoRy2 = 2 * ry2;
  int32_t px = 0;
  int32_t py = twoRx2 * y;
  int32_t d = ry2 - rx2 * y + (rx2 >> 2);

  // Region 1
  while (px < py)
  {
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), color);
    x++;
    px += twoRy2;
    if (d < 0)
      d += ry2 + px;
    else
    {
      y--;
      py -= twoRx2;
      d += ry2 + px - py;
    }
  }

  // Region 2
  d = ry2 * (x + 1) * (x + 1) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
  while (y >= 0)
  {
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), color);
    ST7735_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), color);
    ST7735_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), color);
    y--;
    py -= twoRx2;
    if (d > 0)
      d += rx2 - py;
    else
    {
      x++;
      px += twoRy2;
      d += rx2 - py + px;
    }
  }
}

// 实心椭圆 (水平线填充)
void ST7735_FillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color)
{
  if (rx <= 0 || ry <= 0) return;
  int16_t x = 0;
  int16_t y = ry;
  int32_t rx2 = (int32_t)rx * rx;
  int32_t ry2 = (int32_t)ry * ry;
  int32_t twoRx2 = 2 * rx2;
  int32_t twoRy2 = 2 * ry2;
  int32_t px = 0;
  int32_t py = twoRx2 * y;
  int32_t d = ry2 - rx2 * y + (rx2 >> 2);

  while (px < py)
  {
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 + y, color);
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 - y, color);
    x++;
    px += twoRy2;
    if (d < 0)
      d += ry2 + px;
    else
    {
      y--;
      py -= twoRx2;
      d += ry2 + px - py;
    }
  }

  d = ry2 * (x + 1) * (x + 1) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
  while (y >= 0)
  {
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 + y, color);
    ST7735_FillHLineClipped(x0 - x, x0 + x, y0 - y, color);
    y--;
    py -= twoRx2;
    if (d > 0)
      d += rx2 - py;
    else
    {
      x++;
      px += twoRy2;
      d += rx2 - py + px;
    }
  }
}

// 三角形 (空心) — 三条边
void ST7735_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
  ST7735_DrawLine(x0, y0, x1, y1, color);
  ST7735_DrawLine(x1, y1, x2, y2, color);
  ST7735_DrawLine(x2, y2, x0, y0, color);
}

// 三角形 (实心) — 扫描线 (Adafruit GFX 验证算法), 支持任意方向 (含平底/平顶/全水平)
void ST7735_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
  int16_t a, b, y, last;

  // 按 y 排序顶点: 保证 y0 <= y1 <= y2
  if (y0 > y1) { int16_t t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
  if (y1 > y2) { int16_t t; t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }
  if (y0 > y1) { int16_t t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }

  // 三个顶点在同一水平线上: 直接画一条水平线
  if (y0 == y2)
  {
    a = b = x0;
    if (x1 < a) a = x1; else if (x1 > b) b = x1;
    if (x2 < a) a = x2; else if (x2 > b) b = x2;
    ST7735_FillHLineClipped(a, b, y0, color);
    return;
  }

  int32_t dx02 = (int32_t)x2 - x0, dy02 = (int32_t)y2 - y0;
  int32_t dx01 = (int32_t)x1 - x0, dy01 = (int32_t)y1 - y0;
  int32_t dx12 = (int32_t)x2 - x1, dy12 = (int32_t)y2 - y1;
  int32_t sa = 0, sb = 0;

  // 上半部分: 扫描线交点位于边 0-1 和 0-2 上
  // 若 y1==y2 (平底), last=y1 包含该行; 否则 last=y1-1 跳过 (避免下部分 /0)
  last = (y1 == y2) ? y1 : (y1 - 1);
  for (y = y0; y <= last; y++)
  {
    a = (int16_t)(x0 + sa / dy01);
    b = (int16_t)(x0 + sb / dy02);
    ST7735_FillHLineClipped(a, b, y, color);
    sa += dx01;
    sb += dx02;
  }

  // 下半部分: 扫描线交点位于边 0-2 和 1-2 上
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for (; y <= y2; y++)
  {
    a = (int16_t)(x1 + sa / dy12);
    b = (int16_t)(x0 + sb / dy02);
    ST7735_FillHLineClipped(a, b, y, color);
    sa += dx12;
    sb += dx02;
  }
}

// 多边形 (空心)
void ST7735_DrawPolygon(const int16_t* xs, const int16_t* ys, uint16_t n, uint16_t color)
{
  if (n < 3) return;
  for (uint16_t i = 0; i < n; i++)
  {
    uint16_t j = (i + 1) % n;
    ST7735_DrawLine(xs[i], ys[i], xs[j], ys[j], color);
  }
}

// 多边形 (实心) — 通用扫描线填充 (奇偶规则)
void ST7735_FillPolygon(const int16_t* xs, const int16_t* ys, uint16_t n, uint16_t color)
{
  if (n < 3) return;

  // 求包围盒
  int16_t minY = ys[0], maxY = ys[0];
  for (uint16_t i = 1; i < n; i++)
  {
    if (ys[i] < minY) minY = ys[i];
    if (ys[i] > maxY) maxY = ys[i];
  }
  if (maxY < 0 || minY >= (int16_t)ST7735_HEIGHT) return;

  // 每行扫描: 计算所有交点, 两两配对填充
  for (int16_t y = minY; y <= maxY; y++)
  {
    int16_t xsect[16];   // 最多 8 个顶点的多边形, 每行最多 8 个交点
    uint16_t cnt = 0;

    for (uint16_t i = 0; i < n && cnt < 16; i++)
    {
      uint16_t j = (i + 1) % n;
      int16_t yi = ys[i], yj = ys[j];
      int16_t xi = xs[i], xj = xs[j];

      // 边与扫描线的交点 (半开区间避免顶点重复计数)
      bool cond1 = (yi <= y) && (yj > y);
      bool cond2 = (yj <= y) && (yi > y);
      if (cond1 || cond2)
      {
        int32_t x = (int32_t)xi + ((int32_t)(y - yi) * (xj - xi)) / (yj - yi);
        xsect[cnt++] = (int16_t)x;
      }
    }

    // 排序交点
    for (uint16_t a = 0; a < cnt; a++)
      for (uint16_t b = a + 1; b < cnt; b++)
        if (xsect[b] < xsect[a])
        {
          int16_t t = xsect[a]; xsect[a] = xsect[b]; xsect[b] = t;
        }

    // 成对填充
    for (uint16_t k = 0; k + 1 < cnt; k += 2)
      ST7735_FillHLineClipped(xsect[k], xsect[k + 1], y, color);
  }
}
