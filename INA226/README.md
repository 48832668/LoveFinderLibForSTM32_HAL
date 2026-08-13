# LoveFinderLib :: INA226 — 电流/电压/功率监测驱动库（I2C）

> 适用环境：任意 **STM32 + ST 标准 HAL 库**（C++17）。
> 命名空间：`LoveFinderLib`

本库基于 **STM32 HAL I2C 接口**（`HAL_I2C_Mem_Read/Write`）驱动 **INA226** 电流/电压/功率监测芯片。提供总线电压、分流电压、电流、功率、在线检测等测量接口，以及工作模式 / 转换时间 / 平均次数配置与校准。

---

## 1. 特性

- ✅ **I2C 从机地址枚举化**：地址由外部 **A0/A1 引脚**连接决定，提供全部 **16 种组合**枚举（`e_INA226_I2CAddr`），初始化时直接传枚举，无需手算地址。
- ✅ 测量接口：
  - 总线电压 `getBusVoltage()`（mV）
  - 分流电压 `getShuntVoltage()`（uV，可负）
  - 电流 `getCurrent()`（mA）
  - 功率 `getPower()`（mW）
  - 制造商 ID / Die ID / 在线检测 `isOnline()`
- ✅ **总线扫描 `SCANINA226()`**：遍历地址表全部 16 个地址，逐个通讯探测 + 制造商 ID 校验（0x5449），一次性找出总线上所有可用的 INA226 设备。
- ✅ 工作模式 / 转换时间 / 平均次数配置（`config()` 或初始化时 `INA226_Config`）。
- ✅ 校准（`setCalibration()`），支持运行期修改分流电阻。
- ✅ **告警系统**：告警使能（`setMaskEnable`）、物理量阈值（`setAlertLimit`）、告警标志读取（`getAlertFlags`）、极性/锁存配置。
- ✅ **ALERT 引脚可选绑定**：`bindAlertPin()` / `bindAlertPin` 支持绑定 ALERT 引脚读电平或配合 EXTI 中断；**不绑定时所有功能仍可用**（`getAlertFlags()` 纯寄存器轮询）。
- ✅ **触发模式单次转换**：`triggerConversion()` + `isConversionReady()` 轮询转换完成。
- ✅ 配置/校准寄存器读回（`getConfig()` / `getCalibration()`），Die 修订号（`getDieRevision()`）。
- ✅ 软件复位（`reset()`）。
- ✅ 纯 C++17 接口，位于 `LoveFinderLib` 命名空间，与 BUTTON/ENCODER 库风格一致。

---

## 2. 硬件要求

- INA226 通过 **I2C** 连接 STM32（如 STM32F4 的 I2C1）。
- 需先用 CubeMX / HAL 把对应 I2C 初始化为可用状态（本库只负责读写，不初始化 I2C 外设）。
- INA226 的从机地址由 **A0 / A1 引脚** 的连接方式决定（见 §5 地址表）。

---

## 3. 目录结构

```
INA226/
├── INA226.hpp    // 头文件：寄存器地址、模式枚举、地址枚举、Config、INA226 类
├── INA226.cpp    // 实现：读写寄存器、配置、校准、测量、在线检测、复位
└── README.md     // 本文档
```

---

## 4. 移植到你的工程（唯一需要改的地方）

库只依赖通用 STM32 HAL I2C API，跨系列移植时**只需修改 `INA226.hpp` 头部的一行 include**：

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

把 `INA226.cpp` 加入编译，`INA226.hpp` 加入头文件包含路径即可。

---

## 5. I2C 地址枚举（A0 / A1 引脚）

INA226 从机地址由外部 **A0 / A1** 引脚连接决定。A0/A1 可接 `GND` / `VS` / `SDA` / `SCL`，共 16 种组合。库提供完整枚举 `e_INA226_I2CAddr`，**初始化时直接传枚举**（内部自动左移 1 位得到 HAL 使用的 8 位地址）。

