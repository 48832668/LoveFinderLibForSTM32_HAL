/**
 * @file fonts_subset.cpp
 * @brief 字符级编译字体数据 — 每字符独立数组 + switch 查表
 * @author LoveFinder
 *
 * ============================================================================
 *  为什么这样设计?
 * ============================================================================
 * 传统 fonts.c 把整套 95 字符打包成一个大数组, 链接器无法区分哪些字符
 * 被使用, 全部进入固件。本文件让每个字符成为独立的 static 数组,
 * 只有被 font_get_glyph 引用的字符才会被链接器保留。
 *
 * 开关/字符集由 fonts_config.hpp 控制:
 *   - USE_FONT_SUBSET_7X10   + FONT_SUBSET_7X10_CHARS  -> Font_Subset_7x10
 *   - USE_FONT_SUBSET_ZH_16X16 + FONT_SUBSET_ZH_CHARS  -> Font_Subset_ZH_16x16
 * ============================================================================
 */

#include "fonts_subset.h"
#include "fonts_config.hpp"

/*============================================================================
 * Font_Subset_7x10 — 仅编译 FONT_SUBSET_7X10_CHARS 中的字符
 *==========================================================================*/
#if USE_FONT_SUBSET_7X10

// --- 每个字符一个独立数组 (字符级编译的核心) ---

