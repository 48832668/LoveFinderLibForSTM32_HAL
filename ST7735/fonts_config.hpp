/**
 * @file fonts_config.hpp
 * @brief 字体编译配置 — 字符级编译 (Per-Character Compilation)
 * @author LoveFinder
 *
 * ============================================================================
 *  什么是字符级编译?
 * ============================================================================
 * 传统字库把整套 95 个 ASCII 字符(或整片中文字库)全部编译进固件,
 * 即使你只用其中几个字符, 也会白白占用大量 Flash。
 *
 * 本库支持"字符级编译": 通过下面的开关, 你可以精确控制编译哪些字符。
 * 每个字符是独立的 static constexpr 数组, 未启用的字符根本不会进入
 * 固件 (链接器会自动丢弃未引用数据)。
 *
 * 示例:
 *   - 只需要开机欢迎语 "你好"  -> 启用 USE_FONT_SUBSET_ZH_16X16,
 *     字库只包含 你(U+4F60) 好(U+597D) 两个字模 (~64 字节)
 *   - 只需要显示 "A" 和 "!"   -> 在 SUBSET_7X10_CHARS 中只写 "A!"
 *
 * ============================================================================
 *  如何使用
 * ============================================================================
 * 1. 在下面按需打开/关闭字体开关
 * 2. 通过 FONT_SUBSET_*_CHARS 宏声明该字体需要编译的字符集
 * 3. 在 fonts_subset.cpp 中为字符集内每个字符保留独立字模数组 +
 *    switch 查表分支 (未启用的字符删除对应数组与 case, 链接器即丢弃)
 * 4. 渲染端: ST7735_WriteChar/WriteString 遇到未编译字符自动跳过,
 *    不影响其他字符显示
 * ============================================================================
 */

#ifndef FONTS_CONFIG_HPP
#define FONTS_CONFIG_HPP

// ------------------------------------------------------------------
// 完整字库开关 (传统模式, 整套 95 字符 ASCII, 与旧代码完全兼容)
// 若旧代码仍在使用 Font_7x10 / Font_11x18 / Font_16x26, 保持 1
// ------------------------------------------------------------------
#ifndef USE_FONT_FULL_7X10
#define USE_FONT_FULL_7X10      1
#endif

#ifndef USE_FONT_FULL_11X18
#define USE_FONT_FULL_11X18     1
#endif

#ifndef USE_FONT_FULL_16X26
#define USE_FONT_FULL_16X26     1
#endif

// ------------------------------------------------------------------
// 字符级编译子集字体 (推荐)
// Font_Subset_7x10  : 仅编译 FONT_SUBSET_7X10_CHARS 中的字符
// Font_Subset_ZH_16x16 : 仅编译 FONT_SUBSET_ZH_CHARS 中的汉字
// ------------------------------------------------------------------
#ifndef USE_FONT_SUBSET_7X10
#define USE_FONT_SUBSET_7X10    1
#endif

#ifndef USE_FONT_SUBSET_ZH_16X16
#define USE_FONT_SUBSET_ZH_16X16 1
#endif

// ------------------------------------------------------------------
// 子集字符集定义
// 仅这些字符会被编译进固件! 其余字符渲染时自动跳过。
// 注意: 字符集请保持 ASCII 升序, 便于生成器维护。
// ------------------------------------------------------------------
#ifndef FONT_SUBSET_7X10_CHARS
#define FONT_SUBSET_7X10_CHARS  " !0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#endif

// 中文字符集: 用 UTF-8 转义序列声明 (避免源文件编码问题)
// 默认: 你(U+4F60) 好(U+597D) —— 开机欢迎语
#ifndef FONT_SUBSET_ZH_CHARS
#define FONT_SUBSET_ZH_CHARS    "\xE4\xBD\xA0\xE5\xA5\xBD"   // "你好"
#endif

#endif // FONTS_CONFIG_HPP