| A1 | A0 | 7位二进制 | 7位地址 | 枚举值 |
|----|----|----------|---------|--------|
| GND | GND | 1000000 | 0x40 | `A0_GND_A1_GND` |
| GND | VS  | 1000001 | 0x41 | `A0_VS_A1_GND` |
| GND | SDA | 1000010 | 0x42 | `A0_SDA_A1_GND` |
| GND | SCL | 1000011 | 0x43 | `A0_SCL_A1_GND` |
| VS  | GND | 1000100 | 0x44 | `A0_GND_A1_VS` |
| VS  | VS  | 1000101 | 0x45 | `A0_VS_A1_VS` |
| VS  | SDA | 1000110 | 0x46 | `A0_SDA_A1_VS` |
| VS  | SCL | 1000111 | 0x47 | `A0_SCL_A1_VS` |
| SDA | GND | 1001000 | 0x48 | `A0_GND_A1_SDA` |
| SDA | VS  | 1001001 | 0x49 | `A0_VS_A1_SDA` |
| SDA | SDA | 1001010 | 0x4A | `A0_SDA_A1_SDA` |
| SDA | SCL | 1001011 | 0x4B | `A0_SCL_A1_SDA` |
| SCL | GND | 1001100 | 0x4C | `A0_GND_A1_SCL` |
| SCL | VS  | 1001101 | 0x4D | `A0_VS_A1_SCL` |
| SCL | SDA | 1001110 | 0x4E | `A0_SDA_A1_SCL` |
| SCL | SCL | 1001111 | 0x4F | `A0_SCL_A1_SCL` |

---

## 6. 接入示例（完整）

```cpp
#include "main.h"          // 或 stm32fxxx_hal.h
#include "INA226.hpp"
using namespace LoveFinderLib;

extern I2C_HandleTypeDef hi2c1;   // CubeMX 已初始化的 I2C 外设
INA226 ina;

void app_init(void)
{
    INA226_Config cfg = INA226_Config::getDefault();
    cfg.i2cAddr              = e_INA226_I2CAddr::A0_GND_A1_GND; // A0=A1=GND → 0x40
    cfg.shuntResistance_mOhm = 10.0f;                           // 检流电阻 10mΩ
    cfg.mode                 = e_INA226_Mode::SHUNT_BUS_CONT;   // 连续测量
    cfg.shuntConvTime        = e_INA226_ConvTime::CT_1100US;
    cfg.busConvTime          = e_INA226_ConvTime::CT_1100US;
    cfg.avgMode              = e_INA226_AvgMode::AVG_16;
    ina.init(&hi2c1, cfg);

    if (ina.isOnline())
    {
        ina.setCalibration(3.0f);   // 最大期望电流 3A
    }
}

void app_loop(void)
{
    float bus_mV  = ina.getBusVoltage();
    float cur_mA  = ina.getCurrent();
    float pwr_mW  = ina.getPower();
    // 驱动你的 UI / 显示
}
```

---

## 7. 常用方法

```cpp
INA226();
INA226(I2C_HandleTypeDef* i2c, const INA226_Config& config = INA226_Config::getDefault());

void  init(I2C_HandleTypeDef* i2c, const INA226_Config& config);
void  init(I2C_HandleTypeDef* i2c);                       // 默认配置

HAL_StatusTypeDef writeReg(e_INA226_Reg reg, uint16_t value);
uint16_t readReg(e_INA226_Reg reg) const;

void  config(e_INA226_Mode mode, e_INA226_ConvTime shuntCT,
             e_INA226_ConvTime busCT, e_INA226_AvgMode avg);
void  setCalibration(float maxCurrent_A);

float    getBusVoltage() const;     // mV
float    getShuntVoltage() const;   // uV (可负)
float    getCurrent() const;        // mA
float    getPower() const;          // mW
uint16_t getManufacturerID() const; // 应为 0x5449 ('TI')
uint16_t getDieID() const;          // 应为 0x2260 / 0x2261
uint8_t  getDieRevision() const;    // Die 修订版本 (低 4 位)
bool     isOnline() const;          // 在线检测
void     reset();                   // 软件复位

// ---- 告警系统 (MASK_ENABLE 0x06 / ALERT_LIMIT 0x07) ----
void     setMaskEnable(uint16_t mask);          // 告警使能 (e_INA226_AlertFlag 位掩码)
uint16_t getMaskEnable() const;                 // 读回告警使能
void     setAlertLimit(float value, e_INA226_AlertType type);  // 物理量阈值
float    getAlertLimit(e_INA226_AlertType type) const;         // 读回阈值 (物理量)
uint16_t getAlertFlags() const;                 // 告警源标志 + AFF/CVRF/OVF
void     setAlertLatch(bool enable);            // 告警锁存 (true=锁存)

// ---- ALERT 引脚绑定 (可选, 固定低有效/下降沿) ----
void bindAlertPin(GPIO_TypeDef* port, uint16_t pin);  // 绑定 ALERT 引脚
void unbindAlertPin();                                // 解除绑定
bool isAlertAsserted() const;                         // 读引脚: 低电平=有告警

// ---- 触发模式单次转换 ----
void triggerConversion();                       // 触发一次转换
bool isConversionReady() const;                 // 轮询转换完成 (CVRF)

// ---- 状态读回 ----
uint16_t getConfig() const;                     // 配置寄存器原始值
uint16_t getCalibration() const;                // 校准寄存器原始值

// 静态方法: 扫描 I2C 总线上所有 INA226 设备 (SCANINA226)
static uint8_t SCANINA226(I2C_HandleTypeDef* i2c,
                          e_INA226_I2CAddr* foundAddrs,
                          uint8_t maxFound);   // 返回找到的设备数量

float            getCurrentLSB() const;     // 校准后的电流 LSB (uA)
float            getShuntResistance() const; // 分流电阻 (mOhm)
e_INA226_I2CAddr getI2CAddr() const;         // 7 位从机地址
```

