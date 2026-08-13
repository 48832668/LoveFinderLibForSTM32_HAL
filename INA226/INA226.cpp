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
