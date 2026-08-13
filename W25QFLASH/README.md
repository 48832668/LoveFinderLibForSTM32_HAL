# W25QFLASH (LoveFinderLib)

W25Q 系列 SPI Flash 驱动库，支持 **W25Q16 / W25Q32 / W25Q64 / W25Q128 / W25Q256 / W25Q512** 全系列，自动识别型号，自动适配地址宽度。

## 特性

- **型号自动识别**：通过 JEDEC ID (0x9F) 读取厂商/类型/容量字节，查表返回精确型号（`identify()`）。
- **全系列支持**：W25Q16 (2MB) ~ W25Q512 (64MB)。
- **自动地址宽度**：W25Q16~W25Q128 使用 24 位地址；W25Q256/W25Q512 自动切换 4 字节地址扩展命令（读 0x13 / 写 0x12 / 擦除 0x21/0x5C/0xDC），**无需修改芯片模式寄存器**，掉电唤醒后无需重新配置。
- **SPI 传输优化**：<32B 阻塞传输，≥32B DMA 传输（与旧 W25Q128 库一致）。
- **完整功能**：读 / 页编程 / 跨页连续写 / 4K 扇区擦除 / 32K 块擦除 / 64K 块擦除 / 全片擦除 / 掉电唤醒。
- **纯 C++17**，`LoveFinderLib` 命名空间，风格与 INA226/ENCODER/BUTTON 库一致。

## 使用示例

```cpp
#include "W25QFLASH.hpp"
using namespace LoveFinderLib;

// 全局对象
W25QFLASH flash;

// 初始化 (SPI2, CS=PB12)
flash.init(&hspi2, FLASH_CS_GPIO_Port, FLASH_CS_Pin);

// 识别型号 (检测程序核心)
e_W25QFLASH_Model model = flash.identify();
if (model != e_W25QFLASH_Model::UNKNOWN)
{
    printf("Flash: %s, %lu bytes\n", flash.getModelName(), flash.getCapacityBytes());
}

// 读写示例
uint8_t buf[256];
flash.eraseSector(0x000000);          // 先擦除 4KB 扇区
flash.write(0x000000, buf, 256);      // 连续写 (自动跨页)
flash.read(0x000000, buf, 256);       // 读回
```

## 配置 (W25QFLASH_Config)

| 字段 | 默认 | 说明 |
|------|------|------|
| `spi` | `nullptr` | SPI 外设句柄 (需用户赋值) |
| `csPort` | `nullptr` | CS 引脚 GPIO 端口 (需用户赋值) |
| `csPin` | `0` | CS 引脚编号 (需用户赋值) |
| `timeout` | `5000` | 忙等待超时 (ms) |

## 型号识别表

| JEDEC 容量字节 | 型号 | 容量 | 地址宽度 |
|---------------|------|------|---------|
| 0x15 | W25Q16 | 2MB | 24 位 |
| 0x16 | W25Q32 | 4MB | 24 位 |
| 0x17 | W25Q64 | 8MB | 24 位 |
| 0x18 | W25Q128 | 16MB | 24 位 |
| 0x19 | W25Q256 | 32MB | 32 位 |
| 0x20 | W25Q512 | 64MB | 32 位 |

厂商 ID = 0xEF (Winbond)。

## API 一览

| 方法 | 说明 |
|------|------|
| `init(spi, csPort, csPin)` / `init(config)` | 初始化 |
| `identify()` | 识别型号 (返回 `e_W25QFLASH_Model`) |
| `getModelName()` | 型号字符串 |
| `getCapacityBytes()` | 容量 (字节) |
| `isOnline()` | 是否在线 (识别成功) |
| `readJEDECID(...)` | 读原始 JEDEC ID |
| `readStatusReg(reg)` / `waitBusy(timeout)` | 状态 |
| `writeEnable()` / `writeDisable()` | 写使能/禁用 |
| `read(addr, data, len)` | 读数据 |
| `pageProgram(addr, data, len)` | 页编程 (≤256B) |
| `write(addr, data, len)` | 连续写 (自动跨页) |
| `eraseSector(addr)` | 4KB 扇区擦除 |
| `eraseBlock32K(addr)` | 32KB 块擦除 |
| `eraseBlock64K(addr)` | 64KB 块擦除 |
| `eraseChip()` | 全片擦除 |
| `powerDown()` / `powerUp()` | 掉电/唤醒 |

## 平台移植

头文件顶部 `PORT` 注释块内只有一行 include（`stm32f4xx_hal.h`），移植到其他 STM32 系列时替换为对应 HAL 头文件即可，其余代码无需改动。

## 许可证

MIT