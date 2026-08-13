/**
 * @file INA226.hpp
 * @brief INA226 电流/电压/功率监测芯片驱动库 (LoveFinderLib) - C++17
 *
 * 设计要点:
 *  - 基于 STM32 HAL I2C 接口 (HAL_I2C_Mem_Write/Read)。
 *  - 纯 C++17 接口，位于 LoveFinderLib 命名空间，与 BUTTON/ENCODER 库风格一致。
 *  - 采用统一的 INA226_Config 配置结构体 + getDefault() 工厂 (编译期宏默认值)。
 *  - I2C 从机地址由外部 A0/A1 引脚连接决定，提供完整的 16 种组合枚举
 *    (e_INA226_I2CAddr)，初始化时直接传入枚举即可，无需手算地址。
 *  - 支持测量: 总线电压 / 分流电压 / 电流 / 功率 / 制造商ID / DieID / 在线检测。
 *  - 提供校准 (setCalibration) 与工作模式配置 (config) / 软件复位 (reset)。
 *
 * @author LoveFinder
 * @date 2026
 */

#ifndef LOVE_FINDER_LIB_INA226_HPP
#define LOVE_FINDER_LIB_INA226_HPP

#include <cstdint>

/*============================================================================
 * 平台移植点 (PORT) —— 唯一需要修改的地方
 *
 * 本库只依赖通用 STM32 HAL I2C API (I2C_HandleTypeDef、HAL_I2C_Mem_Write/
 * HAL_I2C_Mem_Read、HAL_I2C_IsDeviceReady、HAL_Delay)，这些在所有 STM32 系列
 * 的 HAL 中均一致。移植到其他 STM32 系列时，只需把下面这一行 include 改成你
 * 所用器件的 HAL 头文件即可，其余代码不用动：
 *
 *   STM32F0/F1/F3/F4/F7 : stm32f0xx_hal.h / stm32f1xx_hal.h / stm32f3xx_hal.h
 *                         stm32f4xx_hal.h / stm32f7xx_hal.h
 *   STM32L0/L1/L4/L5    : stm32l0xx_hal.h / stm32l1xx_hal.h / stm32l4xx_hal.h / stm32l5xx_hal.h
 *   STM32G0/G4          : stm32g0xx_hal.h / stm32g4xx_hal.h
 *   STM32H5/H7          : stm32h5xx_hal.h / stm32h7xx_hal.h
 *   STM32WB/WL          : stm32wbxx_hal.h / stm32wlxx_hal.h
 *   STM32C0/U0/U5       : stm32c0xx_hal.h / stm32u0xx_hal.h / stm32u5xx_hal.h
 *
 * 若你的工程已用 CubeMX 生成 main.h，也可以改用 #include "main.h"。
 *============================================================================*/
#include "stm32f4xx_hal.h"   // <<<< 移植到其他 STM32 系列时，只改这一行

