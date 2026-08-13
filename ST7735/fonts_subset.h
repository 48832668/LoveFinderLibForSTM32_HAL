/**
 * @file fonts_subset.h
 * @brief 字符级编译字体 — 每字符独立字模 + 查表接口
 * @author LoveFinder
 *
 * 与 fonts.h (传统整套字库) 的区别:
 *   - 传统: FontDef.data 指向完整 95 字符数组, 固定索引 (ch-32)*height
 *   - 本文件: FontDef.data == nullptr, 通过 font_get_glyph() 查表,
 *     只编译 FONT_SUBSET_7X10_CHARS / FONT_SUBSET_ZH_CHARS 中的字符
 */

#ifndef FONTS_SUBSET_H
#define FONTS_SUBSET_H

#include <stdint.h>
#include "fonts.h"
#include "fonts_config.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// 字符级编译子集字体 (data == nullptr, 走查表)
#if USE_FONT_SUBSET_7X10
extern FontDef Font_Subset_7x10;
#endif

#if USE_FONT_SUBSET_ZH_16X16
extern FontDef Font_Subset_ZH_16x16;
#endif

/**
 * @brief 按 ASCII 码查字模 (子集字体专用)
 * @param ch ASCII 码 (32-126)
 * @return 指向 height 个 uint16_t 的指针; 字符未编译返回 nullptr
 */
#if USE_FONT_SUBSET_7X10
const uint16_t* subset_glyph_7x10(uint8_t ch);
#endif

/**
 * @brief 按 Unicode 查汉字字模 (子集字体专用)
 * @param uni Unicode 码点 (如 0x4F60=你, 0x597D=好)
 * @return 指向 height 个 uint16_t 的指针; 未编译返回 nullptr
 */
#if USE_FONT_SUBSET_ZH_16X16
const uint16_t* subset_glyph_zh_16x16(uint16_t uni);
#endif

#ifdef __cplusplus
}

// 以下为 C++ 专用接口 (依赖 FontDef 引用比较, 供 st7735.cpp 渲染端调用)

/**
 * @brief 通用字形分发: 按 ASCII 查任意子集字体的字模
 * @param font 字体定义 (data==nullptr 时有效)
 * @param ch    ASCII 码 (32-126)
 * @return 指向 height 个 uint16_t 的指针; 字符未编译返回 nullptr
 */
const uint16_t* font_get_glyph(const FontDef& font, uint8_t ch);

/**
 * @brief 通用字形分发: 按 Unicode 查任意子集字体的字模
 * @param font 字体定义 (data==nullptr 时有效)
 * @param uni   Unicode 码点 (如 0x4F60=你, 0x597D=好)
 * @return 指向 height 个 uint16_t 的指针; 字符未编译返回 nullptr
 */
const uint16_t* font_get_glyph_unicode(const FontDef& font, uint16_t uni);

#endif // __cplusplus

#endif // FONTS_SUBSET_H