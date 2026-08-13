/**
 * @file W25QFLASH.hpp
 * @brief W25Q 系列 SPI Flash 驱动库 (W25Q16 ~ W25Q512) (LoveFinderLib) - C++17
 *
 * 设计要点:
 *  - 基于 STM32 HAL SPI 接口 (HAL_SPI_Transmit/Receive/TransmitReceive)。
 *  - 纯 C++17 接口，位于 LoveFinderLib 命名空间，与 INA226/ENCODER/BUTTON 库风格一致。
 *  - 采用统一的 W25QFLASH_Config 配置结构体 + getDefault() 工厂。
 *  - 支持 W25Q16 / W25Q32 / W25Q64 / W25Q128 / W25Q256 / W25Q512 全系列，
 *    通过 JEDEC ID (0x9F) 自动识别型号 (identify())。
 *  - 自动地址宽度适配: W25Q16~W25Q128 用 24 位地址; W25Q256/W25Q512 自动切换
 *    4 字节地址扩展命令 (读0x13/写0x12/擦除0x21/0x5C/0xDC)，无需修改芯片模式寄存器。
 *  - 支持读 / 页编程 / 跨页连续写 / 扇区擦除 / 32K块擦除 / 64K块擦除 / 全片擦除。
 *
 * @author LoveFinder
 * @date 2026
 */

#ifndef LOVE_FINDER_LIB_W25QFLASH_HPP
#define LOVE_FINDER_LIB_W25QFLASH_HPP

#include <cstdint>

/*============================================================================
 * 平台移植点 (PORT) —— 唯一需要修改的地方
 *
 * 本库只依赖通用 STM32 HAL SPI API (SPI_HandleTypeDef、HAL_SPI_Transmit/
 * HAL_SPI_Receive、HAL_SPI_TransmitReceive、HAL_SPI_GetState)，这些在所有
 * STM32 系列的 HAL 中均一致。移植到其他 STM32 系列时，只需把下面这一行
 * include 改成你所用器件的 HAL 头文件即可，其余代码不用动：
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
 * 型号枚举 (e_W25QFLASH_Model) —— identify() 返回值
 *
 * 由 JEDEC ID (0x9F) 的第 3 字节 (容量字节) 自动判定:
 *   0x15 → W25Q16   (2MB)     0x16 → W25Q32 (4MB)
 *   0x17 → W25Q64   (8MB)     0x18 → W25Q128 (16MB)
 *   0x19 → W25Q256  (32MB)    0x20 → W25Q512 (64MB)
 *============================================================================*/

enum class e_W25QFLASH_Model : uint8_t {
    UNKNOWN = 0,    // 未识别 (无响应 / 非 W25Q 系列 / 未知容量)
    W25Q16  = 1,    // 2MB
    W25Q32  = 2,    // 4MB
    W25Q64  = 3,    // 8MB
    W25Q128 = 4,    // 16MB
    W25Q256 = 5,    // 32MB (4 字节地址)
    W25Q512 = 6     // 64MB (4 字节地址)
};

/*============================================================================
 * 配置结构体 (W25QFLASH_Config)
 *============================================================================*/

struct W25QFLASH_Config {
    SPI_HandleTypeDef* spi = nullptr;   // SPI 外设句柄
    GPIO_TypeDef* csPort = nullptr;     // CS (片选) 引脚 GPIO 端口
    uint16_t csPin = 0;                 // CS (片选) 引脚编号
    uint32_t timeout = 5000;            // 忙等待超时 (ms)

    // 默认配置工厂函数 (SPI 句柄与 CS 引脚必须由用户显式指定)
    static W25QFLASH_Config getDefault() {
        return {
            nullptr,    // spi: 需用户赋值 &hspiX
            nullptr,    // csPort: 需用户赋值
            0,          // csPin: 需用户赋值
            5000        // timeout: 默认 5s
        };
    }
};

/*============================================================================
 * W25QFLASH 类
 *============================================================================*/

class W25QFLASH {
public:
    /**
     * @brief 默认构造函数
     */
    W25QFLASH() = default;

    /**
     * @brief 构造并初始化
     * @param spi SPI 外设句柄
     * @param csPort CS 引脚 GPIO 端口
     * @param csPin CS 引脚编号
     */
    W25QFLASH(SPI_HandleTypeDef* spi, GPIO_TypeDef* csPort, uint16_t csPin);

    /**
     * @brief 构造并初始化
     * @param config 配置参数
     */
    W25QFLASH(const W25QFLASH_Config& config);

    /**
     * @brief 初始化
     * @param spi SPI 外设句柄
     * @param csPort CS 引脚 GPIO 端口
     * @param csPin CS 引脚编号
     */
    void init(SPI_HandleTypeDef* spi, GPIO_TypeDef* csPort, uint16_t csPin);

    /**
     * @brief 初始化 (使用配置结构体)
     * @param config 配置参数
     */
    void init(const W25QFLASH_Config& config);

    /*------------------------------------------------------------------------
     * 型号识别 (检测程序核心)
     *-----------------------------------------------------------------------*/

    /**
     * @brief 识别 Flash 型号 (读 JEDEC ID 0x9F, 按容量字节查表)
     *
     * 识别成功后自动缓存型号与地址宽度: W25Q16~W25Q128 用 24 位地址,
     * W25Q256/W25Q512 自动切换 4 字节地址扩展命令。
     *
     * @return e_W25QFLASH_Model 型号枚举 (UNKNOWN=未识别)
     */
    e_W25QFLASH_Model identify();

