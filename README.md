# LoveFinderLib for STM32 HAL

> **轻量级 STM32 外设驱动库（C++17）** · 基于 ST 标准 HAL 库
> Lightweight STM32 peripheral driver library (C++17) built on the ST Standard HAL.

一套面向 **STM32 + ST 标准 HAL** 的模块化外设驱动集合，纯 C++17 编写，风格统一、移植简单、CPU 占用极低。命名空间 `LoveFinderLib`。

A modular collection of STM32 peripheral drivers, written in modern C++17 with a unified style, easy porting and minimal CPU overhead. Namespace: `LoveFinderLib`.

---

## 模块 Modules

| 模块 | 说明 | 关键特性 |
|------|------|----------|
| [**BUTTON**](BUTTON/README.md) | EXTI 驱动稳定按键库 | 单击 / 双击 / 长按，长按充能条，事件标志位，运行期可调阈值 |
| [**ENCODER**](ENCODER/README.md) | 旋转编码器驱动库（TIM 硬件编码器模式） | 有符号增量累加（无中断无竞态）、方向/活动/范围 API、事件标志位、型号预设 |

更多模块持续添加中。More modules coming.

---

## 快速开始 Quick Start

### 1. 硬件要求 Hardware requirements

- 任意 STM32 MCU + ST 标准 HAL 库（STM32Cube HAL）
- 每个模块的硬件要求见对应 `README.md`

### 2. 接入工程 Add to your project

以 **Keil MDK / STM32CubeIDE** 为例：

1. 拷贝需要的模块目录（如 `ENCODER/`）到你的工程，例如 `Drivers/LoveFinderLib/ENCODER/`
2. 把 `ENCODER.cpp`（及 `ENCODER.hpp` 所在目录）加入编译 / include 路径
3. 按模块 `README.md` 用 CubeMX 配置对应外设（TIM 编码器模式 / EXTI 中断等）
4. 在代码中包含头文件并使用

```cpp
#include "ENCODER.hpp"
using namespace LoveFinderLib;

extern TIM_HandleTypeDef htim3;   // CubeMX 已配置为编码器模式
Encoder enc;

void app_init(void)
{
    ENCODER_Config cfg = ENCODER_Config::getDefault();
    cfg.type     = e_ENCODER_Type::EC11;   // 按实际型号
    cfg.minValue = -128;
    cfg.maxValue = 128;
    enc.init(&htim3, cfg);
    enc.start();
}

void app_loop(void)
{
    if (enc.changed())
    {
        int32_t c = enc.getCount();
        update_display(c);        // 驱动你的 UI
    }
}
```

### 3. 移植到其他 STM32 系列 Port to other STM32 families

每个模块**只需修改头文件顶部一行 include**：

```cpp
#include "stm32f4xx_hal.h"   // <<<< 改成你所用器件的 HAL 头文件
```

支持系列 Supported families:

| 系列 | 头文件 |
|------|--------|
| F0 / F1 / F3 / F4 / F7 | `stm32f0xx_hal.h` / `stm32f1xx_hal.h` / `stm32f3xx_hal.h` / `stm32f4xx_hal.h` / `stm32f7xx_hal.h` |
| L0 / L1 / L4 / L5 | `stm32l0xx_hal.h` / `stm32l1xx_hal.h` / `stm32l4xx_hal.h` / `stm32l5xx_hal.h` |
| G0 / G4 | `stm32g0xx_hal.h` / `stm32g4xx_hal.h` |
| H5 / H7 | `stm32h5xx_hal.h` / `stm32h7xx_hal.h` |
| WB / WL | `stm32wbxx_hal.h` / `stm32wlxx_hal.h` |
| C0 / U0 / U5 | `stm32c0xx_hal.h` / `stm32u0xx_hal.h` / `stm32u5xx_hal.h` |

---

## 设计理念 Design Principles

- **纯 C++17，无 C API 残留**：全库统一 C++ 接口、`LoveFinderLib` 命名空间、`init(handle, config)` 风格
- **统一配置结构体**：每个模块用 `XXX_Config` + `getDefault()` 工厂，编译期宏设默认值，运行期可动态修改
- **事件驱动 + 标志位**：`changed()` / `update()` + 事件标志位（`peekFlags` / `getAndClearFlags` / `testFlag`），配合 UI 定时器做事件驱动刷新，**不忙等、不逐脉冲轮询**
- **CPU 占用极低**：硬件外设承担核心工作（TIM 硬件编码器计数 / EXTI 中断置位），主循环开销仅几十个周期
- **多实例支持**：无共享静态状态，同一模块可在单板创建多个实例（多编码器 / 多按键）
- **中断安全**：标志位 `volatile` 设计，可在用户自定义中断中安全读取 / 置位
- **移植成本 ≈ 0**：只依赖通用 HAL API，跨系列只需改一行 include

---

## 目录结构 Repository Layout

```
LoveFinderLibForSTM32_HAL/
├── README.md          // 本文档
├── BUTTON/
│   ├── BUTTON.hpp     // 头文件：宏、枚举、结构体、Button 类
│   ├── BUTTON.cpp     // 实现：EXTI 配置、状态机、消抖/事件逻辑
│   └── README.md      // BUTTON 使用文档
└── ENCODER/
    ├── ENCODER.hpp    // 头文件：宏、枚举、结构体、Encoder 类
    ├── ENCODER.cpp    // 实现：初始化、计数换算、启停
    └── README.md      // ENCODER 使用文档
```

---

## 每个模块的详细文档

- [BUTTON — EXTI 驱动稳定按键库](BUTTON/README.md)
- [ENCODER — 旋转编码器驱动库](ENCODER/README.md)

---

## License

本项目采用 **MIT License**。详见 [LICENSE](LICENSE)。

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

*LoveFinderLib for STM32 HAL — 2026*
