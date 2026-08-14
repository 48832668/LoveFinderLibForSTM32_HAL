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

## 新手三步上手

> 第一次用本库? 按这三步走, 10 分钟点亮屏幕。

**Step 1 — 接线与 CubeMX 配置** (SmartOneT1 已配好, 跳过)

| 屏引脚 | 接 STM32 | CubeMX 配置 |
|--------|----------|-------------|
| SCK    | PB3 (SPI1_SCK) | SPI1: Master, 8bit, MSB, Prescaler≤8 (≤10.5MHz) |
| MOSI   | PB5 (SPI1_MOSI) | 同上 |
| CS     | PB4 | GPIO Output, 初始 High |
| DC     | PB8 | GPIO Output |
| RES    | PB9 | GPIO Output |
| EN     | PA15 | GPIO Output, 初始化时拉 High (屏幕供电) |

> 换板子? 改 `st7735.hpp` 顶部的 `ST7735_SPI_PORT` / `ST7735_*_Pin` 宏即可, 库内无运行时绑定。

**Step 2 — 工程集成** (Keil 为例)
1. 把 `ST7735/` 下 4 个 `.cpp` 加入工程: `st7735.cpp` `fonts.c` `fonts_subset.cpp` `icons.c`
2. Include 路径加 `ST7735/`
3. 在 `main()` 外设初始化后调用 `ST7735_Init()`

**Step 3 — 第一个程序**
```cpp
#include "st7735.hpp"

ST7735_Init();                              // 初始化屏幕
ST7735_FillScreen_DMA(ST7735_BLACK);        // 清屏 (DMA 版)
ST7735_WriteString(10, 10, "HELLO", Font_7x10, ST7735_WHITE, ST7735_BLACK);
ST7735_DrawCircle(80, 40, 30, ST7735_YELLOW);      // 空心圆
ST7735_FillCircle(120, 40, 15, ST7735_GREEN);      // 实心圆
ST7735_WriteStringUTF8(20, 60, "\xE4\xBD\xA0\xE5\xA5\xBD",
                       Font_Subset_ZH_16x16, ST7735_YELLOW, ST7735_BLACK); // "你好"
```

