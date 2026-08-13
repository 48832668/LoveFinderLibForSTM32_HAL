/**
 * @file ENCODER.cpp
 * @brief Rotary Encoder Driver Implementation - C++17 (LoveFinderLib)
 *
 * 位置读取: 基于 16 位硬件计数器 + 带符号增量累加。
 *
 * 设计要点:
 *  - STM32 TIM 硬件编码器模式在硬件上完成全部边沿计数 (不占 CPU 轮询)。
 *  - 读取时把 16 位计数器按【有符号】解释 (0..65535 ↔ -32768..32767)，
 *    两次读取间的差值就是带符号增量，16 位回绕天然被有符号数学吸收，
 *    因此【无需更新中断、无竞态、不会死锁、边界不再跳变】。
 *  - 只要两次读取间隔内增量不超过 ±32767 脉冲 (人工转速远达不到)，累加绝对可靠。
 */

#include "ENCODER.hpp"

namespace LoveFinderLib {

/*============================================================================
 * 编码器参数表
 *============================================================================*/

namespace {

constexpr ENCODER_Params paramTable[] = {
    {"EC06",  2, "2 pulses/detent"},          // EC06
    {"EC11",  4, "4 pulses/detent (common)"}, // EC11
    {"EC12",  4, "4 pulses/detent"},          // EC12
    {"EC16",  4, "4 pulses/detent"},          // EC16
    {"EC20",  4, "4 pulses/detent"},          // EC20
    {"CUSTOM", 0, "User defined"}             // CUSTOM
};

} // namespace

/*============================================================================
 * Encoder 类实现
 *============================================================================*/

Encoder::Encoder(TIM_HandleTypeDef* tim, const ENCODER_Config& config)
{
    init(tim, config);
}

void Encoder::init(TIM_HandleTypeDef* tim, const ENCODER_Config& config)
{
    m_tim = tim;
    m_type = config.type;
    m_swapAB = config.swapAB;
    m_minValue = config.minValue;
    m_maxValue = config.maxValue;

    // 每格脉冲数: CUSTOM 用配置里的值，否则用型号预设
    m_countsPerDetent = (config.type == e_ENCODER_Type::CUSTOM)
                            ? config.countsPerDetent
                            : getParams(config.type).countsPerDetent;

    m_pos = 0;
    m_lastRaw = 0;
    m_lastPos = 0;
    m_flags = 0;
    m_direction = 0;
    m_lastActivityTick = 0;
}

void Encoder::init(TIM_HandleTypeDef* tim)
{
    init(tim, ENCODER_Config::getDefault());
}

void Encoder::setRange(int32_t minVal, int32_t maxVal)
{
    m_minValue = minVal;
    m_maxValue = maxVal;
}

void Encoder::start()
{
    if (m_tim == nullptr)
    {
        return;
    }

    // 软件位置清零，硬件计数器复位到 0
    m_pos = 0;
    m_lastPos = 0;
    m_flags = 0;
    m_direction = 0;
    m_lastActivityTick = 0;
    __HAL_TIM_SET_COUNTER(m_tim, 0);

    // 启动编码器 (TIM 硬件边沿计数，不占 CPU)
    HAL_TIM_Encoder_Start(m_tim, TIM_CHANNEL_ALL);

    // 建立读取基线 (带符号 16 位)
    m_lastRaw = static_cast<int16_t>(__HAL_TIM_GET_COUNTER(m_tim));
    m_lastPos = getCount();
}

void Encoder::stop()
{
    if (m_tim == nullptr)
    {
        return;
    }
    HAL_TIM_Encoder_Stop(m_tim, TIM_CHANNEL_ALL);
}

int32_t Encoder::getRawPosition() const
{
    if (m_tim == nullptr)
    {
        return 0;
    }

    // 按有符号读取 16 位硬件计数器 (0..65535 ↔ -32768..32767)
    int16_t raw = static_cast<int16_t>(__HAL_TIM_GET_COUNTER(m_tim));

    // 带符号增量 (16 位回绕被有符号数学自然吸收，无竞态)
    int32_t delta = static_cast<int32_t>(raw) - static_cast<int32_t>(m_lastRaw);
    if (delta > 32767)  delta -= 65536;    // 向下回绕修正
    if (delta < -32767) delta += 65536;    // 向上回绕修正

    m_lastRaw = raw;
    m_pos += delta;

    return m_swapAB ? -m_pos : m_pos;
}

int32_t Encoder::getCount() const
{
    int32_t count = getRawPosition() / static_cast<int32_t>(m_countsPerDetent);
    if (count < m_minValue) count = m_minValue;
    if (count > m_maxValue) count = m_maxValue;
    return count;
}

bool Encoder::changed()
{
    int32_t c = getCount();
    if (c != m_lastPos)
    {
        // 计算方向增量并置位事件标志位 (CHANGED + DIR_PLUS/DIR_MINUS)
        int32_t delta = c - m_lastPos;
        m_flags |= flagOf(delta);

        // 记录最近旋转方向与活动时刻 (供复杂 UI: 惯性/加速/超时)
        m_direction = (delta > 0) ? static_cast<int8_t>(1)
                    : (delta < 0) ? static_cast<int8_t>(-1)
                                  : static_cast<int8_t>(0);
        m_lastActivityTick = HAL_GetTick();

        m_lastPos = c;
        return true;
    }
    return false;
}

void Encoder::setCount(int32_t value)
{
    if (m_tim == nullptr)
    {
        return;
    }

    // 换算为原始脉冲位置并写入硬件计数器，重建基线
    int32_t raw = value * static_cast<int32_t>(m_countsPerDetent);
    m_pos = raw;
    __HAL_TIM_SET_COUNTER(m_tim, static_cast<uint32_t>(raw & 0xFFFFu));
    m_lastRaw = static_cast<int16_t>(raw & 0xFFFFu);
    m_lastPos = getCount();
    m_flags = 0;   // 程序性设值, 清除方向/变化标志 (非旋转事件)
}

void Encoder::addCount(int32_t delta)
{
    setCount(getCount() + delta);
}

void Encoder::invertCount()
{
    setCount(-getCount());
}

void Encoder::reset()
{
    setCount(m_minValue);
}

const ENCODER_Params& Encoder::getParams(e_ENCODER_Type type)
{
    uint8_t idx = static_cast<uint8_t>(type);
    if (idx < static_cast<uint8_t>(e_ENCODER_Type::COUNT))
    {
        return paramTable[idx];
    }
    return paramTable[static_cast<uint8_t>(e_ENCODER_Type::EC11)];  // 默认返回EC11
}

} // namespace LoveFinderLib
