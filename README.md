# ST7735 LCD Display Library for STM32 (最终版)

高性能、可移植的 ST7735 液晶屏驱动库（C++17 / HAL 库），支持**字符级编译**、**空心/实心图形**、SPI 轮询 + DMA 双通道传输。

## 特性

- **字符级编译**: 只编译需要的字符（如中文字库仅编译"你好"两个字模做上电欢迎语），显著减小 Flash 占用，适合小容量 MCU
- **双模式字库**:
  - 模式 A (传统): `font.data != nullptr` — 整套字库, 固定索引 `(ch-32)*height`
  - 模式 B (字符级): `font.data == nullptr` — 查表 `font_get_glyph`/`font_get_glyph_unicode`, 未编译字符自动跳过
- **完整图形 API**: 空心+实心 — 点/线/圆/椭圆/矩形/圆角矩形/三角形/多边形
- **SPI 传输双通道**: 默认阻塞轮询 (`HAL_SPI_Transmit`), 高性能路径提供 `_DMA` 后缀函数 (`HAL_SPI_Transmit_DMA` + 完成回调)
- **UTF-8 中文支持**: `ST7735_WriteStringUTF8` 可直接输出中文字符串
- **轻量**: 无动态内存分配, 最小化 RAM 占用

## 目录结构

```
LoveFinderLibForSTM32_HAL/
├── ST7735/                    # ST7735 屏幕驱动核心 (9 文件)
│   ├── st7735.hpp            # 驱动头文件 (常量/宏/API 声明)
│   ├── st7735.cpp            # 驱动实现 (初始化/字符/图形/SPI)
│   ├── fonts_config.hpp      # 字体配置与字符集定义
│   ├── fonts.h / fonts.c     # 完整字库: Font_7x10 / Font_11x18 / Font_16x26
│   ├── fonts_subset.h/cpp    # 字符级编译子集字库 (ASCII 子集 + 中文子集)
│   └── icons.h / icons.c     # 图标库 (Icons_*, IconIndex)
├── BUTTON/ ENCODER/ INA226/ W25QFLASH/   # 其他外设库 (同风格)
├── README.md                 # 本文档
├── PROJECT_SUMMARY.md        # 项目交付总结
└── LICENSE                   # MIT 许可证
```

## 硬件绑定 (编译期宏, 非运行时参数)

库通过 `st7735.hpp` 中的宏直接绑定 HAL 句柄与引脚, **无需运行时配置函数**:

```cpp
// SPI 句柄 (CubeMX 生成的全局变量名)
#define ST7735_SPI_PORT      hspi1

// 引脚 (CubeMX 在 main.h 生成的宏)
#define ST7735_RES_Pin       LCD_RESET_Pin      // PB9
#define ST7735_RES_GPIO_Port LCD_RESET_GPIO_Port
#define ST7735_CS_Pin        LCD_CS_Pin         // PB4
#define ST7735_CS_GPIO_Port  LCD_CS_GPIO_Port
#define ST7735_DC_Pin        LCD_DC_Pin         // PB8
#define ST7735_DC_GPIO_Port  LCD_DC_GPIO_Port
```

> 换平台/换引脚只需改这几个宏 + 保证 CubeMX 生成的宏名一致。屏幕电源使能 (EN) 由工程初始化阶段单独控制, 不属库职责。

## 快速开始

### 1. 初始化

```cpp
#include "st7735.hpp"

// 必须在 CubeMX SPI + GPIO 初始化完成后调用 (含 EN 引脚拉高)
ST7735_Init();                 // CS 拉低 -> RES 复位 -> 初始化命令序列 -> CS 拉高
ST7735_FillScreen(ST7735_BLACK);
```

### 2. 字符与字符串

```cpp
// ASCII (完整字库)
ST7735_WriteString(10, 10, "HELLO", Font_7x10, ST7735_WHITE, ST7735_BLACK);

// UTF-8 中文 (字符级子集, 只编译"你好")
ST7735_WriteStringUTF8(62, 28, "\xE4\xBD\xA0\xE5\xA5\xBD",
                       Font_Subset_ZH_16x16, ST7735_YELLOW, ST7735_BLACK);

// 格式化输出
ST7735_Print(10, 30, Font_7x10, ST7735_GREEN, ST7735_BLACK, "Volt: %.2fV", 3.3f);
```

### 3. 图形 (空心 + 实心)