遇到问题? 见 [故障排除](#故障排除) 与 [新手常见问题](#新手常见问题)。

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

## 如何启用 DMA (完整指南)

### 前提: CubeMX 配置 SPI TX DMA (一次性)

SmartOneT1 工程**已配置好**, 可直接调用, 跳过本节。

新平台需 3 步 (CubeMX 图形化勾选即可):

1. **SPI1 TX DMA**: DMA Settings 添加 `SPI1_TX`, 推荐 `DMA2_Stream3 / Channel3 / MEMORY_TO_PERIPH`
2. **DMA 中断**: 使能对应 DMA Stream 中断 (库内忙等完成标志, 依赖中断置位)
3. **校验**: 生成后 `spi.c` 应有 `__HAL_LINKDMA(hspi1, hdmatx, hdma_spi1_tx);`

> ⚠️ **硬前提**: 若 DMA 未 LINKDMA, `HAL_SPI_Transmit_DMA` 返回错误, 库内 `while(!dma_transfer_complete)` 将**永久忙等 (死锁)**。调试时若卡死在 DMA 函数, 先查这一步。

### 启用方式: 调用 `_DMA` 后缀函数

库只有 3 个 DMA 入口 (大块传输才有 DMA 价值):

| 函数 | 用途 | 普通版对比 |
|------|------|-----------|
| `ST7735_FillScreen_DMA(color)` | 全屏填充/清屏 | `ST7735_FillScreen` |
| `ST7735_FillRectangle_DMA(x,y,w,h,color)` | 区域填充 | `ST7735_FillRectangle` |
| `ST7735_DrawImage_DMA(x,y,w,h,data)` | RGB565 图像 | `ST7735_DrawImage` |

其余绘制 (字符/点/线/圆/多边形) **保持轮询** — 小传输下 DMA 启动开销反而更大。

### 何时用 DMA, 何时用轮询

| 场景 | 推荐 | 原因 |
|------|------|------|
| 页面切换全屏清屏 (≥100px 级) | `FillScreen_DMA` | 大块传输, DMA 释放 CPU |
| 图像显示 | `DrawImage_DMA` | 同上 |
| 动画局部擦除/绘制 (≤几百 px) | 轮询版 | DMA 启动开销 > 传输收益 |
| 文本/小图形 | 轮询版 | 逐像素小传输 |

### 库内部机制 (无需干预)

```cpp
static void ST7735_WriteData_DMA(uint8_t *buff, size_t buff_size)
{
    dma_transfer_complete = 0;
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, buff, buff_size);
    while (!dma_transfer_complete);   // 忙等, 函数返回时传输必已完成
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)  // 库内定义
{
    if (hspi->Instance == SPI1) dma_transfer_complete = 1;
}
```

### 常见坑

1. **DMA 未 LINKDMA → 死锁** (见前提, 最重要)
2. **`HAL_SPI_TxCpltCallback` 重名冲突**: 库定义了该回调, 若你的其他模块 (如 SPI2 Flash) 也定义了同名回调, 链接报重复定义 → 合并到一个回调里按 `hspi->Instance` 分发
3. **DMA 缓冲区生命周期**: `DrawImage_DMA` 的 `data` 指针在函数返回前必须有效 (库是阻塞式, 返回即传输完, 一般无此问题)

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

## 新手常见问题

**Q1: 屏幕全黑 / 不亮?**
- 查 EN (PA15) 是否拉高 — 屏幕供电, 最常见原因
- 查 SPI 是否已 `HAL_SPI_Init` (CubeMX 生成代码不能删)
- 查 `ST7735_Init()` 是否在 GPIO/SPI 初始化**之后**调用

**Q2: 死机卡死 (进 HardFault)?**
- 是否用了 DMA 函数但 CubeMX 没配 SPI TX DMA → `while(!dma_transfer_complete)` 死锁, 见 [如何启用 DMA](#如何启用-dma-完整指南) 前提
- 数组越界: `DrawImage` 的 `data` 长度必须 ≥ `w*h*2` 字节

**Q3: 中文显示乱码 / 空白?**
- 乱码 → 字模数据错误, 按 [生成新字模](#生成新字模-python--pil) 重新生成 (bit15=最左列)
- 空白 → 该字符未编译进子集, 检查 `FONT_SUBSET_ZH_CHARS` / `FONT_SUBSET_7X10_CHARS`
- 文本乱码 → 用 `ST7735_WriteStringUTF8` (UTF-8 输入), 不要用 GBK 编码源文件

**Q4: 显示错位 / 旋转方向不对?**
- 调 `st7735.hpp` 的 `WIDTH/HEIGHT/XSTART/YSTART/ROTATION` (本工程 160x80 已配好)
- 确认 SPI 时序: CPOL=Low, CPHA=1Edge (Mode 0)

**Q5: 想加一个汉字怎么做?**
1. Python+PIL 生成字模 (见 [生成新字模](#生成新字模-python--pil))
2. `fonts_subset.cpp` 加 `static const uint16_t` 数组 + 查表 case
3. `fonts_config.hpp` 的 `FONT_SUBSET_ZH_CHARS` 加码点

## 故障排除

### 显示异常
1. 检查 SPI 连线 (SCK/MOSI/CS/DC/RESET/EN) 与 CubeMX 引脚一致
2. 验证时序: CPOL/CPHA, 时钟频率 ≤ 规格
3. 复位时序: RES 低电平 ≥5ms, 上升沿后等待 SLPOUT 完成

### 文字乱码
1. 字模数据格式: 每行一个 uint16_t, bit15=最左列 (与渲染引擎 `(data<<j)&0x8000` 匹配)
2. 中文用 UTF-8 输入 + `ST7735_WriteStringUTF8`
3. 前景/背景色参数顺序正确

### 图形错位
1. 检查坐标系: 原点左上, x 向右, y 向下
2. 验证 `ROTATION` (MADCTL) 与实际屏幕方向一致
3. 确认 `SetAddressWindow` 的 CASET/RASET 范围 (160x80 已配)

## 验收记录

- Keil MDK-ARM (AC6 V6.24) 全量重建: **0 Error, 0 Warning**
- 烧录: Erase/Program/Verify OK, Application running
- 硬件实测 (SmartOneT1): 中文显示正常, 动画无闪烁, 上电直达 demo

## 许可证

MIT License