static const uint16_t g7_space[10] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t g7_excl[10] = {
    0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
    0x1000, 0x0000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_0[10] = {
    0x3800, 0x4400, 0x4400, 0x5400, 0x4400,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_1[10] = {
    0x1000, 0x3000, 0x5000, 0x1000, 0x1000,
    0x1000, 0x1000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_2[10] = {
    0x3800, 0x4400, 0x4400, 0x0400, 0x0800,
    0x1000, 0x2000, 0x7C00, 0x0000, 0x0000,
};

static const uint16_t g7_3[10] = {
    0x3800, 0x4400, 0x0400, 0x1800, 0x0400,
    0x0400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_4[10] = {
    0x0800, 0x1800, 0x2800, 0x2800, 0x4800,
    0x7C00, 0x0800, 0x0800, 0x0000, 0x0000,
};

static const uint16_t g7_5[10] = {
    0x7C00, 0x4000, 0x4000, 0x7800, 0x0400,
    0x0400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_6[10] = {
    0x3800, 0x4400, 0x4000, 0x7800, 0x4400,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_7[10] = {
    0x7C00, 0x0400, 0x0800, 0x1000, 0x1000,
    0x2000, 0x2000, 0x2000, 0x0000, 0x0000,
};

static const uint16_t g7_8[10] = {
    0x3800, 0x4400, 0x4400, 0x3800, 0x4400,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_9[10] = {
    0x3800, 0x4400, 0x4400, 0x4400, 0x3C00,
    0x0400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_colon[10] = {
    0x0000, 0x0000, 0x1000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_A[10] = {
    0x1000, 0x2800, 0x2800, 0x2800, 0x2800,
    0x7C00, 0x4400, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_B[10] = {
    0x7800, 0x4400, 0x4400, 0x7800, 0x4400,
    0x4400, 0x4400, 0x7800, 0x0000, 0x0000,
};

static const uint16_t g7_C[10] = {
    0x3800, 0x4400, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_D[10] = {
    0x7000, 0x4800, 0x4400, 0x4400, 0x4400,
    0x4400, 0x4800, 0x7000, 0x0000, 0x0000,
};

static const uint16_t g7_E[10] = {
    0x7C00, 0x4000, 0x4000, 0x7C00, 0x4000,
    0x4000, 0x4000, 0x7C00, 0x0000, 0x0000,
};

static const uint16_t g7_F[10] = {
    0x7C00, 0x4000, 0x4000, 0x7800, 0x4000,
    0x4000, 0x4000, 0x4000, 0x0000, 0x0000,
};

static const uint16_t g7_G[10] = {
    0x3800, 0x4400, 0x4000, 0x4000, 0x5C00,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_H[10] = {
    0x4400, 0x4400, 0x4400, 0x7C00, 0x4400,
    0x4400, 0x4400, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_I[10] = {
    0x3800, 0x1000, 0x1000, 0x1000, 0x1000,
    0x1000, 0x1000, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_J[10] = {
    0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
    0x0400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_K[10] = {
    0x4400, 0x4800, 0x5000, 0x6000, 0x5000,
    0x4800, 0x4800, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_L[10] = {
    0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4000, 0x7C00, 0x0000, 0x0000,
};

static const uint16_t g7_M[10] = {
    0x4400, 0x6C00, 0x6C00, 0x5400, 0x4400,
    0x4400, 0x4400, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_N[10] = {
    0x4400, 0x6400, 0x6400, 0x5400, 0x5400,
    0x4C00, 0x4C00, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_O[10] = {
    0x3800, 0x4400, 0x4400, 0x4400, 0x4400,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_P[10] = {
    0x7800, 0x4400, 0x4400, 0x4400, 0x7800,
    0x4000, 0x4000, 0x4000, 0x0000, 0x0000,
};

static const uint16_t g7_Q[10] = {
    0x3800, 0x4400, 0x4400, 0x4400, 0x4400,
    0x4400, 0x5400, 0x3800, 0x0400, 0x0000,
};

static const uint16_t g7_R[10] = {
    0x7800, 0x4400, 0x4400, 0x4400, 0x7800,
    0x4800, 0x4800, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_S[10] = {
    0x3800, 0x4400, 0x4000, 0x3000, 0x0800,
    0x0400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_T[10] = {
    0x7C00, 0x1000, 0x1000, 0x1000, 0x1000,
    0x1000, 0x1000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_U[10] = {
    0x4400, 0x4400, 0x4400, 0x4400, 0x4400,
    0x4400, 0x4400, 0x3800, 0x0000, 0x0000,
};

static const uint16_t g7_V[10] = {
    0x4400, 0x4400, 0x4400, 0x2800, 0x2800,
    0x2800, 0x1000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_W[10] = {
    0x4400, 0x4400, 0x5400, 0x5400, 0x5400,
    0x6C00, 0x2800, 0x2800, 0x0000, 0x0000,
};

static const uint16_t g7_X[10] = {
    0x4400, 0x2800, 0x2800, 0x1000, 0x1000,
    0x2800, 0x2800, 0x4400, 0x0000, 0x0000,
};

static const uint16_t g7_Y[10] = {
    0x4400, 0x4400, 0x2800, 0x2800, 0x1000,
    0x1000, 0x1000, 0x1000, 0x0000, 0x0000,
};

static const uint16_t g7_Z[10] = {
    0x7C00, 0x0400, 0x0800, 0x1000, 0x1000,
    0x2000, 0x4000, 0x7C00, 0x0000, 0x0000,
};

// --- 查表: 只包含 FONT_SUBSET_7X10_CHARS 中的字符 ---

const uint16_t* subset_glyph_7x10(uint8_t ch) {
    switch (ch) {
        case ' ': return g7_space;
        case '!': return g7_excl;
        case '0': return g7_0;
        case '1': return g7_1;
        case '2': return g7_2;
        case '3': return g7_3;
        case '4': return g7_4;
        case '5': return g7_5;
        case '6': return g7_6;
        case '7': return g7_7;
        case '8': return g7_8;
        case '9': return g7_9;
        case ':': return g7_colon;
        case 'A': return g7_A;
        case 'B': return g7_B;
        case 'C': return g7_C;
        case 'D': return g7_D;
        case 'E': return g7_E;
        case 'F': return g7_F;
        case 'G': return g7_G;
        case 'H': return g7_H;
        case 'I': return g7_I;
        case 'J': return g7_J;
        case 'K': return g7_K;
        case 'L': return g7_L;
        case 'M': return g7_M;
        case 'N': return g7_N;
        case 'O': return g7_O;
        case 'P': return g7_P;
        case 'Q': return g7_Q;
        case 'R': return g7_R;
        case 'S': return g7_S;
        case 'T': return g7_T;
        case 'U': return g7_U;
        case 'V': return g7_V;
        case 'W': return g7_W;
        case 'X': return g7_X;
        case 'Y': return g7_Y;
        case 'Z': return g7_Z;
        default:  return nullptr;   // 未编译的字符 -> 渲染端跳过
    }
}

FontDef Font_Subset_7x10 = {7, 10, nullptr};

#endif /* USE_FONT_SUBSET_7X10 */

/*============================================================================
 * Font_Subset_ZH_16x16 — 仅编译 FONT_SUBSET_ZH_CHARS 中的汉字
 *==========================================================================*/
#if USE_FONT_SUBSET_ZH_16X16

// --- 每个汉字一个独立数组 ---

// U+4F60 你 (16x16, 黑体, 由 tools/font_generator.py 生成, bit15=最左列)
static const uint16_t gzh_4f60[16] = {
    0x0000, 0x0880, 0x1980, 0x11FC,
    0x33FE, 0x3204, 0x7664, 0x5060,
    0x1168, 0x116C, 0x1364, 0x1666,
    0x1462, 0x1060, 0x10E0, 0x1080,
};

// U+597D 好 (16x16, 黑体, 由 tools/font_generator.py 生成, bit15=最左列)
static const uint16_t gzh_597d[16] = {
    0x1800, 0x18FC, 0x10FE, 0x7E0C,
    0x7E18, 0x3230, 0x2630, 0x25FE,
    0x2410, 0x1C10, 0x0C10, 0x1E10,
    0x3230, 0x4070, 0x0040, 0x0000,
};

// --- 查表: 只包含 FONT_SUBSET_ZH_CHARS 中的汉字 ---

const uint16_t* subset_glyph_zh_16x16(uint16_t uni) {
    switch (uni) {
        case 0x4F60: return gzh_4f60;   // 你
        case 0x597D: return gzh_597d;   // 好
        default:     return nullptr;    // 未编译 -> 渲染端跳过
    }
}

FontDef Font_Subset_ZH_16x16 = {16, 16, nullptr};

#endif /* USE_FONT_SUBSET_ZH_16X16 */

/*============================================================================
 * 通用字形分发 — 供 st7735.cpp 渲染端调用
 * (FontDef.data==nullptr 时走查表; 指针比较识别具体字体)
 *==========================================================================*/

const uint16_t* font_get_glyph(const FontDef& font, uint8_t ch) {
#if USE_FONT_SUBSET_7X10
    if (&font == &Font_Subset_7x10) return subset_glyph_7x10(ch);
#endif
    return nullptr;
}

const uint16_t* font_get_glyph_unicode(const FontDef& font, uint16_t uni) {
    if (uni <= 127) {
        return font_get_glyph(font, static_cast<uint8_t>(uni));
    }
#if USE_FONT_SUBSET_ZH_16X16
    if (&font == &Font_Subset_ZH_16x16) return subset_glyph_zh_16x16(uni);
#endif
    return nullptr;
}