### SCANINA226 用法示例（先扫描，再按实际地址初始化）

```cpp
#include "main.h"
#include "INA226.hpp"
using namespace LoveFinderLib;

extern I2C_HandleTypeDef hi2c1;

void app_scan_demo(void)
{
    e_INA226_I2CAddr found[16];                     // 最多 16 个设备
    uint8_t n = INA226::SCANINA226(&hi2c1, found, 16);

    if (n == 0)
    {
        // 总线上没有 INA226: 检查接线 / 上拉电阻 / 地址
        return;
    }

    // found[0..n-1] 即总线上所有可用 INA226 的地址
    INA226_Config cfg = INA226_Config::getDefault();
    cfg.i2cAddr              = found[0];             // 用扫描到的真实地址
    cfg.shuntResistance_mOhm = 10.0f;
    INA226 ina;
    ina.init(&hi2c1, cfg);
    ina.setCalibration(3.0f);
}
```

---

## 8. 注意事项

- **必须**先用 CubeMX / HAL 初始化对应 I2C 外设；本库只负责读写。
- **地址务必匹配实际 A0/A1 接线**：地址由硬件引脚决定，`INA226_Config::getDefault()` 默认 `A0_GND_A1_GND`（0x40）。若接线不同，改 `cfg.i2cAddr` 枚举即可。
- **分流电阻**（`shuntResistance_mOhm`）与**最大期望电流**（`setCalibration` 参数）务必按实际硬件填写，否则电流/功率读数不准。
- 电流 / 功率读数依赖校准寄存器，**必须先 `setCalibration()` 再读取**，否则 `getCurrent()`/`getPower()` 返回 0。
- **告警引脚是开漏输出，必须外部上拉**：使用告警功能（`setMaskEnable`）时，ALERT 引脚必须外接上拉电阻（上拉到 VVS）。本库固定**低有效 + 下降沿**中断语义，不支持高有效（`setMaskEnable` 内部强制清零 APOL 位）。
- **ALERT 引脚绑定为可选项**：`INA226_Config` 的 `alertPort`/`alertPin` 默认 `nullptr`/`0`，`bindAlertPin()` 不调用即不绑定。**不绑定时告警功能依然完整可用**（`getAlertFlags()` 纯寄存器轮询，不依赖引脚）；`isAlertAsserted()` 仅在绑定后有效，未绑定恒返回 `false`。
- **同一时刻只能激活一个告警源**：若多个告警使能位同时置位，最高位（SOL 优先）的告警生效。
- **负阈值以补码写入**：分流欠压（`SHUNT_UNDER`）等负方向阈值由 `setAlertLimit` 自动转换；单个 ALERT_LIMIT 寄存器无法同时覆盖正负两个方向，电流换向须重写阈值。
- **告警比较基于单次转换值**：高 AVG 档位下告警可能被瞬时尖峰触发，需要精确告警时建议 `AVG_1`。
- **读 MASK_ENABLE 会清除 CVRF**：`isConversionReady()` 每次调用会清除转换完成标志，连续转换应在读到 `true` 后重新 `triggerConversion()`。
- **触发模式**：`config()` 将模式设为 `SHUNT_TRIG` / `BUS_TRIG` / `SHUNT_BUS_TRIG` 后，每次 `triggerConversion()` 触发一次单次转换，用 `isConversionReady()` 轮询完成。
- 配置 / 校准寄存器为**易失性**，上电后若改动过默认值需重新写入（可用 `getConfig()` 校验）。
- 本库为 **C++17**，请确保工程开启 C++（Keil MDK 中把源文件设为 `.cpp` 或启用 C++ 编译）。
- 只依赖 ST 标准 HAL 库；非 STM32 / 非 HAL 的外设库不支持。