namespace LoveFinderLib {

/*============================================================================
 * 寄存器地址
 *============================================================================*/

enum class e_INA226_Reg : uint8_t {
    CONFIGURATION   = 0x00,   // 配置寄存器
    SHUNT_VOLTAGE   = 0x01,   // 分流电压
    BUS_VOLTAGE     = 0x02,   // 总线电压
    POWER           = 0x03,   // 功率
    CURRENT         = 0x04,   // 电流
    CALIBRATION     = 0x05,   // 校准
    MASK_ENABLE     = 0x06,   // 屏蔽/使能
    ALERT_LIMIT     = 0x07,   // 报警阈值
    MANUFACTURER_ID = 0xFE,   // 制造商ID
    DIE_ID          = 0xFF    // DieID
};

/*============================================================================
 * 转换时间枚举 (e_INA226_ConvTime)
 *============================================================================*/

enum class e_INA226_ConvTime : uint8_t {
    CT_140US  = 0x00,   // 140us
    CT_204US  = 0x01,   // 204us
    CT_332US  = 0x02,   // 332us
    CT_588US  = 0x03,   // 588us
    CT_1100US = 0x04,   // 1.1ms
    CT_2116US = 0x05,   // 2.116ms
    CT_4156US = 0x06,   // 4.156ms
    CT_8244US = 0x07    // 8.244ms
};

/*============================================================================
 * 平均模式枚举 (e_INA226_AvgMode)
 *============================================================================*/

enum class e_INA226_AvgMode : uint8_t {
    AVG_1    = 0x00,   // 1 次
    AVG_4    = 0x01,   // 4 次
    AVG_16   = 0x02,   // 16 次
    AVG_64   = 0x03,   // 64 次
    AVG_128  = 0x04,   // 128 次
    AVG_256  = 0x05,   // 256 次
    AVG_512  = 0x06,   // 512 次
    AVG_1024 = 0x07    // 1024 次
};

/*============================================================================
 * 工作模式枚举 (e_INA226_Mode)
 *============================================================================*/

enum class e_INA226_Mode : uint8_t {
    POWER_DOWN     = 0x00,   // 关机
    SHUNT_TRIG     = 0x01,   // 分流电压触发
    BUS_TRIG       = 0x02,   // 总线电压触发
    SHUNT_BUS_TRIG = 0x03,   // 分流+总线触发
    ADC_OFF        = 0x04,   // ADC关闭
    SHUNT_CONT     = 0x05,   // 分流连续
    BUS_CONT       = 0x06,   // 总线连续
    SHUNT_BUS_CONT = 0x07    // 分流+总线连续
};

/*============================================================================
 * I2C 从机地址枚举 (由外部 A0/A1 引脚连接决定)
 *
 * 16 种组合: A0/A1 可接 GND / VS / SDA / SCL。
 * 数值为 7 位从机地址 (0x40..0x4F)，init() 内部会左移 1 位得到 8 位 HAL 地址。
 *============================================================================*/

enum class e_INA226_I2CAddr : uint8_t {
    A0_GND_A1_GND = 0x40,   // A0=GND  A1=GND  → 1000000
    A0_VS_A1_GND  = 0x41,   // A0=VS   A1=GND  → 1000001
    A0_SDA_A1_GND = 0x42,   // A0=SDA  A1=GND  → 1000010
    A0_SCL_A1_GND = 0x43,   // A0=SCL  A1=GND  → 1000011
    A0_GND_A1_VS  = 0x44,   // A0=GND  A1=VS   → 1000100
    A0_VS_A1_VS   = 0x45,   // A0=VS   A1=VS   → 1000101
    A0_SDA_A1_VS  = 0x46,   // A0=SDA  A1=VS   → 1000110
    A0_SCL_A1_VS  = 0x47,   // A0=SCL  A1=VS   → 1000111
    A0_GND_A1_SDA = 0x48,   // A0=GND  A1=SDA  → 1001000
    A0_VS_A1_SDA  = 0x49,   // A0=VS   A1=SDA  → 1001001
    A0_SDA_A1_SDA = 0x4A,   // A0=SDA  A1=SDA  → 1001010
    A0_SCL_A1_SDA = 0x4B,   // A0=SCL  A1=SDA  → 1001011
    A0_GND_A1_SCL = 0x4C,   // A0=GND  A1=SCL  → 1001100
    A0_VS_A1_SCL  = 0x4D,   // A0=VS   A1=SCL  → 1001101
    A0_SDA_A1_SCL = 0x4E,   // A0=SDA  A1=SCL  → 1001110
    A0_SCL_A1_SCL = 0x4F    // A0=SCL  A1=SCL  → 1001111
};

/*============================================================================
 * 配置结构体 (INA226_Config)
 *============================================================================*/

struct INA226_Config {
    e_INA226_I2CAddr i2cAddr;       // I2C 从机地址 (由 A0/A1 引脚决定)
    float shuntResistance_mOhm;     // 分流电阻 (毫欧)
    e_INA226_Mode mode;             // 工作模式
    e_INA226_ConvTime shuntConvTime; // 分流转换时间
    e_INA226_ConvTime busConvTime;   // 总线转换时间
    e_INA226_AvgMode avgMode;       // 平均次数

    // 默认配置工厂函数
    static INA226_Config getDefault() {
        return {
            e_INA226_I2CAddr::A0_GND_A1_GND,   // 默认 A0=A1=GND (0x40)
            10.0f,                              // 10 毫欧检流电阻
            e_INA226_Mode::SHUNT_BUS_CONT,      // 分流+总线连续测量
            e_INA226_ConvTime::CT_1100US,       // 1.1ms 转换
            e_INA226_ConvTime::CT_1100US,       // 1.1ms 转换
            e_INA226_AvgMode::AVG_16            // 16 次平均
        };
    }
};

/*============================================================================
 * INA226 类
 *============================================================================*/

class INA226 {
public:
    /**
     * @brief 默认构造函数
     */
    INA226() = default;

