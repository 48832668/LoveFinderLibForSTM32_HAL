/**
 * @file BUTTON.hpp
 * @brief EXTI 驱动的稳定按键库 (LoveFinderLib) - C++17
 *
 * 设计要点:
 *  - KEY 引脚必须启用 EXTI 外部中断 (硬性要求)。
 *  - 初始化时自动配置为【下降沿触发】(GPIO_MODE_IT_FALLING)，并启用 NVIC。
 *    - 若用户已用 CubeMX 配置好 EXTI，本库 init() 仍会再次显式配置为下降沿，
 *      因此"是否已用 CubeMX 配置"都不影响正确性。
 *  - 硬件依赖: 按键 IO 需有外部上拉 (activeLow = true)，按下为低电平。
 *    (库内再加内部上拉作为兜底，防止误触发。)
 *  - 通过 EXTI 下降沿中断 + 主循环状态机，稳定区分: 单击 / 双击 / 长按。
 *  - 长按阈值、双击间隔阈值可通过宏在【编译期】设定默认值，也可通过
 *    setLongPressMs() / setDoubleClickMs() 在【MCU 运行期间】动态修改。
 *  - 附带"长按充能"矩形区域 (charge meter):
 *      长按(达到阈值后)持续充能，填满一次 → 长按次数 +1 并清零继续充能；
 *      未填满松开 → 内部填充慢慢消退 (decay)。
 *  - 提供事件标志位 (event flags) 与事件计数命名访问器，便于外部/UI 读取：
 *      单击/双击/长按/按下/释放 各占一个标志位 bit，ISR 可置位 (setFlag)，
 *      UI 可非破坏读取 (peekFlags/testFlag) 或读取并清除 (getAndClearFlags)；
 *      计数可用 getClickCount()/getDoubleClickCount()/getLongPressCount() 直接读取。
 *
 * @author LoveFinder
 * @date 2026
 */

#ifndef LOVE_FINDER_LIB_BUTTON_HPP
#define LOVE_FINDER_LIB_BUTTON_HPP

#include <cstdint>

/*============================================================================
 * 平台移植点 (PORT) —— 唯一需要修改的地方
 *
 * 本库只依赖通用 STM32 HAL API (HAL_GPIO_Init / HAL_GPIO_ReadPin / HAL_GetTick
 * / HAL_NVIC_* 及 EXTI 中断号)。EXTI → NVIC 中断号映射通过预处理按系列自动选择
 * (支持 F0/F1/F3/F4/F7、G0/G4、C0、H5、L4/L5、U5、WB/WL 等，见 configureExti())。
 * 因此移植到其他 STM32 系列时，只需把下面这一行 include 改成你所用器件的 HAL
 * 头文件即可，其余代码不用动：
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

/*============================================================================
 * 默认阈值宏定义 (编译期可改)
 *
 * 这些宏决定 BUTTON_Config::getDefault() 的默认值。
 * 若需在编译期统一调整，可在包含本头文件之前重新 #define 覆盖；也可在
 * MCU 运行期间通过 LoveFinderLib::Button::setLongPressMs() /
 * setDoubleClickMs() 动态修改 (见类方法)。
 *============================================================================*/
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
    #define BUTTON_CHARGE_FULL_MS_DEFAULT   2000    // 长按充能填满时间 (ms)
#endif

#ifndef BUTTON_DECAY_MS_DEFAULT
    #define BUTTON_DECAY_MS_DEFAULT         3000    // 未填满松开后消退时间 (ms)
#endif