---

## 9. 告警与触发模式示例

```cpp
#include "main.h"
#include "INA226.hpp"
using namespace LoveFinderLib;

extern I2C_HandleTypeDef hi2c1;
INA226 ina;

void app_init(void)
{
    INA226_Config cfg = INA226_Config::getDefault();
    cfg.i2cAddr              = e_INA226_I2CAddr::A0_GND_A1_GND;
    cfg.shuntResistance_mOhm = 10.0f;
    ina.init(&hi2c1, cfg);
    ina.setCalibration(3.0f);

    // (可选) 绑定 ALERT 引脚: 板子上无此引脚 / 不想用中断时, 跳过即可
    // ina.bindAlertPin(ALERT_GPIO_Port, ALERT_Pin);   // CubeMX 生成的宏

    // 告警: 分流过压 80mV (SOL), 锁存模式, 低有效 (默认)
    ina.setAlertLatch(true);                                    // 锁存, 读 0x06 后清除
    ina.setAlertLimit(80000.0f, e_INA226_AlertType::SHUNT_OVER); // 80mV = 80000uV
    ina.setMaskEnable(static_cast<uint16_t>(e_INA226_AlertFlag::SOL));

    // 触发模式单次转换 (告警精确场景建议 AVG_1)
    ina.config(e_INA226_Mode::SHUNT_BUS_TRIG, e_INA226_ConvTime::CT_1100US,
               e_INA226_ConvTime::CT_1100US, e_INA226_AvgMode::AVG_1);
}

void app_loop(void)
{
    // 单次转换: 触发 → 轮询完成 → 读数
    ina.triggerConversion();
    if (ina.isConversionReady())
    {
        float bus_mV = ina.getBusVoltage();
        float cur_mA = ina.getCurrent();
        // ...
    }

    // 告警状态检查 (读 0x06 同时清除锁存告警与 CVRF)
    uint16_t flags = ina.getAlertFlags();
    if (flags & static_cast<uint16_t>(e_INA226_AlertFlag::SOL))
    {
        // 分流过压告警触发
    }
    if (flags & static_cast<uint16_t>(e_INA226_AlertFlag::OVF))
    {
        // 运算溢出: 电流/功率读数无效
    }

    // 已绑定 ALERT 引脚时, 可零 I2C 开销读电平 (需外接上拉)
    // if (ina.isAlertAsserted()) { /* 有告警 */ }
}
```

### ALERT 引脚 + EXTI 下降沿中断用法（可选）

```cpp
// 1. CubeMX 中把 ALERT 引脚配置为 GPIO_EXTI (上拉输入), 下降沿触发
// 2. 初始化时绑定:
ina.bindAlertPin(ALERT_GPIO_Port, ALERT_Pin);   // CubeMX 生成的宏

// 3. 中断回调中读标志 (低有效: 告警时引脚拉低 → 下降沿)
volatile uint16_t g_alertFlags = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ALERT_Pin)
    {
        g_alertFlags = ina.getAlertFlags();   // 读+清除锁存告警
    }
}
```

> **注意**：ALERT 引脚为开漏输出，**必须外部上拉**（上拉到 VVS）。本库固定**低有效 + 下降沿**中断语义（`setMaskEnable` 内部强制清零 APOL 极性位），不支持高有效配置。

---

*LoveFinderLib :: INA226 — 2026*
