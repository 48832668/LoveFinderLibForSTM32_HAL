# LoveFinderLib :: BUTTON — 基于 EXTI 的稳定按键库

> 适用环境：任意 **STM32 + ST 标准 HAL 库**（C++17）。
> 命名空间：`LoveFinderLib`

本库用于稳定识别按键的 **单击 / 双击 / 长按**，并附带一个「长按充能」矩形进度条，同时支持在 **MCU 运行期间动态修改** 长按阈值与双击间隔阈值。

---

## 1. 特性

- ✅ 硬性要求：KEY 引脚启用 **EXTI 外部中断**，初始化时自动配置为 **下降沿触发**（`GPIO_MODE_IT_FALLING`）并自动使能对应 **NVIC**。
  - 即使你已用 CubeMX 配置好 EXTI，`init()` 仍会再次显式配置为下降沿，保证结果一致。
- ✅ 稳定区分 **单击 / 双击 / 长按**（含软件消抖、双击窗口判定）。
- ✅ 长按阈值、双击间隔阈值：
  - **编译期**：通过头文件宏设定默认值；
  - **运行期**：通过 `setLongPressMs()` / `setDoubleClickMs()` 动态修改。
- ✅ 附带「长按充能」矩形区域（charge meter）：
  - 长按（达到阈值后）持续充能，矩形内填充慢慢变满；
  - **填满一次 → 长按次数 +1**，并清零继续充能（持续按住可多次累加）；
  - **未填满就松开 → 内部填充慢慢消退**。
- ✅ 提供事件标志位（event flags）与事件计数命名访问器，便于 UI 非破坏读取。
- ✅ 纯 C++17 接口，位于 `LoveFinderLib` 命名空间。

---

## 2. 硬件要求

- 按键 IO 需有 **外部上拉**（按键按下为低电平）。
  - 库内还会再开 `GPIO_PULLUP` 内部上拉作为兜底，防止误触发。
- 若按键是"按下为高电平"（下拉/上拉到高），请把配置中的 `activeLow` 设为 `false`。

---

## 3. 目录结构

```
BUTTON/
├── BUTTON.hpp    // 头文件：宏定义、枚举、结构体、Button 类、命名空间
├── BUTTON.cpp    // 实现：EXTI 配置、状态机、充能/消退逻辑
└── README.md     // 本文档
```

---

## 4. 移植到你的工程（唯一需要改的地方）

库只依赖通用 STM32 HAL API，因此跨系列移植时，**只需修改 `BUTTON.hpp` 头部的一行 include**：

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
>
> `configureExti()` 内的 EXTI→NVIC 中断号映射已通过预处理自动适配
> 单线式（F0/F1/F3/F4/F7、L4/L5、U5、WB/WL、G4）、分组式（G0/C0）、H5 式命名。

把 `BUTTON.cpp` 加入编译，`BUTTON.hpp` 加入头文件包含路径即可。

---

## 5. 编译期默认阈值宏（头文件顶部）

这些宏决定 `BUTTON_Config::getDefault()` 的默认值，可在包含本头文件前用 `#define` 覆盖：

```cpp
#ifndef BUTTON_LONG_PRESS_MS_DEFAULT
    #define BUTTON_LONG_PRESS_MS_DEFAULT    1000    // 长按判定阈值 (ms)
#endif

#ifndef BUTTON_DOUBLE_CLICK_MS_DEFAULT
    #define BUTTON_DOUBLE_CLICK_MS_DEFAULT  300     // 双击间隔阈值 (ms)
#endif

#ifndef BUTTON_DEBOUNCE_MS_DEFAULT
    #define BUTTON_DEBOUNCE_MS_DEFAULT      20      // 消抖时间 (ms)
#endif

#ifndef BUTTON_CHARGE_FULL_MS_DEFAULT
    #define BUTTON_CHARGE_FULL_MS_DEFAULT   2000    // 长按充能填满所需时间 (ms)
#endif

#ifndef BUTTON_DECAY_MS_DEFAULT
    #define BUTTON_DECAY_MS_DEFAULT         3000    // 未填满松开后消退时间 (ms)
#endif
```

示例（编译期改阈值）：

```cpp
#define BUTTON_LONG_PRESS_MS_DEFAULT   1500
#define BUTTON_DOUBLE_CLICK_MS_DEFAULT 250
#include "BUTTON.hpp"
```

---

## 6. 运行时阈值修改（MCU 运行期间可动态调整）

即使初始化后，也能随时修改阈值，无需重新编译：

