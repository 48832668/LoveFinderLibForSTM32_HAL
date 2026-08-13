/**
 * @file ENCODER.hpp
 * @brief 旋转编码器驱动库 (LoveFinderLib) - C++17, TIM 硬件编码器模式
 *
 * 设计要点:
 *  - 基于 STM32 TIM 硬件编码器模式 (Encoder Mode)，由硬件捕获 AB 相脉冲，
 *    边沿计数完全在 TIM 硬件完成，不占用 CPU 轮询。
 *  - 【位置读取: 带符号增量累加】16 位硬件计数器按有符号解释
 *    (0..65535 ↔ -32768..32767)，两次读取的带符号差值天然吸收回绕，
 *    无需更新中断、无竞态、不会死锁、边界不再跳变。
 *  - 纯 C++17 接口，位于 LoveFinderLib 命名空间，与 BUTTON 库风格一致。
 *  - 采用统一的 ENCODER_Config 配置结构体 + getDefault() 工厂 (编译期宏默认值)。
 *  - 默认计数范围可通过头文件宏在【编译期】设定，也可通过 setRange() 在
 *    【MCU 运行期间】动态修改。
 *  - 支持常见型号预设 (EC06/EC11/EC12/EC16/EC20) 或 CUSTOM 自定义每格脉冲数。
 *  - 可交换 AB 相 (swapAB) 以修正旋转方向；提供加/减/取反/复位等工具方法。
 *
 * 典型用法 (无需任何中断接线):
 *   ENCODER_Config cfg = ENCODER_Config::getDefault();
 *   cfg.type = e_ENCODER_Type::EC06;   // 按型号
 *   Encoder encoder(&htim3, cfg);
 *   encoder.start();
 *   // 主循环或 UI 定时器:
 *   if (encoder.changed()) { int32_t v = encoder.getCount(); ... }
 *
 * @author LoveFinder
 * @date 2026
 */

#ifndef LOVE_FINDER_LIB_ENCODER_HPP
#define LOVE_FINDER_LIB_ENCODER_HPP

#include <cstdint>

/*============================================================================
 * 平台移植点 (PORT) —— 唯一需要修改的地方
 *
 * 本库只依赖通用 STM32 HAL 的定时器/编码器 API (TIM_HandleTypeDef、
 * HAL_TIM_Encoder_Start/Stop、__HAL_TIM_GET_COUNTER、__HAL_TIM_SET_COUNTER)，
 * 这些在所有 STM32 系列的 HAL 中均一致。移植到其他 STM32 系列时，
 * 只需把下面这一行 include 改成你所用器件的 HAL 头文件即可，其余代码不用动：
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
 * 默认计数范围宏定义 (编译期可改)
 *
 * 这些宏决定 ENCODER_Config::getDefault() 的默认值；也可在运行期通过
 * setRange() 动态修改。
 *============================================================================*/
#ifndef ENCODER_MIN_DEFAULT
    #define ENCODER_MIN_DEFAULT   (-32768)   // 默认最小值
#endif

#ifndef ENCODER_MAX_DEFAULT
    #define ENCODER_MAX_DEFAULT   (32767)    // 默认最大值
#endif

/*============================================================================
 * 是否交换 AB 相 (编译期宏选择, 修正旋转方向)
 *
 * 按实际硬件接线在编译期设定，无需运行期配置:
 *   ENCODER_SWAP_AB = 1  → 交换 AB 相 (某方向旋转计数取反)
 *   ENCODER_SWAP_AB = 0  → 不交换 (默认)
 *
 * 该宏只决定 ENCODER_Config::getDefault() 的默认值；个别实例仍可用
 * config.swapAB 在运行期覆盖 (高级用法)。
 *============================================================================*/
#ifndef ENCODER_SWAP_AB
    #define ENCODER_SWAP_AB   1    // 0=不交换, 1=交换 (本工程接线需要交换以修正方向)
#endif