namespace LoveFinderLib {

/*============================================================================
 * 按键事件枚举 (enum class)
 *============================================================================*/

enum class e_BUTTON_Event : uint8_t {
    NONE         = 0,   // 无事件
    CLICK        = 1,   // 单击
    DOUBLE_CLICK = 2,   // 双击
    LONG_PRESS   = 3,   // 长按 (达到阈值瞬间触发一次)
    PRESS_DOWN   = 4,   // 按下瞬间
    RELEASE      = 5    // 释放
};

/*============================================================================
 * 事件标志位 (event flags)
 *
 * 每个事件对应一个标志位。置位后一直保持，直到 UI 主动读取并清除
 * (peekFlags 非破坏读取 / getAndClearFlags 读取并清除 / clearFlags 直接清除)。
 * 标志位寄存器为 volatile，可在中断上下文 (onExti) 中置位；
 * 主循环 / UI 负责读取与清除。UI 可基于标志位做即时动作，不必依赖事件计数。
 *============================================================================*/

enum class BUTTON_Flag : uint8_t {
    PRESS_DOWN    = 0x01,   // 按下瞬间
    CLICK         = 0x02,   // 单击
    DOUBLE_CLICK  = 0x04,   // 双击
    LONG_PRESS    = 0x08,   // 长按
    RELEASE       = 0x10    // 释放
};

/**
 * @brief 将按键事件映射为对应的标志位
 * @param e 事件
 * @return 对应标志位掩码 (无事件返回 0)
 */
inline constexpr uint8_t flagOf(e_BUTTON_Event e) noexcept
{
    switch (e)
    {
        case e_BUTTON_Event::PRESS_DOWN:    return static_cast<uint8_t>(BUTTON_Flag::PRESS_DOWN);
        case e_BUTTON_Event::CLICK:         return static_cast<uint8_t>(BUTTON_Flag::CLICK);
        case e_BUTTON_Event::DOUBLE_CLICK:  return static_cast<uint8_t>(BUTTON_Flag::DOUBLE_CLICK);
        case e_BUTTON_Event::LONG_PRESS:    return static_cast<uint8_t>(BUTTON_Flag::LONG_PRESS);
        case e_BUTTON_Event::RELEASE:       return static_cast<uint8_t>(BUTTON_Flag::RELEASE);
        default:                            return 0;
    }
}

/*============================================================================
 * 按键状态枚举 (enum class)
 *============================================================================*/

enum class e_BUTTON_State : uint8_t {
    IDLE         = 0,   // 空闲状态
    DEBOUNCE     = 1,   // 消抖状态
    PRESSED      = 2,   // 已按下
    WAIT_RELEASE = 3,   // 等待释放 (保留，兼容)
    WAIT_CLICK   = 4    // 等待第二次点击
};

/*============================================================================
 * 按键配置结构体
 *============================================================================*/

struct BUTTON_Config {
    uint16_t debounceMs;      // 消抖时间 (ms)
    uint16_t longPressMs;     // 长按判定阈值 (ms)
    uint16_t doubleClickMs;   // 双击间隔时间 (ms)
    uint16_t chargeFullMs;    // 长按充能填满所需时间 (ms)
    uint16_t decayMs;         // 未填满松开后消退所需时间 (ms)
    bool activeLow;           // true: 低电平按下, false: 高电平按下

    // 默认配置工厂函数 (使用头文件顶部的宏默认值)
    static BUTTON_Config getDefault() {
        return {
            BUTTON_DEBOUNCE_MS_DEFAULT,
            BUTTON_LONG_PRESS_MS_DEFAULT,
            BUTTON_DOUBLE_CLICK_MS_DEFAULT,
            BUTTON_CHARGE_FULL_MS_DEFAULT,
            BUTTON_DECAY_MS_DEFAULT,
            true  // 默认低电平有效
        };
    }
};

/*============================================================================
 * 按键计数器结构体
 *============================================================================*/

struct BUTTON_Counter {
    uint32_t clickCount;       // 单击次数
    uint32_t doubleClickCount; // 双击次数
    uint32_t longPressCount;   // 长按次数 (充能填满次数)
};

/*============================================================================
 * 按键类
 *============================================================================*/

class Button {
public:
    /**
     * @brief 默认构造函数
     */
    Button() = default;

    /**
     * @brief 构造并初始化
     * @param port GPIO端口
     * @param pin GPIO引脚
     * @param config 配置参数 (可选，使用默认配置)
     */
    Button(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config = BUTTON_Config::getDefault());

    /**
     * @brief 初始化
     * @param port GPIO端口
     * @param pin GPIO引脚
     * @param config 配置参数
     */
    void init(GPIO_TypeDef* port, uint16_t pin, const BUTTON_Config& config);

    /**
     * @brief 初始化 (使用默认配置)
     * @param port GPIO端口
     * @param pin GPIO引脚
     */
    void init(GPIO_TypeDef* port, uint16_t pin);

    /**
     * @brief 更新按键状态与充能 (需在主循环中定期调用，建议 1-10ms)
     * @return 当前按键事件
     */
    e_BUTTON_Event update();

    /**
     * @brief 检查是否有待处理事件
     * @return true=有事件, false=无事件
     */
    bool hasEvent() const { return m_hasEvent; }

    /**
     * @brief 获取并清除事件
     * @return 按键事件
     */
    e_BUTTON_Event getEvent();

    /**
     * @brief 获取按键计数器
     * @return 计数器结构体引用
     */
    const BUTTON_Counter& getCounter() const { return m_counter; }

    /**
     * @brief 重置按键计数器
     */
    void resetCounter();

    /*================ 事件计数命名访问器 (便于外部/UI 读取) ================*/

    /**
     * @brief 获取单击次数
     * @return 单击次数
     */
    uint32_t getClickCount() const { return m_counter.clickCount; }

    /**
     * @brief 获取双击次数
     * @return 双击次数
     */
    uint32_t getDoubleClickCount() const { return m_counter.doubleClickCount; }

    /**
     * @brief 获取长按次数 (长按充能填满次数)
     * @return 长按次数
     */
    uint32_t getLongPressCount() const { return m_counter.longPressCount; }

    /*================ 事件标志位 (便于外部/UI 读取) ================*/

    /**
     * @brief 置位一个事件标志位
     * @note 可在中断上下文 (onExti) 中调用；置位后保持，直到被读取/清除。
     * @param e 事件
     */
    void setFlag(e_BUTTON_Event e) { m_flags |= flagOf(e); }

    /**
     * @brief 非破坏性读取全部标志位 (UI 可反复轮询，不会清除)
     * @return 标志位掩码
     */
    uint8_t peekFlags() const { return m_flags; }

