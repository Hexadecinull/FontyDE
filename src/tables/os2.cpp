//
// FontyDE — Decompiler Edition
// src/tables/os2.cpp
//

#include "../fontyde/tables/os2.h"

namespace fontyde {

Os2Table Os2Table::parse(BinaryReader& r) {
    Os2Table t = {};
    t.version              = r.read_u16();
    t.x_avg_char_width     = r.read_i16();
    t.us_weight_class      = r.read_u16();
    t.us_width_class       = r.read_u16();
    t.fs_type              = r.read_u16();
    t.y_subscript_x_size   = r.read_i16();
    t.y_subscript_y_size   = r.read_i16();
    t.y_subscript_x_offset = r.read_i16();
    t.y_subscript_y_offset = r.read_i16();
    t.y_superscript_x_size   = r.read_i16();
    t.y_superscript_y_size   = r.read_i16();
    t.y_superscript_x_offset = r.read_i16();
    t.y_superscript_y_offset = r.read_i16();
    t.y_strikeout_size     = r.read_i16();
    t.y_strikeout_position = r.read_i16();
    t.s_family_class       = r.read_i16();
    for (int i = 0; i < 10; i++) t.panose[i] = r.read_u8();
    for (int i = 0; i < 4; i++) t.ul_unicode_range[i] = r.read_u32();
    for (int i = 0; i < 4; i++) t.ach_vend_id[i] = static_cast<char>(r.read_u8());
    t.fs_selection         = r.read_u16();
    t.us_first_char_index  = r.read_u16();
    t.us_last_char_index   = r.read_u16();
    t.s_typo_ascender      = r.read_i16();
    t.s_typo_descender     = r.read_i16();
    t.s_typo_line_gap      = r.read_i16();
    t.us_win_ascent        = r.read_u16();
    t.us_win_descent       = r.read_u16();
    if (t.version >= 1) {
        t.ul_code_page_range[0] = r.read_u32();
        t.ul_code_page_range[1] = r.read_u32();
    }
    if (t.version >= 2) {
        t.sx_height        = r.read_i16();
        t.s_cap_height     = r.read_i16();
        t.us_default_char  = r.read_u16();
        t.us_break_char    = r.read_u16();
        t.us_max_context   = r.read_u16();
    }
    return t;
}

std::string Os2Table::weight_class_name() const {
    if (us_weight_class <= 100)  return "Thin (100)";
    if (us_weight_class <= 200)  return "ExtraLight (200)";
    if (us_weight_class <= 300)  return "Light (300)";
    if (us_weight_class <= 400)  return "Regular (400)";
    if (us_weight_class <= 500)  return "Medium (500)";
    if (us_weight_class <= 600)  return "SemiBold (600)";
    if (us_weight_class <= 700)  return "Bold (700)";
    if (us_weight_class <= 800)  return "ExtraBold (800)";
    return "Black (900)";
}

std::string Os2Table::width_class_name() const {
    switch (us_width_class) {
        case 1: return "Ultra-Condensed";
        case 2: return "Extra-Condensed";
        case 3: return "Condensed";
        case 4: return "Semi-Condensed";
        case 5: return "Medium (Normal)";
        case 6: return "Semi-Expanded";
        case 7: return "Expanded";
        case 8: return "Extra-Expanded";
        case 9: return "Ultra-Expanded";
        default: return "Unknown";
    }
}

} // namespace fontyde