    /**
     * @brief 获取型号名称字符串
     * @return 如 "W25Q128" (未识别返回 "UNKNOWN")
     */
    const char* getModelName() const;

    /**
     * @brief 获取 Flash 容量
     * @return 容量 (字节), 未识别返回 0
     */
    uint32_t getCapacityBytes() const;

    /**
     * @brief 检测 Flash 是否在线 (已识别出已知型号)
     * @return true=在线
     */
    bool isOnline();

    /**
     * @brief 读取原始 JEDEC ID (3 字节)
     * @param manufacturer 输出: 厂商 ID (W25Q 系列 = 0xEF)
     * @param memoryType 输出: 存储器类型字节
     * @param capacity 输出: 容量字节
     */
    void readJEDECID(uint8_t* manufacturer, uint8_t* memoryType, uint8_t* capacity);

    /*------------------------------------------------------------------------
     * 状态与使能
     *-----------------------------------------------------------------------*/

    /**
     * @brief 读取状态寄存器
     * @param reg 1=状态寄存器1 (0x05), 2=状态寄存器2 (0x35)
     * @return 状态寄存器值
     */
    uint8_t readStatusReg(uint8_t reg);

    /**
     * @brief 等待 Flash 空闲 (忙位清除)
     * @param timeout 超时 (ms)
     * @return true=空闲, false=超时
     */
    bool waitBusy(uint32_t timeout = 5000);

    /**
     * @brief 写使能 (每次写/擦除前必须调用)
     */
    void writeEnable();

    /**
     * @brief 写禁用
     */
    void writeDisable();

    /*------------------------------------------------------------------------
     * 读 / 写 / 擦除
     *-----------------------------------------------------------------------*/

    /**
     * @brief 读取数据
     * @param addr 起始地址
     * @param data 输出缓冲区
     * @param len 读取长度 (字节)
     * @return true=成功
     */
    bool read(uint32_t addr, uint8_t* data, uint32_t len);

    /**
     * @brief 页编程 (单页, 最多 256 字节, 不跨页)
     * @param addr 页内地址
     * @param data 数据
     * @param len 长度 (1~256)
     * @return true=成功
     */
    bool pageProgram(uint32_t addr, const uint8_t* data, uint16_t len);

    /**
     * @brief 连续写入 (自动跨页拆分)
     *
     * 注意: 目标区域必须先擦除 (Flash 只能 1→0), 本函数不做自动擦除。
     *
     * @param addr 起始地址
     * @param data 数据
     * @param len 长度 (字节)
     * @return true=成功
     */
    bool write(uint32_t addr, const uint8_t* data, uint32_t len);

    /**
     * @brief 扇区擦除 (4KB)
     * @param addr 扇区起始地址 (4KB 对齐)
     * @return true=成功
     */
    bool eraseSector(uint32_t addr);

    /**
     * @brief 块擦除 (32KB)
     * @param addr 块起始地址 (32KB 对齐)
     * @return true=成功
     */
    bool eraseBlock32K(uint32_t addr);

    /**
     * @brief 块擦除 (64KB)
     * @param addr 块起始地址 (64KB 对齐)
     * @return true=成功
     */
    bool eraseBlock64K(uint32_t addr);

    /**
     * @brief 全片擦除 (耗时较长, 请确认用途)
     * @return true=成功
     */
    bool eraseChip();

    /*------------------------------------------------------------------------
     * 功耗管理
     *-----------------------------------------------------------------------*/

    /**
     * @brief 进入掉电模式 (功耗降至最低, 唤醒前不可读写)
     */
    void powerDown();

    /**
     * @brief 退出掉电模式 (唤醒)
     */
    void powerUp();

    /*------------------------------------------------------------------------
     * 配置参数 (查询)
     *-----------------------------------------------------------------------*/

    /**
     * @brief 获取当前型号 (上次 identify 结果)
     * @return 型号枚举
     */
    e_W25QFLASH_Model getModel() const { return m_model; }

private:
    /*===== CS 片选控制 =====*/
    void select() const;
    void deselect() const;

    /*===== SPI 传输 =====*/
    uint8_t transfer(uint8_t data);
    void transferMulti(const uint8_t* txData, uint8_t* rxData, uint32_t len);

    /*===== 地址宽度 =====*/
    uint8_t addrBytes() const { return m_addr4Bytes ? 4 : 3; }
    void sendAddress(uint32_t addr);

    /*===== 容量与命令选择 =====*/
    static uint32_t modelCapacityBytes(e_W25QFLASH_Model model);
    static bool modelNeeds4ByteAddr(e_W25QFLASH_Model model);

    /*===== 成员 =====*/
    SPI_HandleTypeDef* m_hspi = nullptr;    // SPI 外设句柄
    GPIO_TypeDef* m_csPort = nullptr;       // CS 引脚端口
    uint16_t m_csPin = 0;                   // CS 引脚编号
    uint32_t m_timeout = 5000;              // 忙等待超时 (ms)
    e_W25QFLASH_Model m_model = e_W25QFLASH_Model::UNKNOWN;  // 识别出的型号
    bool m_addr4Bytes = false;              // 是否使用 4 字节地址 (W25Q256/512)
};

} // namespace LoveFinderLib

#endif // LOVE_FINDER_LIB_W25QFLASH_HPP
