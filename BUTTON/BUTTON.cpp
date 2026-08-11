/**
 * @file BUTTON.cpp
 * @brief EXTI-Driven Button Driver Implementation - C++17
 */

#include "BUTTON.hpp"

namespace LoveFinderLib {

/*============================================================================
 * 内部工具: 从 GPIO_PIN_x 掩码计算引脚编号 (0..15)
 *============================================================================*/
namespace {

inline uint8_t pinNumber(uint16_t pinMask)
{
    uint8_t n = 0;
    uint16_t m = pinMask;
    while ((m & 0x0001u) == 0u)
    {
        m >>= 1u;
        n++;
    }
    return n;
}

} // namespace

/*============================================================================
 * Button 类实现
 *============================================================================*/

Button::Button(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config)
{
    init(port, pin, config);
}

void Button::init(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config)
{
    m_port = port;
    m_pin  = pin;
    m_config = config;

    m_state = e_BUTTON_State::IDLE;
    m_pressPending = false;
    m_pressTick = 0;
    m_debounceTick = 0;
    m_pressTime = 0;
    m_waitingSecondClick = false;
    m_longPressFired = false;
    m_longPressActive = false;

    m_chargePercent = 0;
    m_chargeTick = 0;
    m_decayTick = 0;

    m_counter = {0, 0, 0};
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
    m_flags = 0;

    // 硬性要求: KEY 必须启用 EXTI，并配置为下降沿触发 + 启用 NVIC。
    configureExti();
}

void Button::init(GPIO_TypeDef* port, uint16_t pin)
{
    init(port, pin, BUTTON_Config::getDefault());
}

void Button::configureExti()
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = m_pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;   // 下降沿触发 (activeLow 按键按下)
    gpio.Pull = GPIO_PULLUP;            // 外部上拉已满足；内部上拉兜底
    HAL_GPIO_Init(m_port, &gpio);

    // 映射 EXTI line -> NVIC IRQ 并启用 (关键: CubeMX 可能未启用 EXTI NVIC)
    //
    // 不同 STM32 系列的 EXTI 中断号命名不一致，这里通过预处理按系列自动选择：
    //   - 分组式命名 (G0/C0):        EXTI0_1_IRQn / EXTI2_3_IRQn / EXTI4_15_IRQn
    //   - H5 式命名 (H5):            EXTI0_IRQn / EXTI1_IRQn / EXTI2_3_IRQn / EXTI4_15_IRQn
    //   - 单线式命名 (F0/F1/F3/F4/F7, L4/L5, U5, WB/WL, G4): EXTI0_IRQn..EXTI4_IRQn
    //                                + EXTI9_5_IRQn + EXTI15_10_IRQn
    uint8_t num = pinNumber(m_pin);
    IRQn_Type irq;
#if defined(EXTI0_1_IRQn)
    // G0 / C0: 引脚按 (0-1) / (2-3) / (4-15) 分组
    if (num <= 1)
    {
        irq = EXTI0_1_IRQn;
    }
    else if (num <= 3)
    {
        irq = EXTI2_3_IRQn;
    }
    else
    {
        irq = EXTI4_15_IRQn;
    }
#elif defined(EXTI2_3_IRQn) && defined(EXTI0_IRQn)
    // H5: 0 / 1 / (2-3) / (4-15)
    if (num == 0)
    {
        irq = EXTI0_IRQn;
    }
    else if (num == 1)
    {
        irq = EXTI1_IRQn;
    }
    else if (num <= 3)
    {
        irq = EXTI2_3_IRQn;
    }
    else
    {
        irq = EXTI4_15_IRQn;
    }
#else
    // F0/F1/F3/F4/F7, L4/L5, U5, WB/WL, G4: 0-4 单线 + EXTI9_5 + EXTI15_10
    if (num <= 4)
    {
        irq = static_cast<IRQn_Type>(EXTI0_IRQn + num);
    }
    else if (num <= 9)
    {
        irq = EXTI9_5_IRQn;
    }
    else
    {
        irq = EXTI15_10_IRQn;
    }
#endif

    HAL_NVIC_SetPriority(irq, 2, 0);
    HAL_NVIC_EnableIRQ(irq);
}

void Button::onExti()
{
    // 仅在下降沿记录按下。重复/抖动边缘在此被过滤。
    if (m_pressPending)
    {
        return;
    }
    m_pressPending = true;
    m_pressTick = HAL_GetTick();
}

bool Button::isPressed() const
{
    GPIO_PinState s = HAL_GPIO_ReadPin(m_port, m_pin);
    return m_config.activeLow ? (s == GPIO_PIN_RESET) : (s == GPIO_PIN_SET);
}

void Button::pushEvent(e_BUTTON_Event e)
{
    m_lastEvent = e;
    m_hasEvent = true;
    m_flags |= flagOf(e);   // 累积事件标志位，供 UI 非破坏/读取清除
}