    /**
     * @brief 构造并初始化
     * @param i2c I2C 外设句柄
     * @param config 配置参数 (可选，使用默认配置)
     */
    INA226(I2C_HandleTypeDef* i2c, const INA226_Config& config = INA226_Config::getDefault());

    /**
     * @brief 初始化
     * @param i2c I2C 外设句柄
     * @param config 配置参数
     */
    void init(I2C_HandleTypeDef* i2c, const INA226_Config& config);

    /**
     * @brief 初始化 (使用默认配置)
     * @param i2c I2C 外设句柄
     */
    void init(I2C_HandleTypeDef* i2c);

    /**
     * @brief 写 16 位寄存器
     * @param reg 寄存器地址
     * @param value 写入值
     * @return HAL 状态
     */
    HAL_StatusTypeDef writeReg(e_INA226_Reg reg, uint16_t value);

    /**
     * @brief 读 16 位寄存器
     * @param reg 寄存器地址
     * @return 寄存器值
     */
    uint16_t readReg(e_INA226_Reg reg) const;

    /**
     * @brief 配置工作模式 / 转换时间 / 平均次数
     * @param mode 工作模式
     * @param shuntCT 分流转换时间
     * @param busCT 总线转换时间
     * @param avg 平均次数
     */
    void config(e_INA226_Mode mode, e_INA226_ConvTime shuntCT,
                e_INA226_ConvTime busCT, e_INA226_AvgMode avg);

    /**
     * @brief 设置校准寄存器
     * @param maxCurrent_A 最大期望电流 (安培)
     */
    void setCalibration(float maxCurrent_A);

    /**
     * @brief 获取总线电压
     * @return 电压 (mV)
     */
    float getBusVoltage() const;

    /**
     * @brief 获取分流电压
     * @return 电压 (uV)，可为负值
     */
    float getShuntVoltage() const;

    /**
     * @brief 获取电流
     * @return 电流 (mA)
     */
    float getCurrent() const;

    /**
     * @brief 获取功率
     * @return 功率 (mW)
     */
    float getPower() const;

    /**
     * @brief 获取制造商 ID
     * @return 应返回 0x5449 ('TI')
     */
    uint16_t getManufacturerID() const;

    /**
     * @brief 获取 Die ID
     * @return 应返回 0x2260
     */
    uint16_t getDieID() const;

    /**
     * @brief 检测设备是否在线
     * @return true=在线, false=离线
     */
    bool isOnline() const;

    /**
     * @brief 软件复位
     */
    void reset();

    /**
     * @brief 扫描 I2C 总线上所有 INA226 设备 (SCANINA226)
     *
     * 遍历 e_INA226_I2CAddr 地址表 (0x40~0x4F 共 16 个地址)，对每个地址:
     *   1. HAL_I2C_IsDeviceReady 通讯探测;
     *   2. 读取制造商 ID 寄存器 (0xFE)，校验是否为 0x5449 ('TI')。
     * 通过校验的地址即为总线上实际存在的 INA226 设备。
     * 静态方法，无需先创建实例。
     *
     * @param i2c I2C 外设句柄
     * @param foundAddrs 输出数组: 存放扫描到的设备地址 (容量至少 maxFound)
     * @param maxFound 输出数组容量 (最多 16)
     * @return 实际找到的设备数量 (0=未发现)
     */
    static uint8_t SCANINA226(I2C_HandleTypeDef* i2c,
                              e_INA226_I2CAddr* foundAddrs,
                              uint8_t maxFound);

    /**
     * @brief 获取当前电流 LSB (校准后)
     * @return 电流 LSB (uA)
     */
    float getCurrentLSB() const { return m_currentLSB_uA; }

    /**
     * @brief 获取分流电阻值
     * @return 分流电阻 (mOhm)
     */
    float getShuntResistance() const { return m_shuntResistance_mOhm; }

    /**
     * @brief 获取 I2C 从机地址 (7 位)
     * @return 从机地址
     */
    e_INA226_I2CAddr getI2CAddr() const { return m_addr; }

private:
    I2C_HandleTypeDef* m_i2c = nullptr;      // I2C 外设句柄
    e_INA226_I2CAddr m_addr = e_INA226_I2CAddr::A0_GND_A1_GND;  // 7 位从机地址
    float m_shuntResistance_mOhm = 10.0f;   // 检流电阻 (毫欧)
    float m_currentLSB_uA = 0.0f;           // 电流 LSB (uA, 校准后)
};

} // namespace LoveFinderLib

#endif // LOVE_FINDER_LIB_INA226_HPP
