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
 * 告警类型枚举 (e_INA226_AlertType) —— setAlertLimit 物理量换算依据
 *
 * 对应 Mask/Enable 寄存器 (0x06) 的告警使能位, 决定 ALERT_LIMIT (0x07)
 * 阈值寄存器与哪个测量值比较, 以及阈值的 LSB 缩放:
 *  - 分流电压 (SOL/SUL): LSB = 2.5uV, 可为负值 (二进制补码)
 *  - 总线电压 (BOL/BUL): LSB = 1.25mV
 *  - 功率     (POL):     LSB = 25 x Current_LSB
 *============================================================================*/

enum class e_INA226_AlertType : uint8_t {
    SHUNT_OVER  = 0,   // 分流电压过压告警 (SOL), 阈值单位 uV
    SHUNT_UNDER = 1,   // 分流电压欠压告警 (SUL), 阈值单位 uV (可负)
    BUS_OVER    = 2,   // 总线电压过压告警 (BOL), 阈值单位 mV
    BUS_UNDER   = 3,   // 总线电压欠压告警 (BUL), 阈值单位 mV
    POWER_OVER  = 4    // 功率过限告警     (POL), 阈值单位 mW
};

/*============================================================================
 * 告警位定义 (e_INA226_AlertFlag) —— Mask/Enable 寄存器 (0x06) 各位
 *
 * D15~D10: 告警使能位, 读取时同时反映告警源状态 (透明模式下随故障清除复位);
 * D4~D2:   只读状态标志位; D1/D0: 告警引脚行为配置位。
 *============================================================================*/

enum class e_INA226_AlertFlag : uint16_t {
    SOL  = 0x8000,   // D15 分流电压过压告警 使能/标志
    SUL  = 0x4000,   // D14 分流电压欠压告警 使能/标志
    BOL  = 0x2000,   // D13 总线电压过压告警 使能/标志
    BUL  = 0x1000,   // D12 总线电压欠压告警 使能/标志
    POL  = 0x0800,   // D11 功率过限告警     使能/标志
    CNVR = 0x0400,   // D10 转换完成告警 (ALERT 引脚输出 CVRF)
    AFF  = 0x0010,   // D4  告警功能标志 (只读)
    CVRF = 0x0008,   // D3  转换完成标志 (只读)
    OVF  = 0x0004,   // D2  运算溢出标志 (只读, 电流/功率可能无效)
    APOL = 0x0002,   // D1  告警极性: 0=低有效(默认) 1=高有效
    LEN  = 0x0001    // D0  告警锁存: 0=透明(默认) 1=锁存
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
     * @return 应返回 0x2260 (USA/Japan) 或 0x2261 (USA)
     */
    uint16_t getDieID() const;

    /**
     * @brief 获取 Die 修订版本号 (Die ID 低 4 位 RID3~RID0)
     * @return 修订版本 (0~15)
     */
    uint8_t getDieRevision() const;

    /**
     * @brief 检测设备是否在线
     * @return true=在线, false=离线
     */
    bool isOnline() const;

    /**
     * @brief 软件复位
     */
    void reset();

    /*------------------------------------------------------------------------
     * 告警系统 (MASK_ENABLE 0x06 / ALERT_LIMIT 0x07)
     *-----------------------------------------------------------------------*/

    /**
     * @brief 设置告警使能寄存器 (Mask/Enable 0x06)
     *
     * 仅能同时激活一个告警源 (若多个使能, 最高位优先); ALERT 引脚为开漏,
     * 必须外接上拉电阻。使用 e_INA226_AlertFlag 位掩码:
     *   setMaskEnable(e_INA226_AlertFlag::SOL);
     * 该操作也会写入 APOL/LEN 配置位。
     *
     * @param mask 位掩码 (e_INA226_AlertFlag)
     */
    void setMaskEnable(uint16_t mask);

    /**
     * @brief 读取告警使能寄存器 (Mask/Enable 0x06)
     * @return 当前 Mask/Enable 值 (含 APOL/LEN 配置位)
     */
    uint16_t getMaskEnable() const;

    /**
     * @brief 设置告警阈值寄存器 (Alert Limit 0x07)
     *
     * 阈值按告警类型自动换算为寄存器 LSB:
     *  - SHUNT_OVER/SHUNT_UNDER: value 单位 uV, LSB 2.5uV, 负值自动转补码
     *  - BUS_OVER/BUS_UNDER:     value 单位 mV, LSB 1.25mV
     *  - POWER_OVER:             value 单位 mW, LSB 25 x Current_LSB
     * 注意: 单个阈值寄存器无法同时覆盖正负两个电流方向, 换向须重写。
     *
     * @param value 阈值物理量 (uV / mV / mW)
     * @param type 告警类型 (e_INA226_AlertType)
     */
    void setAlertLimit(float value, e_INA226_AlertType type);

    /**
     * @brief 读取告警阈值寄存器 (Alert Limit 0x07) 并换算回物理量
     * @param type 告警类型 (e_INA226_AlertType)
     * @return 阈值物理量 (uV / mV / mW)
     */
    float getAlertLimit(e_INA226_AlertType type) const;

    /**
     * @brief 读取告警标志 (Mask/Enable 0x06 读回)
     *
     * 返回 D15~D10 (告警源标志) 与 D4~D2 (AFF/CVRF/OVF 状态位)。
     * 注意: 读操作会清除 CVRF (锁存模式下连同 AFF 一并清除)。
     *
     * @return 告警标志位 (e_INA226_AlertFlag 位掩码)
     */
    uint16_t getAlertFlags() const;

    /**
     * @brief 设置告警引脚极性 (APOL, D1)
     * @param activeHigh true=高有效, false=低有效 (默认)
     */
    void setAlertPolarity(bool activeHigh);

    /**
     * @brief 设置告警锁存模式 (LEN, D0)
     *
     * true=锁存: 告警后保持激活, 直到读 Mask/Enable 或写 Configuration 才清除;
     * false=透明: 故障消失后自动复位 (默认)。
     *
     * @param enable true=锁存, false=透明
     */
    void setAlertLatch(bool enable);

    /*------------------------------------------------------------------------
     * 转换控制 (触发模式)
     *-----------------------------------------------------------------------*/

    /**
     * @brief 触发一次单次转换
     *
     * 需先将工作模式配置为触发模式 (SHUNT_TRIG/BUS_TRIG/SHUNT_BUS_TRIG),
     * 每次调用写入配置寄存器 (即使值不变) 即触发下一次转换。
     * 转换完成后可用 isConversionReady() 轮询。
     */
    void triggerConversion();

    /**
     * @brief 查询转换是否完成 (CVRF, D3)
     *
     * 转换+平均+乘法全部完成后置位。注意: 每次读取会清除 CVRF,
     * 因此连续轮询时应在读到 true 后重新 triggerConversion()。
     *
     * @return true=转换完成
     */
    bool isConversionReady() const;

    /*------------------------------------------------------------------------
     * 状态读回
     *-----------------------------------------------------------------------*/

    /**
     * @brief 读取配置寄存器 (0x00) 原始值
     *
     * 寄存器为易失性, 上电后若更改过默认值必须重新写入 (手册 6.5.4)。
     *
     * @return 配置寄存器值
     */
    uint16_t getConfig() const;

    /**
     * @brief 读取校准寄存器 (0x05) 原始值
     * @return 校准寄存器值
     */
    uint16_t getCalibration() const;

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
    uint16_t m_configValue = 0;             // 最近写入的配置寄存器值 (触发转换用)
};

} // namespace LoveFinderLib

#endif // LOVE_FINDER_LIB_INA226_HPP