namespace LoveFinderLib {

/*============================================================================
 * 编码器类型枚举 (enum class)
 *============================================================================*/

enum class e_ENCODER_Type : uint8_t {
    EC06   = 0,    // EC06: 2 脉冲/格
    EC11   = 1,    // EC11: 4 脉冲/格 (最常用)
    EC12   = 2,    // EC12: 4 脉冲/格
    EC16   = 3,    // EC16: 4 脉冲/格
    EC20   = 4,    // EC20: 4 脉冲/格
    CUSTOM = 5,    // 自定义类型
    COUNT  = 6     // 类型总数
};

/*============================================================================
 * 编码器参数结构体
 *============================================================================*/

struct ENCODER_Params {
    const char* name;            // 型号名称
    uint8_t countsPerDetent;     // 每格脉冲数
    const char* description;     // 描述
};

/*============================================================================
 * 编码器配置结构体
 *============================================================================*/

struct ENCODER_Config {
    e_ENCODER_Type type;            // 编码器类型 (CUSTOM 时使用 countsPerDetent)
    uint8_t countsPerDetent;        // 每格脉冲数 (仅 CUSTOM 有效)
    bool swapAB;                    // 是否交换 AB 相 (修正旋转方向)
    int32_t minValue;               // 最小计数 (32位)
    int32_t maxValue;               // 最大计数 (32位)

    // 默认配置工厂函数 (使用头文件顶部的宏默认值)
    static ENCODER_Config getDefault() {
        return {
            e_ENCODER_Type::EC11,   // 默认 EC11: 4 脉冲/格
            4,                      // 每格脉冲数 (CUSTOM 时使用)
            (ENCODER_SWAP_AB != 0), // 是否交换 AB 相 (编译期宏决定)
            ENCODER_MIN_DEFAULT,    // 最小计数
            ENCODER_MAX_DEFAULT     // 最大计数
        };
    }
};

/*============================================================================
 * 编码器事件标志位 (event flags)
 *
 * 每个事件对应一个标志位。置位后一直保持，直到 UI / 用户代码主动读取并清除
 * (peekFlags 非破坏读取 / getAndClearFlags 读取并清除 / clearFlags 直接清除)。
 * 标志位在 getCount() / changed() 读取计数时由库内部置位 (主循环侧)；
 * 用户可在自己的中断函数中【读取】标志位 (peekFlags / testFlag 非破坏，
 * 不涉及共享变量写入，安全)。需要即时置位请使用 setFlag()。
 *============================================================================*/

enum class ENCODER_Flag : uint8_t {
    CHANGED    = 0x01,   // 计数发生变化 (任意方向)
    DIR_PLUS   = 0x02,   // 计数增加 (正方向旋转)
    DIR_MINUS  = 0x04    // 计数减少 (负方向旋转)
};

/**
 * @brief 将编码器方向映射为对应的标志位
 * @param delta 计数增量 (>0 正方向, <0 负方向, 0 无变化)
 * @return 对应标志位掩码 (无变化返回 0)
 */
inline constexpr uint8_t flagOf(int32_t delta) noexcept
{
    if (delta > 0)
    {
        return static_cast<uint8_t>(ENCODER_Flag::CHANGED) |
               static_cast<uint8_t>(ENCODER_Flag::DIR_PLUS);
    }
    if (delta < 0)
    {
        return static_cast<uint8_t>(ENCODER_Flag::CHANGED) |
               static_cast<uint8_t>(ENCODER_Flag::DIR_MINUS);
    }
    return 0;
}

/*============================================================================
 * 编码器类 (TIM 硬件编码器模式)
 *============================================================================*/

class Encoder {
public:
    /**
     * @brief 默认构造函数
     */
    Encoder() = default;

    /**
     * @brief 构造并初始化
     * @param tim TIM外设句柄 (已配置为编码器模式)
     * @param config 配置参数 (可选，使用默认配置)
     */
    Encoder(TIM_HandleTypeDef* tim, const ENCODER_Config& config = ENCODER_Config::getDefault());

    /**
     * @brief 初始化
     * @param tim TIM外设句柄
     * @param config 配置参数
     */
    void init(TIM_HandleTypeDef* tim, const ENCODER_Config& config);

    /**
     * @brief 初始化 (使用默认配置)
     * @param tim TIM外设句柄
     */
    void init(TIM_HandleTypeDef* tim);

    /**
     * @brief 设置计数范围 (运行期动态修改)
     * @param minVal 最小值
     * @param maxVal 最大值
     */
    void setRange(int32_t minVal, int32_t maxVal);

    /**
     * @brief 启动编码器 (HAL 启动 + 建立读取基线)
     * @note 无更新中断、无 NVIC；TIM 硬件边沿计数，主循环按需读取。
     */
    void start();

    /**
     * @brief 停止编码器 (HAL 停止)
     */
    void stop();

    /**
     * @brief 获取当前 32 位格计数 (已按范围限幅)
     * @return 格计数
     */
    int32_t getCount() const;