    /**
     * @brief 测试某个标志位是否置位 (非破坏性)
     * @param f 标志位
     * @return 是否置位
     */
    bool testFlag(BUTTON_Flag f) const
    {
        return (m_flags & static_cast<uint8_t>(f)) != 0u;
    }

    /**
     * @brief 读取并清除全部标志位 (UI 读取后主动清除)
     * @return 读取到的标志位掩码
     */
    uint8_t getAndClearFlags()
    {
        uint8_t f = m_flags;
        m_flags = 0;
        return f;
    }

    /**
     * @brief 直接清除全部标志位
     */
    void clearFlags() { m_flags = 0; }

    /**
     * @brief 检查当前引脚是否属于此按键
     * @param pin 触发中断的引脚
     * @return 是否匹配
     */
    bool isPin(uint16_t pin) const { return m_pin == pin; }

    /**
     * @brief 获取当前按键物理状态
     * @return true=按下, false=释放
     */
    bool isPressed() const;

    /**
     * @brief 获取当前状态
     * @return 状态枚举
     */
    e_BUTTON_State getState() const { return m_state; }

    /*================ 运行时阈值修改 (MCU 运行期间可动态调整) ================*/

    /**
     * @brief 设置长按判定阈值 (运行期动态修改)
     * @param ms 长按判定阈值 (毫秒)
     */
    void setLongPressMs(uint16_t ms) { m_config.longPressMs = ms; }

    /**
     * @brief 获取当前长按判定阈值
     * @return 毫秒
     */
    uint16_t getLongPressMs() const { return m_config.longPressMs; }

    /**
     * @brief 设置双击间隔阈值 (运行期动态修改)
     * @param ms 双击间隔时间 (毫秒)
     */
    void setDoubleClickMs(uint16_t ms) { m_config.doubleClickMs = ms; }

    /**
     * @brief 获取当前双击间隔阈值
     * @return 毫秒
     */
    uint16_t getDoubleClickMs() const { return m_config.doubleClickMs; }

    /*================ EXTI 相关 ================*/

    /**
     * @brief 配置 EXTI (下降沿触发) 并启用对应 NVIC
     * @note 在 init() 中自动调用。若用户已用 CubeMX 配置过 EXTI，仍会重复配置
     *       为下降沿，因此结果一致。
     */
    void configureExti();

    /**
     * @brief EXTI 中断回调入口 (由 HAL_GPIO_EXTI_Callback 分发到此)
     * @note 必须在中断上下文调用 (尽量保持短小: 仅记录标志与时间戳)。
     */
    void onExti();

    /*================ 长按充能 (charge meter) ================*/

    /**
     * @brief 获取当前充能百分比
     * @return 0..100
     */
    uint8_t getChargePercent() const { return m_chargePercent; }

private:
    GPIO_TypeDef* m_port = nullptr;
    uint16_t m_pin = 0;

    BUTTON_Config m_config;
    e_BUTTON_State m_state = e_BUTTON_State::IDLE;

    /* --- 中断侧 (volatile) --- */
    volatile bool   m_pressPending = false;   // ISR 置位，主循环消费
    volatile uint32_t m_pressTick  = 0;       // ISR 记录按下时间戳

    /* --- 主循环侧 --- */
    uint32_t m_debounceTick = 0;
    uint32_t m_pressTime    = 0;              // 确认按下时间
    bool m_waitingSecondClick = false;        // 是否等待第二次点击
    bool m_longPressFired   = false;          // 本次按下是否已触发长按事件
    bool m_longPressActive  = false;          // 是否处于长按充能中

    /* --- 充能 (charge meter) --- */
    uint16_t m_chargePercent = 0;             // 0..100
    uint32_t m_chargeTick    = 0;
    uint32_t m_decayTick     = 0;

    BUTTON_Counter m_counter = {0, 0, 0};

    volatile e_BUTTON_Event m_lastEvent = e_BUTTON_Event::NONE;
    volatile bool m_hasEvent = false;
    volatile uint8_t m_flags = 0;   // 事件标志位 (ISR 可置位，主循环/UI 读取清除)

    void pushEvent(e_BUTTON_Event e);
    void updateCharge(uint32_t now);
};

/*============================================================================
 * 使用说明 (C++17 纯 C++ 接口)
 *
 *   Button btn;
 *   btn.init(GPIOA, GPIO_PIN_8, config);
 *
 *   // 主循环定期调用 update()，返回当前事件并累积标志位
 *   e_BUTTON_Event evt = btn.update();
 *
 *   // UI 侧: 非破坏读取标志位 / 读取并清除标志位
 *   if (btn.testFlag(BUTTON_Flag::DOUBLE_CLICK)) { ... }
 *   uint8_t flags = btn.getAndClearFlags();
 *
 *   // 计数访问器
 *   uint32_t clicks = btn.getClickCount();
 *
 *   // EXTI 回调中分发到 btn.onExti()
 *   extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) { ... }
 *============================================================================*/

} // namespace LoveFinderLib

#endif // LOVE_FINDER_LIB_BUTTON_HPP
