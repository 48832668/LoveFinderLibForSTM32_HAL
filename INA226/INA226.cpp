/**
 * @file INA226.cpp
 * @brief INA226 Current/Power Monitor Driver Implementation - C++17 (LoveFinderLib)
 */

#include "INA226.hpp"

namespace LoveFinderLib {

/*============================================================================
 * 物理量 LSB (constexpr)
 *============================================================================*/

namespace {

constexpr float SHUNT_VOLTAGE_LSB_uV = 2.5f;    // 分流电压 LSB: 2.5uV
constexpr float BUS_VOLTAGE_LSB_mV   = 1.25f;   // 总线电压 LSB: 1.25mV

} // namespace

/*============================================================================
 * INA226 类实现
 *============================================================================*/

INA226::INA226(I2C_HandleTypeDef* i2c, const INA226_Config& config)
{
    init(i2c, config);
}

void INA226::init(I2C_HandleTypeDef* i2c, const INA226_Config& config)
{
    m_i2c = i2c;
    m_addr = config.i2cAddr;
    m_shuntResistance_mOhm = config.shuntResistance_mOhm;
    m_currentLSB_uA = 0.0f;

    // 按配置写工作模式 / 转换时间 / 平均次数
    // 注意: 参数名 config 会遮蔽同名成员函数, 需用 this-> 显式调用
    this->config(config.mode, config.shuntConvTime, config.busConvTime, config.avgMode);
}

void INA226::init(I2C_HandleTypeDef* i2c)
{
    init(i2c, INA226_Config::getDefault());
}

HAL_StatusTypeDef INA226::writeReg(e_INA226_Reg reg, uint16_t value)
{
    if (m_i2c == nullptr)
    {
        return HAL_ERROR;
    }

    uint8_t data[2];
    data[0] = static_cast<uint8_t>((value >> 8) & 0xFFu);   // 高字节在前
    data[1] = static_cast<uint8_t>(value & 0xFFu);

    // HAL I2C 需要 8 位地址 (7 位地址左移 1 位)
    return HAL_I2C_Mem_Write(m_i2c, static_cast<uint16_t>(static_cast<uint8_t>(m_addr) << 1),
                             static_cast<uint16_t>(reg), I2C_MEMADD_SIZE_8BIT, data, 2, 0xFF);
}

uint16_t INA226::readReg(e_INA226_Reg reg) const
{
    if (m_i2c == nullptr)
    {
        return 0;
    }

    uint8_t data[2] = {0, 0};
    HAL_I2C_Mem_Read(m_i2c, static_cast<uint16_t>(static_cast<uint8_t>(m_addr) << 1),
                     static_cast<uint16_t>(reg), I2C_MEMADD_SIZE_8BIT, data, 2, 0xFF);

    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

void INA226::config(e_INA226_Mode mode, e_INA226_ConvTime shuntCT,
                    e_INA226_ConvTime busCT, e_INA226_AvgMode avg)
{
    // 配置寄存器布局: [15:12] Reserved | [11:9] AVG | [8:6] VBUS CT | [5:3] VSH CT | [2:0] Mode
    uint16_t cfg = (static_cast<uint16_t>(avg) << 9)
                 | (static_cast<uint16_t>(busCT) << 6)
                 | (static_cast<uint16_t>(shuntCT) << 3)
                 | static_cast<uint16_t>(mode);

    m_configValue = cfg;    // 记录最近配置 (triggerConversion 重写用)
    writeReg(e_INA226_Reg::CONFIGURATION, cfg);
}

void INA226::setCalibration(float maxCurrent_A)
{
    // 电流 LSB = MaxExpectedCurrent / 32768 (单位: A)
    float currentLSB_A = maxCurrent_A / 32768.0f;

    // 分流电阻 (单位: Ω)
    float shuntRes_Ohm = m_shuntResistance_mOhm / 1000.0f;

    // 校准寄存器: Cal = 0.00512 / (Current_LSB * R_shunt)
    float cal = 0.00512f / (currentLSB_A * shuntRes_Ohm);
    uint16_t calValue = static_cast<uint16_t>(cal);

    // 按实际写入的校准值反推精确 Current_LSB (uA)，保证读数精度
    m_currentLSB_uA = (0.00512f / (static_cast<float>(calValue) * shuntRes_Ohm)) * 1000000.0f;

    writeReg(e_INA226_Reg::CALIBRATION, calValue);
}

float INA226::getBusVoltage() const
{
    // 总线电压 LSB = 1.25mV，寄存器为有符号数
    int16_t raw = static_cast<int16_t>(readReg(e_INA226_Reg::BUS_VOLTAGE));
    return static_cast<float>(raw) * BUS_VOLTAGE_LSB_mV;
}

float INA226::getShuntVoltage() const
{
    // 分流电压 LSB = 2.5uV，可为负值
    int16_t raw = static_cast<int16_t>(readReg(e_INA226_Reg::SHUNT_VOLTAGE));
    return static_cast<float>(raw) * SHUNT_VOLTAGE_LSB_uV;
}

float INA226::getCurrent() const
{
    // 电流 = 寄存器值 * Current_LSB (uA) / 1000 → mA
    int16_t raw = static_cast<int16_t>(readReg(e_INA226_Reg::CURRENT));
    return static_cast<float>(raw) * m_currentLSB_uA / 1000.0f;
}

float INA226::getPower() const
{
    // 功率 = 寄存器值 * 25 * Current_LSB (uA) / 1000000 → mW
    uint16_t raw = readReg(e_INA226_Reg::POWER);
    return static_cast<float>(raw) * 25.0f * m_currentLSB_uA / 1000000.0f;
}

uint16_t INA226::getManufacturerID() const
{
    return readReg(e_INA226_Reg::MANUFACTURER_ID);
}

uint16_t INA226::getDieID() const
{
    return readReg(e_INA226_Reg::DIE_ID);
}

bool INA226::isOnline() const
{
    // 通过读取制造商 ID 判断设备是否在线 (TI 固定返回 0x5449)
    return getManufacturerID() == 0x5449u;
}

void INA226::reset()
{
    // 写复位命令到配置寄存器 (bit15)
    writeReg(e_INA226_Reg::CONFIGURATION, 0x8000u);
    HAL_Delay(10);
}

/*------------------------------------------------------------------------
 * 告警系统
 *-----------------------------------------------------------------------*/

void INA226::setMaskEnable(uint16_t mask)
{
    writeReg(e_INA226_Reg::MASK_ENABLE, mask);
}

uint16_t INA226::getMaskEnable() const
{
    return readReg(e_INA226_Reg::MASK_ENABLE);
}

void INA226::setAlertLimit(float value, e_INA226_AlertType type)
{
    uint16_t limit = 0;

    switch (type)
    {
    case e_INA226_AlertType::SHUNT_OVER:
    case e_INA226_AlertType::SHUNT_UNDER:
        // 分流电压阈值: LSB = 2.5uV, 负值自动转为补码
        limit = static_cast<uint16_t>(static_cast<int16_t>(value / SHUNT_VOLTAGE_LSB_uV));
        break;

    case e_INA226_AlertType::BUS_OVER:
    case e_INA226_AlertType::BUS_UNDER:
        // 总线电压阈值: LSB = 1.25mV
        limit = static_cast<uint16_t>(value / BUS_VOLTAGE_LSB_mV);
        break;

    case e_INA226_AlertType::POWER_OVER:
        // 功率阈值: LSB = 25 x Current_LSB (uA) / 1000 → mW
        limit = static_cast<uint16_t>(value / (25.0f * m_currentLSB_uA / 1000.0f));
        break;
    }

    writeReg(e_INA226_Reg::ALERT_LIMIT, limit);
}

float INA226::getAlertLimit(e_INA226_AlertType type) const
{
    uint16_t raw = readReg(e_INA226_Reg::ALERT_LIMIT);

    switch (type)
    {
    case e_INA226_AlertType::SHUNT_OVER:
    case e_INA226_AlertType::SHUNT_UNDER:
        // 按有符号解释 (负阈值以补码存储)
        return static_cast<float>(static_cast<int16_t>(raw)) * SHUNT_VOLTAGE_LSB_uV;

    case e_INA226_AlertType::BUS_OVER:
    case e_INA226_AlertType::BUS_UNDER:
        return static_cast<float>(raw) * BUS_VOLTAGE_LSB_mV;

    case e_INA226_AlertType::POWER_OVER:
        return static_cast<float>(raw) * (25.0f * m_currentLSB_uA / 1000.0f);
    }

    return 0.0f;
}

uint16_t INA226::getAlertFlags() const
{
    // 读 0x06: 高 6 位 (D15~D10) 为告警源标志, D4/D3/D2 为 AFF/CVRF/OVF
    return readReg(e_INA226_Reg::MASK_ENABLE);
}

void INA226::setAlertPolarity(bool activeHigh)
{
    uint16_t mask = readReg(e_INA226_Reg::MASK_ENABLE);

    if (activeHigh)
    {
        mask |= static_cast<uint16_t>(e_INA226_AlertFlag::APOL);
    }
    else
    {
        mask &= ~static_cast<uint16_t>(e_INA226_AlertFlag::APOL);
    }

    writeReg(e_INA226_Reg::MASK_ENABLE, mask);
}

void INA226::setAlertLatch(bool enable)
{
    uint16_t mask = readReg(e_INA226_Reg::MASK_ENABLE);

    if (enable)
    {
        mask |= static_cast<uint16_t>(e_INA226_AlertFlag::LEN);
    }
    else
    {
        mask &= ~static_cast<uint16_t>(e_INA226_AlertFlag::LEN);
    }

    writeReg(e_INA226_Reg::MASK_ENABLE, mask);
}

/*------------------------------------------------------------------------
 * 转换控制
 *-----------------------------------------------------------------------*/

void INA226::triggerConversion()
{
    // 触发模式: 再次写入配置寄存器 (即使值不变) 即触发下一次转换
    writeReg(e_INA226_Reg::CONFIGURATION, m_configValue);
}

bool INA226::isConversionReady() const
{
    // 轮询 CVRF (D3): 转换+平均+乘法完成后置位
    // 注意: 读 0x06 会清除 CVRF, 应在读到 true 后重新 triggerConversion()
    return (readReg(e_INA226_Reg::MASK_ENABLE) & static_cast<uint16_t>(e_INA226_AlertFlag::CVRF)) != 0;
}

/*------------------------------------------------------------------------
 * 状态读回
 *-----------------------------------------------------------------------*/

uint16_t INA226::getConfig() const
{
    return readReg(e_INA226_Reg::CONFIGURATION);
}

uint16_t INA226::getCalibration() const
{
    return readReg(e_INA226_Reg::CALIBRATION);
}

uint8_t INA226::getDieRevision() const
{
    // Die ID 低 4 位为修订版本 (RID3~RID0)
    return static_cast<uint8_t>(readReg(e_INA226_Reg::DIE_ID) & 0x0Fu);
}

uint8_t INA226::SCANINA226(I2C_HandleTypeDef* i2c,
                           e_INA226_I2CAddr* foundAddrs,
                           uint8_t maxFound)
{
    if (i2c == nullptr || foundAddrs == nullptr || maxFound == 0)
    {
        return 0;
    }

    uint8_t count = 0;

    // 遍历地址表全部 16 个地址 (0x40 ~ 0x4F)
    for (uint8_t addr = 0x40u; addr <= 0x4Fu && count < maxFound; addr++)
    {
        // HAL I2C 需要 8 位地址 (7 位地址左移 1 位)
        uint16_t halAddr = static_cast<uint16_t>(addr) << 1;

        // 1. 通讯探测: 设备就绪
        if (HAL_I2C_IsDeviceReady(i2c, halAddr, 2, 10) != HAL_OK)
        {
            continue;
        }

        // 2. 校验设备: 读制造商 ID 寄存器 (0xFE)，INA226 固定返回 0x5449 ('TI')
        uint8_t data[2] = {0, 0};
        if (HAL_I2C_Mem_Read(i2c, halAddr,
                             static_cast<uint16_t>(e_INA226_Reg::MANUFACTURER_ID),
                             I2C_MEMADD_SIZE_8BIT, data, 2, 10) != HAL_OK)
        {
            continue;
        }

        uint16_t mfrId = static_cast<uint16_t>(
            (static_cast<uint16_t>(data[0]) << 8) | data[1]);
        if (mfrId != 0x5449u)
        {
            continue;   // 地址上有设备，但不是 INA226
        }

        // 校验通过: 记录该地址
        foundAddrs[count] = static_cast<e_INA226_I2CAddr>(addr);
        count++;
    }

    return count;
}

} // namespace LoveFinderLib
