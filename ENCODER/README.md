# LoveFinderLib :: ENCODER — 旋转编码器驱动库（TIM 硬件编码器模式）

> 适用环境：任意 **STM32 + ST 标准 HAL 库**（C++17）。
> 命名空间：`LoveFinderLib`

本库基于 **STM32 TIM 硬件编码器模式** 驱动旋转编码器。AB 相边沿计数完全由 **TIM 硬件**完成（不占用 CPU 轮询）；位置读取采用**带符号增量累加**——把 16 位硬件计数器按有符号解释（`0..65535 ↔ -32768..32767`），两次读取的带符号差值天然吸收回绕。

**无需更新中断、无竞态、不会死锁、回绕边界不再跳变。**

---

## 1. 特性

- ✅ **不占用轮询 / CPU**：AB 相边沿计数由 TIM 硬件完成，主循环无需逐脉冲解码。
- ✅ **有符号增量累加，回绕安全**：16 位计数器按有符号读，`0→65535` 即自然 `-1`，无需中断扩展 32 位，边界不跳变。
- ✅ 支持型号预设：EC06 / EC11 / EC12 / EC16 / EC20，以及自定义每格脉冲数。
- ✅ 计数范围（32 位）：
  - **编译期**：头文件宏 `ENCODER_MIN_DEFAULT` / `ENCODER_MAX_DEFAULT`；
  - **运行期**：`setRange(min, max)` 动态修改，超出自动限幅。
- ✅ **编译期宏 `ENCODER_SWAP_AB` 选择交换 AB 相**，修正旋转方向（无需运行期配置）。
- ✅ `start()` 自动：HAL 启动 + 建立基线（无需任何中断接线）。
- ✅ `changed()` 检测变化，可配合 UI 定时器做事件驱动刷新，不必忙等。
- ✅ 纯 C++17 接口，位于 `LoveFinderLib` 命名空间。

---

## 2. 硬件要求

- 编码器 A / B 相接到支持编码器模式的 TIM 通道（如 STM32F4 的 TIM3_CH1/TIM3_CH2）。
- 需先用 CubeMX 或 HAL 把该 TIM 配置为 **Encoder Mode**（本库只负责启动/停止与读数）。
- **机械编码器建议**：在 CubeMX 中给编码器引脚使能**内部上拉**（GPIO_PULLUP），并设置 **IC1/IC2 输入滤波**（如 `IC1Filter/IC2Filter = 15`），抑制触点抖动，避免计数毛刺。
- 旋转方向由 `ENCODER_SWAP_AB` 宏编译期决定（见 §5）。

---

## 3. 目录结构

```
ENCODER/
├── ENCODER.hpp    // 头文件：宏、枚举、参数结构体、Encoder 类、命名空间
├── ENCODER.cpp    // 实现：初始化、范围限制、计数换算、启停
└── README.md      // 本文档
```

---

## 4. 移植到你的工程（唯一需要改的地方）

库只依赖通用 STM32 HAL 定时器/编码器 API，跨系列移植时**只需修改 `ENCODER.hpp` 头部的一行 include**：

```cpp
#include "stm32f4xx_hal.h"   // <<<< 移植到其他 STM32 系列时，只改这一行
```

各系列对应头文件：

| 系列 | 头文件 |
|------|--------|
| F0 / F1 / F3 / F4 / F7 | `stm32f0xx_hal.h` / `stm32f1xx_hal.h` / `stm32f3xx_hal.h` / `stm32f4xx_hal.h` / `stm32f7xx_hal.h` |
| L0 / L1 / L4 / L5 | `stm32l0xx_hal.h` / `stm32l1xx_hal.h` / `stm32l4xx_hal.h` / `stm32l5xx_hal.h` |
| G0 / G4 | `stm32g0xx_hal.h` / `stm32g4xx_hal.h` |
| H5 / H7 | `stm32h5xx_hal.h` / `stm32h7xx_hal.h` |
| WB / WL | `stm32wbxx_hal.h` / `stm32wlxx_hal.h` |
| C0 / U0 / U5 | `stm32c0xx_hal.h` / `stm32u0xx_hal.h` / `stm32u5xx_hal.h` |

> 若你的工程用 CubeMX 生成，也可以改用 `#include "main.h"`。

把 `ENCODER.cpp` 加入编译，`ENCODER.hpp` 加入头文件包含路径即可。

---

## 5. 编译期宏（头文件顶部）

### 5.1 默认计数范围

```cpp
#ifndef ENCODER_MIN_DEFAULT
    #define ENCODER_MIN_DEFAULT   (-32768)   // 默认最小值
#endif

#ifndef ENCODER_MAX_DEFAULT
    #define ENCODER_MAX_DEFAULT   (32767)    // 默认最大值
#endif
```

### 5.2 是否交换 AB 相（修正方向）

```cpp
#ifndef ENCODER_SWAP_AB
    #define ENCODER_SWAP_AB   0    // 0=不交换, 1=交换 (按实际接线在编译期设定)
#endif
```

> 该宏只决定 `ENCODER_Config::getDefault()` 的默认 `swapAB`；个别实例仍可用
> `config.swapAB` 在运行期覆盖（高级用法）。

