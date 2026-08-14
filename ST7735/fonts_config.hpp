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
 * 本库支持"字符级编译"(双模式字库):
 *   - 模式 A (传统完整字库): font.data != nullptr -> 整套字库固定索引
 *   - 模式 B (字符级子集):   font.data == nullptr -> font_get_glyph 查表
 *     仅编译 FONT_SUBSET_*_CHARS 中声明的字符, 未编译字符渲染时自动跳过
 *
 * 示例:
 *   - 只需要开机欢迎语 "你好"  -> 启用 USE_FONT_SUBSET_ZH_16X16,
 *     字库只包含 你(U+4F60) 好(U+597D) 两个字模 (~64 字节)
 *   - 只需要显示 "A" 和 "!"   -> 在 FONT_SUBSET_7X10_CHARS 中只写 "A!"
 *
 * ============================================================================
 *  如何使用 (三步)
 * ============================================================================
 * 1. 按需打开/关闭字体开关 (USE_FONT_*)
 * 2. 通过 FONT_SUBSET_*_CHARS 宏声明需要编译的字符集
 * 3. 渲染端自动工作: WriteChar/WriteString 遇到未编译字符跳过, 不影响其他字符
 *
 * 添加新字符 (如再加一个汉字):
 *   a. 生成字模: Python+PIL 渲染 16x16, 每行一个 uint16_t, bit15=最左列
 *   b. 在 fonts_subset.cpp 加 static const uint16_t 数组 + 查表 case
 *   c. 在下面 FONT_SUBSET_ZH_CHARS 声明该字符码点
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