    /**
     * @brief 获取当前 32 位原始脉冲计数 (回绕安全，未限幅)
     * @return 原始脉冲数
     */
    int32_t getRawPosition() const;

    /**
     * @brief 检测自上次调用后计数是否变化 (无需轮询，可配合 UI 定时器调用)
     * @return true=有变化
     */
    bool changed();

    /*================ 复杂系统辅助 (方向/活动时间/范围) ================*/

    /**
     * @brief 获取最近一次旋转方向
     * @return +1=正方向, -1=负方向, 0=尚未旋转或不可用
     * @note 直接可查询的方向，适合菜单循环/数值微调；标志位仍是事件式语义。
     */
    int8_t getDirection() const { return m_direction; }

    /**
     * @brief 获取最后旋转时刻 (HAL_GetTick 时间戳)
     * @return 时间戳 (ms)，从未旋转返回 0
     * @note 用于惯性滚动/长按加速/超时退出编辑等复杂 UI 逻辑。
     */
    uint32_t getLastActivityTick() const { return m_lastActivityTick; }

    /**
     * @brief 获取当前最小计数值
     * @return 最小计数
     */
    int32_t getMinValue() const { return m_minValue; }

    /**
     * @brief 获取当前最大计数值
     * @return 最大计数
     */
    int32_t getMaxValue() const { return m_maxValue; }

    /*================ 事件标志位 (便于外部/UI/自定义中断读取) ================*/

    /**
     * @brief 置位一个事件标志位
     * @note 可在用户自定义中断中调用；置位后保持，直到被读取/清除。
     * @param f 标志位
     */
    void setFlag(ENCODER_Flag f) { m_flags |= static_cast<uint8_t>(f); }

    /**
     * @brief 非破坏性读取全部标志位 (可反复轮询，不会清除)
     * @return 标志位掩码
     */
    uint8_t peekFlags() const { return m_flags; }

    /**
     * @brief 测试某个标志位是否置位 (非破坏性)
     * @param f 标志位
     * @return 是否置位
     */
    bool testFlag(ENCODER_Flag f) const
    {
        return (m_flags & static_cast<uint8_t>(f)) != 0u;
    }

    /**
     * @brief 读取并清除全部标志位 (读取后主动清除)
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
     * @brief 设置计数值
     * @param value 新计数值
     */
    void setCount(int32_t value);

    /**
     * @brief 增加计数值
     * @param delta 增量值
     */
    void addCount(int32_t delta);

    /**
     * @brief 计数值取反
     */
    void invertCount();

    /**
     * @brief 复位计数值为最小值
     */
    void reset();

    /**
     * @brief 获取每格脉冲数
     * @return 每格脉冲数
     */
    uint8_t getCountsPerDetent() const { return m_countsPerDetent; }

    /**
     * @brief 获取编码器类型
     * @return 类型枚举
     */
    e_ENCODER_Type getType() const { return m_type; }

    /**
     * @brief 获取类型参数
     * @param type 编码器类型
     * @return 参数结构体引用
     */
    static const ENCODER_Params& getParams(e_ENCODER_Type type);

private:
    TIM_HandleTypeDef* m_tim = nullptr;     // TIM外设
    e_ENCODER_Type m_type = e_ENCODER_Type::EC11;
    uint8_t m_countsPerDetent = 4;          // 每格脉冲数
    bool m_swapAB = false;                  // 交换AB相
    int32_t m_minValue = ENCODER_MIN_DEFAULT;
    int32_t m_maxValue = ENCODER_MAX_DEFAULT;

    // 带符号增量累加 (无中断扩展, 无竞态)
    mutable volatile int32_t m_pos = 0;     // 32 位累计位置 (原始脉冲数)
    mutable volatile int16_t m_lastRaw = 0; // 上次读到的硬件计数器 (有符号 16 位)
    mutable volatile int32_t m_lastPos = 0; // 上次读取位置 (changed 检测)
    mutable volatile uint8_t m_flags = 0;   // 事件标志位 (主循环置位, UI/中断读取清除)
    mutable int8_t    m_direction = 0;      // 最近旋转方向 (+1/-1/0)
    mutable uint32_t  m_lastActivityTick = 0; // 最后旋转时刻 (HAL_GetTick)
};

} // namespace LoveFinderLib

#endif // LOVE_FINDER_LIB_ENCODER_HPP
