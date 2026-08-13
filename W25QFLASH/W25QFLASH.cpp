/**
 * @file W25QFLASH.cpp
 * @brief W25Q Series SPI Flash Driver Implementation (W25Q16~W25Q512) - C++17 (LoveFinderLib)
 */

#include "W25QFLASH.hpp"
#include <cstring>

namespace LoveFinderLib {

/*============================================================================
 * 指令集 (constexpr)
 *
 * 24 位地址命令 (W25Q16~W25Q128) 与 4 字节地址扩展命令 (W25Q256/W25Q512):
 *   读/写/擦除命令按当前地址宽度自动选择, 其余命令全系列通用。
 *============================================================================*/

namespace {

// JEDEC ID 命令
constexpr uint8_t CMD_READ_ID          = 0x9F;   // 读 JEDEC ID (3 字节)

// 状态寄存器
constexpr uint8_t CMD_READ_STATUS1     = 0x05;   // 读状态寄存器1
constexpr uint8_t CMD_READ_STATUS2     = 0x35;   // 读状态寄存器2

// 写使能 / 禁用
constexpr uint8_t CMD_WRITE_ENABLE     = 0x06;
constexpr uint8_t CMD_WRITE_DISABLE    = 0x04;

// 读数据 (按地址宽度自动选择)
constexpr uint8_t CMD_READ_3B          = 0x03;   // 24 位地址读
constexpr uint8_t CMD_READ_4B          = 0x13;   // 32 位地址读

// 页编程 (按地址宽度自动选择)
constexpr uint8_t CMD_PAGE_PROGRAM_3B  = 0x02;   // 24 位地址页编程
constexpr uint8_t CMD_PAGE_PROGRAM_4B  = 0x12;   // 32 位地址页编程

// 擦除 (按地址宽度自动选择)
constexpr uint8_t CMD_SECTOR_ERASE_3B = 0x20;    // 4KB 扇区擦除
constexpr uint8_t CMD_SECTOR_ERASE_4B = 0x21;    // 4KB 扇区擦除 (4 字节地址)
constexpr uint8_t CMD_BLOCK_ERASE_32K_3B = 0x52; // 32KB 块擦除 (仅 3 字节地址)
// 注意: W25Q256JV 指令集没有 32KB 块擦除的专用 4 字节地址命令 (0x5C 无效!)
//       4B 器件上 eraseBlock32K() 内部回退为 8 次 4KB 扇区擦除 (21h)
constexpr uint8_t CMD_BLOCK_ERASE_64K_3B = 0xD8; // 64KB 块擦除
constexpr uint8_t CMD_BLOCK_ERASE_64K_4B = 0xDC; // 64KB 块擦除 (4 字节地址)
constexpr uint8_t CMD_CHIP_ERASE        = 0xC7;  // 全片擦除

// 功耗管理
constexpr uint8_t CMD_POWER_DOWN        = 0xB9;
constexpr uint8_t CMD_RELEASE_POWER_DOWN = 0xAB;

// 状态寄存器位
constexpr uint8_t STATUS_BUSY           = 0x01;  // 忙标志

// 尺寸常量
constexpr uint32_t PAGE_SIZE            = 256;   // 页大小
constexpr uint32_t SECTOR_SIZE          = 4096;  // 扇区大小
constexpr uint32_t BLOCK32K_SIZE        = 32768; // 32K 块
constexpr uint32_t BLOCK64K_SIZE        = 65536; // 64K 块

// JEDEC ID 厂商字节 (Winbond)
constexpr uint8_t JEDEC_MANUFACTURER_WINBOND = 0xEF;

} // namespace

/*============================================================================
 * 型号容量表
 *============================================================================*/

uint32_t W25QFLASH::modelCapacityBytes(e_W25QFLASH_Model model)
{
    switch (model)
    {
    case e_W25QFLASH_Model::W25Q16:  return 2u   * 1024u * 1024u;   // 2MB
    case e_W25QFLASH_Model::W25Q32:  return 4u   * 1024u * 1024u;   // 4MB
    case e_W25QFLASH_Model::W25Q64:  return 8u   * 1024u * 1024u;   // 8MB
    case e_W25QFLASH_Model::W25Q128: return 16u  * 1024u * 1024u;   // 16MB
    case e_W25QFLASH_Model::W25Q256: return 32u  * 1024u * 1024u;   // 32MB
    case e_W25QFLASH_Model::W25Q512: return 64u  * 1024u * 1024u;   // 64MB
    default:                          return 0;
    }
}

bool W25QFLASH::modelNeeds4ByteAddr(e_W25QFLASH_Model model)
{
    // W25Q256 (32MB) 及以上地址超过 24 位, 需 4 字节地址
    return model == e_W25QFLASH_Model::W25Q256 ||
           model == e_W25QFLASH_Model::W25Q512;
}

/*============================================================================
 * 构造 / 初始化
 *============================================================================*/

W25QFLASH::W25QFLASH(SPI_HandleTypeDef* spi, GPIO_TypeDef* csPort, uint16_t csPin)
{
    init(spi, csPort, csPin);
}

W25QFLASH::W25QFLASH(const W25QFLASH_Config& config)
{
    init(config);
}

void W25QFLASH::init(SPI_HandleTypeDef* spi, GPIO_TypeDef* csPort, uint16_t csPin)
{
    m_hspi = spi;
    m_csPort = csPort;
    m_csPin = csPin;
    m_timeout = W25QFLASH_Config::getDefault().timeout;
    m_model = e_W25QFLASH_Model::UNKNOWN;
    m_addr4Bytes = false;

    deselect();   // CS 初始为高 (不选中)
}

void W25QFLASH::init(const W25QFLASH_Config& config)
{
    m_hspi = config.spi;
    m_csPort = config.csPort;
    m_csPin = config.csPin;
    m_timeout = config.timeout;
    m_model = e_W25QFLASH_Model::UNKNOWN;
    m_addr4Bytes = false;

    deselect();
}

/*============================================================================
 * 型号识别
 *============================================================================*/

e_W25QFLASH_Model W25QFLASH::identify()
{
    uint8_t manufacturer = 0;
    uint8_t memoryType = 0;
    uint8_t capacity = 0;
    readJEDECID(&manufacturer, &memoryType, &capacity);

    // 重置为未知, 仅当厂商匹配且容量字节在已知表中才识别成功
    m_model = e_W25QFLASH_Model::UNKNOWN;
    m_addr4Bytes = false;

    if (manufacturer != JEDEC_MANUFACTURER_WINBOND)
    {
        return m_model;
    }

    switch (capacity)
    {
    case 0x15: m_model = e_W25QFLASH_Model::W25Q16;  break;
    case 0x16: m_model = e_W25QFLASH_Model::W25Q32;  break;
    case 0x17: m_model = e_W25QFLASH_Model::W25Q64;  break;
    case 0x18: m_model = e_W25QFLASH_Model::W25Q128; break;
    case 0x19: m_model = e_W25QFLASH_Model::W25Q256; break;
    case 0x20: m_model = e_W25QFLASH_Model::W25Q512; break;
    default:   m_model = e_W25QFLASH_Model::UNKNOWN; break;
    }

    // W25Q256/W25Q512: 自动切换 4 字节地址
    m_addr4Bytes = modelNeeds4ByteAddr(m_model);

    return m_model;
}

const char* W25QFLASH::getModelName() const
{
    switch (m_model)
    {
    case e_W25QFLASH_Model::W25Q16:  return "W25Q16";
    case e_W25QFLASH_Model::W25Q32:  return "W25Q32";
    case e_W25QFLASH_Model::W25Q64:  return "W25Q64";
    case e_W25QFLASH_Model::W25Q128: return "W25Q128";
    case e_W25QFLASH_Model::W25Q256: return "W25Q256";
    case e_W25QFLASH_Model::W25Q512: return "W25Q512";
    default:                         return "UNKNOWN";
    }
}

uint32_t W25QFLASH::getCapacityBytes() const
{
    return modelCapacityBytes(m_model);
}

bool W25QFLASH::isOnline()
{
    return identify() != e_W25QFLASH_Model::UNKNOWN;
}

void W25QFLASH::readJEDECID(uint8_t* manufacturer, uint8_t* memoryType, uint8_t* capacity)
{
    select();
    transfer(CMD_READ_ID);
    *manufacturer = transfer(0xFF);
    *memoryType = transfer(0xFF);
    *capacity = transfer(0xFF);
    deselect();
}

/*============================================================================
 * 状态与使能
 *============================================================================*/

uint8_t W25QFLASH::readStatusReg(uint8_t reg)
{
    select();
    if (reg == 1)
    {
        transfer(CMD_READ_STATUS1);
    }
    else
    {
        transfer(CMD_READ_STATUS2);
    }
    uint8_t status = transfer(0xFF);
    deselect();
    return status;
}

bool W25QFLASH::waitBusy(uint32_t timeout)
{
    uint32_t startTime = HAL_GetTick();

    while ((HAL_GetTick() - startTime) < timeout)
    {
        if ((readStatusReg(1) & STATUS_BUSY) == 0)
        {
            return true;
        }
        HAL_Delay(1);
    }
    return false;
}

void W25QFLASH::writeEnable()
{
    select();
    transfer(CMD_WRITE_ENABLE);
    deselect();
}

void W25QFLASH::writeDisable()
{
    select();
    transfer(CMD_WRITE_DISABLE);
    deselect();
}

/*============================================================================
 * 底层传输
 *============================================================================*/

void W25QFLASH::select() const
{
    if (m_csPort != nullptr && m_csPin != 0)
    {
        HAL_GPIO_WritePin(m_csPort, m_csPin, GPIO_PIN_RESET);
    }
}

void W25QFLASH::deselect() const
{
    if (m_csPort != nullptr && m_csPin != 0)
    {
        HAL_GPIO_WritePin(m_csPort, m_csPin, GPIO_PIN_SET);
    }
}

uint8_t W25QFLASH::transfer(uint8_t data)
{
    uint8_t rxData = 0;
    if (m_hspi != nullptr)
    {
        HAL_SPI_TransmitReceive(m_hspi, &data, &rxData, 1, 100);
    }
    return rxData;
}

void W25QFLASH::transferMulti(const uint8_t* txData, uint8_t* rxData, uint32_t len)
{
    // 小数据量使用阻塞传输，大数据量使用 DMA (与 W25Q128 旧库一致)
    if (len < 32)
    {
        if (txData != nullptr && rxData != nullptr)
        {
            HAL_SPI_TransmitReceive(m_hspi, const_cast<uint8_t*>(txData), rxData, len, 1000);
        }
        else if (txData != nullptr)
        {
            HAL_SPI_Transmit(m_hspi, const_cast<uint8_t*>(txData), len, 1000);
        }
        else if (rxData != nullptr)
        {
            for (uint32_t i = 0; i < len; i++)
            {
                rxData[i] = transfer(0xFF);
            }
        }
    }
    else
    {
        if (txData != nullptr && rxData != nullptr)
        {
            HAL_SPI_TransmitReceive_DMA(m_hspi, const_cast<uint8_t*>(txData), rxData, len);
            while (HAL_SPI_GetState(m_hspi) != HAL_SPI_STATE_READY) {}
        }
        else if (txData != nullptr)
        {
            HAL_SPI_Transmit_DMA(m_hspi, const_cast<uint8_t*>(txData), len);
            while (HAL_SPI_GetState(m_hspi) != HAL_SPI_STATE_READY) {}
        }
        else if (rxData != nullptr)
        {
            // DMA 接收需要先发送 dummy 字节 (DMA 需要静态内存)
            static uint8_t dummyTx[256];
            memset(dummyTx, 0xFF, sizeof(dummyTx));

            uint32_t remaining = len;
            uint32_t offset = 0;
            while (remaining > 0)
            {
                uint32_t chunkSize = (remaining > sizeof(dummyTx)) ? sizeof(dummyTx) : remaining;
                HAL_SPI_TransmitReceive_DMA(m_hspi, dummyTx, rxData + offset, chunkSize);
                while (HAL_SPI_GetState(m_hspi) != HAL_SPI_STATE_READY) {}
                offset += chunkSize;
                remaining -= chunkSize;
            }
        }
    }
}

void W25QFLASH::sendAddress(uint32_t addr)
{
    if (m_addr4Bytes)
    {
        transfer(static_cast<uint8_t>(addr >> 24));
    }
    transfer(static_cast<uint8_t>(addr >> 16));
    transfer(static_cast<uint8_t>(addr >> 8));
    transfer(static_cast<uint8_t>(addr));
}

/*============================================================================
 * 读 / 写 / 擦除
 *============================================================================*/

bool W25QFLASH::read(uint32_t addr, uint8_t* data, uint32_t len)
{
    if (m_hspi == nullptr || data == nullptr)
    {
        return false;
    }
    if ((addr + len) > getCapacityBytes())
    {
        return false;
    }

    select();
    transfer(m_addr4Bytes ? CMD_READ_4B : CMD_READ_3B);
    sendAddress(addr);
    transferMulti(nullptr, data, len);
    deselect();

    return true;
}

bool W25QFLASH::pageProgram(uint32_t addr, const uint8_t* data, uint16_t len)
{
    if (m_hspi == nullptr || data == nullptr)
    {
        return false;
    }
    if (len == 0 || len > PAGE_SIZE)
    {
        return false;
    }
    if ((addr + len) > getCapacityBytes())
    {
        return false;
    }

    writeEnable();

    select();
    transfer(m_addr4Bytes ? CMD_PAGE_PROGRAM_4B : CMD_PAGE_PROGRAM_3B);
    sendAddress(addr);
    transferMulti(data, nullptr, len);
    deselect();

    return waitBusy(m_timeout);
}

bool W25QFLASH::write(uint32_t addr, const uint8_t* data, uint32_t len)
{
    if (m_hspi == nullptr || data == nullptr)
    {
        return false;
    }
    if ((addr + len) > getCapacityBytes())
    {
        return false;
    }

    uint32_t remaining = len;
    uint32_t currentAddr = addr;
    const uint8_t* currentData = data;

    while (remaining > 0)
    {
        // 计算当前页剩余空间, 拆分为多个页编程
        uint16_t pageOffset = static_cast<uint16_t>(currentAddr % PAGE_SIZE);
        uint16_t pageRemaining = static_cast<uint16_t>(PAGE_SIZE - pageOffset);
        uint16_t writeLen = (remaining < pageRemaining) ? static_cast<uint16_t>(remaining) : pageRemaining;

        if (!pageProgram(currentAddr, currentData, writeLen))
        {
            return false;
        }

        currentAddr += writeLen;
        currentData += writeLen;
        remaining -= writeLen;
    }

    return true;
}

bool W25QFLASH::eraseSector(uint32_t addr)
{
    if ((addr + SECTOR_SIZE) > getCapacityBytes())
    {
        return false;
    }

    writeEnable();

    select();
    transfer(m_addr4Bytes ? CMD_SECTOR_ERASE_4B : CMD_SECTOR_ERASE_3B);
    sendAddress(addr);
    deselect();

    return waitBusy(m_timeout);
}

bool W25QFLASH::eraseBlock32K(uint32_t addr)
{
    if ((addr + BLOCK32K_SIZE) > getCapacityBytes())
    {
        return false;
    }

    // W25Q256JV 无 32KB 块擦除的 4 字节地址专用命令 (0x5C 无效)。
    // 4B 器件回退为 8 次 4KB 扇区擦除 (21h), 语义等价且经过实机验证。
    if (m_addr4Bytes)
    {
        for (uint32_t off = 0; off < BLOCK32K_SIZE; off += SECTOR_SIZE)
        {
            if (!eraseSector(addr + off))
            {
                return false;
            }
        }
        return true;
    }

    writeEnable();

    select();
    transfer(CMD_BLOCK_ERASE_32K_3B);
    sendAddress(addr);
    deselect();

    return waitBusy(m_timeout);
}

bool W25QFLASH::eraseBlock64K(uint32_t addr)
{
    if ((addr + BLOCK64K_SIZE) > getCapacityBytes())
    {
        return false;
    }

    writeEnable();

    select();
    transfer(m_addr4Bytes ? CMD_BLOCK_ERASE_64K_4B : CMD_BLOCK_ERASE_64K_3B);
    sendAddress(addr);
    deselect();

    return waitBusy(m_timeout);
}

bool W25QFLASH::eraseChip()
{
    writeEnable();

    select();
    transfer(CMD_CHIP_ERASE);
    deselect();

    // 全片擦除耗时较长, 使用更大超时 (默认 5s 基础上放宽到 3 倍)
    return waitBusy(m_timeout * 3);
}

/*============================================================================
 * 功耗管理
 *============================================================================*/

void W25QFLASH::powerDown()
{
    select();
    transfer(CMD_POWER_DOWN);
    deselect();
}

void W25QFLASH::powerUp()
{
    select();
    transfer(CMD_RELEASE_POWER_DOWN);
    deselect();
    HAL_Delay(3);   // 唤醒后需 tRES1 (典型 3us~30us), 留足余量
}

} // namespace LoveFinderLib