```cpp
ST7735_DrawLine(0, 0, 80, 40, ST7735_RED);            // 直线
ST7735_DrawCircle(40, 40, 20, ST7735_YELLOW);         // 空心圆
ST7735_FillCircle(100, 40, 20, ST7735_GREEN);         // 实心圆
ST7735_DrawEllipse(60, 20, 25, 12, ST7735_CYAN);      // 空心椭圆
ST7735_FillEllipse(120, 20, 25, 12, ST7735_MAGENTA);  // 实心椭圆
ST7735_DrawRect(10, 60, 30, 15, ST7735_ORANGE);       // 空心矩形
ST7735_FillRectangle(50, 60, 30, 15, ST7735_BLUE);    // 实心矩形
ST7735_DrawRoundRect(90, 60, 30, 15, 5, ST7735_RED);  // 空心圆角矩形
ST7735_FillRoundRect(130, 60, 24, 15, 5, ST7735_GREEN);// 实心圆角矩形
ST7735_DrawTriangle(10, 20, 20, 40, 0, 40, ST7735_CYAN);       // 空心三角形
ST7735_FillTriangle(30, 20, 40, 40, 20, 40, ST7735_YELLOW);    // 实心三角形

int16_t xs[5] = {70, 85, 95, 80, 65};
int16_t ys[5] = {25, 25, 40, 50, 40};
ST7735_DrawPolygon(xs, ys, 5, ST7735_WHITE);          // 空心多边形
ST7735_FillPolygon(xs, ys, 5, ST7735_ORANGE);         // 实心多边形 (扫描线填充)
```

### 4. DMA 高性能路径

```cpp
ST7735_FillScreen_DMA(ST7735_BLACK);        // DMA 全屏填充
ST7735_FillRectangle_DMA(0, 0, 80, 40, ...); // DMA 区域填充
ST7735_DrawImage_DMA(0, 0, 160, 80, img);    // DMA 图像 (RGB565)
```

> DMA 版内部为**阻塞式等待完成** (`dma_transfer_complete` 标志 + `HAL_SPI_TxCpltCallback`), 无需额外同步。

## 字符级编译

### 配置 (`fonts_config.hpp`)

```cpp
// ASCII 子集字符集 (按需增删)
#define FONT_SUBSET_7X10_CHARS " !0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ"

// 中文子集: 声明要编译的 Unicode 码点
// 例: U+4F60=你  U+597D=好
#define FONT_SUBSET_ZH_CHARS  { 0x4F60, 0x597D }
```

### 字模格式

- ASCII 子集: 每个字符 `height` 个 `uint16_t`, **bit15 = 最左列**
- 中文子集: 16x16, 每行一个 `uint16_t` (bit15=最左), 共 16 行

### 生成新字模 (Python + PIL)

```python
from PIL import Image, ImageDraw, ImageFont
font = ImageFont.truetype('C:/Windows/Fonts/simhei.ttf', 16)
img = Image.new('L', (16, 16), 0)
d = ImageDraw.Draw(img)
bbox = d.textbbox((0,0), ch, font=font)
d.text(((16-(bbox[2]-bbox[0]))//2-bbox[0], (16-(bbox[3]-bbox[1]))//2-bbox[1]), ch, font=font, fill=255)
rows = []
for r in range(16):
    v = 0
    for c in range(16):
        if img.getpixel((c, r)) > 100: v |= (0x8000 >> c)   # bit15=最左列
    rows.append(v)
print(', '.join(f'0x{x:04X}' for x in rows))
```

### 未编译字符行为

渲染时查表失败返回 `nullptr`, 该字符**自动跳过** (不显示也不占位), 其余字符不受影响。

## 演示 (SmartOneT1)

`Core/Src/st7735_lib_demo.cpp` — 5 页自动轮播:

| 页面 | 内容 |
|------|------|
| Page 0 | 中文欢迎语 "你好" (仅 2 个字模 ~64B) |
| Page 1 | 字符级编译演示 (子集字符) |
| Page 2 | 空心图形 (线/圆/椭圆/三角/多边形/矩形/圆角矩形) |
| Page 3 | 实心图形 (同系列填充) |
| Page 4 | 图形叠加动画 (小球沿轨道旋转, 仅更新动态像素, 无整屏刷新) |

## 移植指南

1. CubeMX 配置 SPI (Master, 8bit, MSB, 速率≤10MHz) + 引脚 (SCK/MOSI/CS/DC/RESET/EN)
2. 修改 `st7735.hpp` 的 `ST7735_SPI_PORT` / `ST7735_*_Pin` 宏
3. 屏幕尺寸/偏移: `st7735.hpp` 中 `WIDTH`/`HEIGHT`/`XSTART`/`YSTART`/`ROTATION`
4. 如需 DMA: CubeMX 开启 SPI TX DMA, 使用 `_DMA` 后缀函数
5. 将 ST7735/ 下 `.cpp` 加入工程编译 (Keil: Add Existing Files)

## 验收记录

- Keil MDK-ARM (AC6 V6.24) 全量重建: **0 Error, 0 Warning**
- 烧录: Erase/Program/Verify OK, Application running
- 硬件实测 (SmartOneT1): 中文显示正常, 动画无闪烁, 上电直达 demo

## 许可证

MIT License