```cpp
btn.setLongPressMs(2000);      // 运行期把长按阈值改为 2000ms
btn.setDoubleClickMs(400);     // 运行期把双击间隔改为 400ms

uint16_t lp = btn.getLongPressMs();     // 读回当前长按阈值
uint16_t dc = btn.getDoubleClickMs();   // 读回当前双击间隔
```

> 相关方法（均在 `LoveFinderLib::Button` 内）：
> - `void setLongPressMs(uint16_t ms)` / `uint16_t getLongPressMs() const`
> - `void setDoubleClickMs(uint16_t ms)` / `uint16_t getDoubleClickMs() const`

---

## 7. API 说明

### 7.1 配置结构体 `BUTTON_Config`

```cpp
struct BUTTON_Config {
    uint16_t debounceMs;      // 消抖时间 (ms)
    uint16_t longPressMs;     // 长按判定阈值 (ms)
    uint16_t doubleClickMs;   // 双击间隔时间 (ms)
    uint16_t chargeFullMs;    // 长按充能填满所需时间 (ms)
    uint16_t decayMs;         // 未填满松开后消退时间 (ms)
    bool activeLow;           // true=低电平按下, false=高电平按下
    static BUTTON_Config getDefault();   // 使用头文件宏默认值
};
```

### 7.2 事件枚举 `e_BUTTON_Event`

```cpp
enum class e_BUTTON_Event : uint8_t {
    NONE, CLICK, DOUBLE_CLICK, LONG_PRESS, PRESS_DOWN, RELEASE
};
```

### 7.3 事件标志位 `BUTTON_Flag`

```cpp
enum class BUTTON_Flag : uint8_t {
    PRESS_DOWN = 0x01, CLICK = 0x02, DOUBLE_CLICK = 0x04,
    LONG_PRESS = 0x08, RELEASE = 0x10
};
```

标志位在事件产生时置位并保持，直到被读取/清除，适合 UI 轮询：

```cpp
uint8_t f = btn.peekFlags();            // 非破坏读取全部标志
bool   ok = btn.testFlag(BUTTON_Flag::DOUBLE_CLICK); // 测试某位
uint8_t f2 = btn.getAndClearFlags();    // 读取并清除
btn.clearFlags();                        // 直接清除
```

### 7.4 计数器访问器

```cpp
uint32_t c  = btn.getClickCount();       // 单击次数
uint32_t d  = btn.getDoubleClickCount(); // 双击次数
uint32_t l  = btn.getLongPressCount();   // 长按次数（充能填满次数）
```

### 7.5 长按充能

```cpp
uint8_t pct = btn.getChargePercent();    // 0..100，UI 据此绘制矩形
```

### 7.6 状态 / 物理

```cpp
bool isPressed() const;        // 当前是否按下
e_BUTTON_State getState() const; // 内部状态
```

---

## 8. 接入示例（完整）

```cpp
#include "main.h"          // 或 stm32fxxx_hal.h
#include "BUTTON.hpp"
using namespace LoveFinderLib;

Button btn;

void app_init(void)
{
    BUTTON_Config cfg = BUTTON_Config::getDefault();
    cfg.activeLow = true;             // 外部上拉，按下为低
    cfg.longPressMs = 1000;           // 长按阈值 1000ms
    btn.init(KEY_GPIO_Port, KEY_Pin, cfg);   // 自动配置下降沿 EXTI + NVIC
}

void app_loop(void)          // 主循环 (建议 1-10ms 调用一次)
{
    e_BUTTON_Event evt = btn.update();

    if (btn.testFlag(BUTTON_Flag::DOUBLE_CLICK))
        doDoubleClickAction();

    uint8_t pct = btn.getChargePercent();   // 驱动充能矩形
    draw_charge_bar(pct);

    if (btn.getLongPressCount() != last)
        update_long_count_display(btn.getLongPressCount());

    // 运行期调整阈值示例：
    if (need_slow_threshold)
        btn.setLongPressMs(2500);
}

/* 在 stm32fxxx_it.c / main 中把 EXTI 回调分发到按键 */
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY_Pin)
        btn.onExti();
}
```

---

## 9. 注意事项

- **必须**在中断上下文把 EXTI 回调分发给 `btn.onExti()`（见上文），否则按下的下降沿无法被记录。
- `update()` 需在主循环中**定期、高频**调用（建议 1~10ms），双击窗口（默认 300ms）依赖它计时。
- 长按阈值、双击间隔阈值既可在编译期用宏定，也可在运行期用 `setXxxMs()` 动态改。
- 本库为 **C++17**，请确保工程开启 C++（Keil MDK 中把源文件设为 `.cpp` 或启用 C++ 编译）。
- 只依赖 ST 标准 HAL 库；非 STM32 / 非 HAL 的外设库不支持。

---

*LoveFinderLib :: BUTTON — 2026*