e_BUTTON_Event Button::update()
{
    uint32_t now = HAL_GetTick();
    e_BUTTON_Event event = e_BUTTON_Event::NONE;

    switch (m_state)
    {
        case e_BUTTON_State::IDLE:
            // EXTI 下降沿中断已置位按下
            if (m_pressPending)
            {
                m_pressPending = false;
                m_state = e_BUTTON_State::DEBOUNCE;
                m_debounceTick = now;
            }
            break;

        case e_BUTTON_State::DEBOUNCE:
            if ((now - m_debounceTick) >= m_config.debounceMs)
            {
                m_pressPending = false;   // 丢弃消抖期间的多余边缘
                if (isPressed())
                {
                    m_state = e_BUTTON_State::PRESSED;
                    m_pressTime = now;
                    m_longPressFired = false;
                    m_longPressActive = false;
                    event = e_BUTTON_Event::PRESS_DOWN;
                    pushEvent(event);
                }
                else
                {
                    // 抖动，未真正按下
                    m_state = e_BUTTON_State::IDLE;
                }
            }
            break;

        case e_BUTTON_State::PRESSED:
            if (!isPressed())
            {
                // ===== 释放 =====
                m_pressPending = false;      // 丢弃释放瞬间可能残留的边缘，避免误判双击
                m_longPressActive = false;   // 停止充能
                m_decayTick = now;           // 未填满则开始消退

                if ((now - m_pressTime) >= m_config.longPressMs)
                {
                    // 长按释放 (事件已在阈值处触发)
                    if (!m_longPressFired)
                    {
                        m_longPressFired = true;
                        event = e_BUTTON_Event::LONG_PRESS;
                        pushEvent(event);
                    }
                    m_state = e_BUTTON_State::IDLE;
                }
                else
                {
                    // 短按释放
                    if (m_waitingSecondClick)
                    {
                        // 第二次点击 → 双击
                        m_waitingSecondClick = false;
                        m_counter.doubleClickCount++;
                        event = e_BUTTON_Event::DOUBLE_CLICK;
                        pushEvent(event);
                        m_state = e_BUTTON_State::IDLE;
                    }
                    else
                    {
                        // 第一次点击，进入等待第二次点击窗口
                        m_state = e_BUTTON_State::WAIT_CLICK;
                        m_debounceTick = now;
                    }
                }
            }
            else
            {
                // ===== 仍按住 =====
                if ((now - m_pressTime) >= m_config.longPressMs)
                {
                    if (!m_longPressFired)
                    {
                        m_longPressFired = true;
                        m_longPressActive = true;   // 进入长按充能
                        m_chargeTick = now;         // 从阈值处开始充能
                        event = e_BUTTON_Event::LONG_PRESS;
                        pushEvent(event);
                    }
                }
            }
            break;

        case e_BUTTON_State::WAIT_CLICK:
            if (m_pressPending)
            {
                // 第二次按下
                m_pressPending = false;
                m_waitingSecondClick = true;
                m_state = e_BUTTON_State::DEBOUNCE;
                m_debounceTick = now;
            }
            else if ((now - m_debounceTick) >= m_config.doubleClickMs)
            {
                // 超时 → 确认单击
                m_counter.clickCount++;
                event = e_BUTTON_Event::CLICK;
                pushEvent(event);
                m_state = e_BUTTON_State::IDLE;
            }
            else if (isPressed())
            {
                // 兜底: 未触发 EXTI 也检测到按下
                m_waitingSecondClick = true;
                m_state = e_BUTTON_State::DEBOUNCE;
                m_debounceTick = now;
            }
            break;

        default:
            m_state = e_BUTTON_State::IDLE;
            break;
    }

    // 充能 / 消退更新 (始终调用)
    updateCharge(now);

    return event;
}

void Button::updateCharge(uint32_t now)
{
    if (m_longPressActive)
    {
        // ===== 长按充能 =====
        uint32_t dt = now - m_chargeTick;
        if (dt >= 1)
        {
            uint32_t add = static_cast<uint32_t>(static_cast<uint64_t>(dt) * 100u / m_config.chargeFullMs);
            if (add >= 1)
            {
                m_chargeTick = now;
                uint32_t level = static_cast<uint32_t>(m_chargePercent) + add;
                if (level >= 100)
                {
                    // 填满一次 → 长按次数 +1，剩余部分继续充能 (持续充能)
                    uint32_t fills = level / 100u;
                    m_counter.longPressCount += fills;
                    m_chargePercent = static_cast<uint16_t>(level % 100u);
                }
                else
                {
                    m_chargePercent = static_cast<uint16_t>(level);
                }
            }
            // add==0 时不推进 m_chargeTick，下次调用继续累积，保证不丢充能
        }
    }
    else if (!isPressed() && m_chargePercent > 0)
    {
        // ===== 未填满松开 → 慢慢消退 =====
        uint32_t dt = now - m_decayTick;
        if (dt >= 1)
        {
            uint32_t sub = static_cast<uint32_t>(static_cast<uint64_t>(dt) * 100u / m_config.decayMs);
            if (sub >= 1)
            {
                m_decayTick = now;
                if (sub >= m_chargePercent)
                {
                    m_chargePercent = 0;
                }
                else
                {
                    m_chargePercent -= static_cast<uint16_t>(sub);
                }
            }
        }
    }
}

e_BUTTON_Event Button::getEvent()
{
    e_BUTTON_Event e = m_lastEvent;
    m_lastEvent = e_BUTTON_Event::NONE;
    m_hasEvent = false;
    return e;
}

void Button::resetCounter()
{
    m_counter = {0, 0, 0};
}

} // namespace LoveFinderLib