---

## 6. 运行时修改范围（MCU 运行期间可动态调整）

```cpp
enc.setRange(0, 100);        // 运行期把范围改为 0..100，超出自动限幅
enc.setRange(-50, 50);       // 随时再改
```

---

## 7. API 说明

### 7.1 类型枚举 `e_ENCODER_Type`

```cpp
enum class e_ENCODER_Type : uint8_t {
    EC06, EC11, EC12, EC16, EC20, CUSTOM, COUNT
};
```

各型号每格脉冲数：

| 型号 | 每格脉冲数 |
|------|-----------|
| EC06 | 2 |
| EC11 / EC12 / EC16 / EC20 | 4 |

> **注意**：型号务必与硬件一致。若把 EC11（4 脉冲/格）配成 EC06（2 脉冲/格），
> 转一格会显示 2 而非 1。

### 7.2 配置结构体 `ENCODER_Config`

```cpp
struct ENCODER_Config {
    e_ENCODER_Type type;            // 编码器类型 (CUSTOM 时使用 countsPerDetent)
    uint8_t countsPerDetent;        // 每格脉冲数 (仅 CUSTOM 有效)
    bool swapAB;                    // 是否交换 AB 相 (修正旋转方向)
    int32_t minValue;               // 最小计数
    int32_t maxValue;               // 最大计数
    static ENCODER_Config getDefault();   // 使用头文件宏默认值
};
```

### 7.3 参数结构体 `ENCODER_Params`

```cpp
struct ENCODER_Params {
    const char* name;            // 型号名称
    uint8_t countsPerDetent;     // 每格脉冲数
    const char* description;     // 描述
};
```

### 7.4 常用方法

```cpp
Encoder();
Encoder(TIM_HandleTypeDef* tim, const ENCODER_Config& config = ENCODER_Config::getDefault());

void  init(TIM_HandleTypeDef* tim, const ENCODER_Config& config);
void  init(TIM_HandleTypeDef* tim);                     // 默认配置
void  setRange(int32_t minVal, int32_t maxVal);         // 运行期改范围
void  start();                    // HAL 启动 + 建立读取基线 (无需任何中断接线)
void  stop();                     // HAL 停止
int32_t getCount() const;         // 当前 32 位格计数 (限幅)
int32_t getRawPosition() const;   // 当前 32 位原始脉冲计数 (回绕安全)
bool  changed();                  // 自上次调用后是否变化 (无需轮询)
void  setCount(int32_t value);
void  addCount(int32_t delta);
void  invertCount();
void  reset();                     // 复位到最小值
uint8_t getCountsPerDetent() const;
e_ENCODER_Type getType() const;
static const ENCODER_Params& getParams(e_ENCODER_Type type);
```

---

## 8. 接入示例（无需任何中断接线）

```cpp
#include "main.h"          // 或 stm32fxxx_hal.h
#include "ENCODER.hpp"
using namespace LoveFinderLib;

extern TIM_HandleTypeDef htim3;   // CubeMX 已配置为编码器模式
Encoder enc;

void app_init(void)
{
    ENCODER_Config cfg = ENCODER_Config::getDefault();
    cfg.type     = e_ENCODER_Type::EC11;   // SmartOneT1 等多数开发板: EC11, 4 脉冲/格
    cfg.minValue = -128;
    cfg.maxValue = 128;
    // swapAB 已由 ENCODER_SWAP_AB 宏在编译期决定 (或在此覆盖)
    enc.init(&htim3, cfg);
    enc.start();                             // 启动硬件计数 (无需中断)
}

void app_loop(void)          // 主循环：仅在需要时读取，无需逐脉冲解码
{
    // 配合 UI 定时器做事件驱动刷新
    if (enc.changed())
    {
        int32_t c = enc.getCount();       // 当前格计数
        update_display(c);                // 驱动菜单 / 数值 / 进度条等
    }
}
```

---

## 9. 注意事项

- **必须**先用 CubeMX / HAL 把 TIM 配置为编码器模式；本库只负责启动/停止与读数。
- **无需任何中断接线**：没有更新中断、没有 `HAL_TIM_PeriodElapsedCallback`、没有 `TIMx_IRQHandler`。
- **机械编码器建议**：CubeMX 里给编码器引脚开内部上拉 + 设置 IC1/IC2 输入滤波，抑制抖动毛刺。
- 读取位置用**有符号增量累加**：两次读取间隔内增量不可能超过 ±32767 脉冲（人工转速远达不到），故回绕修正绝对可靠。
- 旋转方向不对时，把 `ENCODER_SWAP_AB` 宏设为 1（或在 `config.swapAB` 运行期覆盖）。
- **型号务必匹配硬件**：EC11 是 4 脉冲/格，配成 EC06 会导致转一格显示 2。
- 本库为 **C++17**，请确保工程开启 C++（Keil MDK 中把源文件设为 `.cpp` 或启用 C++ 编译）。
- 只依赖 ST 标准 HAL 库；非 STM32 / 非 HAL 的外设库不支持。

---

*LoveFinderLib :: ENCODER — 2026*
