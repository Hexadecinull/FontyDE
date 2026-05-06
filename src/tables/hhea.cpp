//
// FontyDE — Decompiler Edition
// src/tables/hhea.cpp
//

#include "fontyde/tables/hhea.h"

namespace fontyde {

HheaTable HheaTable::parse(BinaryReader& r) {
    HheaTable t;
    t.major_version          = r.read_u16();
    t.minor_version          = r.read_u16();
    t.ascender               = r.read_i16();
    t.descender              = r.read_i16();
    t.line_gap               = r.read_i16();
    t.advance_width_max      = r.read_u16();
    t.min_left_side_bearing  = r.read_i16();
    t.min_right_side_bearing = r.read_i16();
    t.x_max_extent           = r.read_i16();
    t.caret_slope_rise       = r.read_i16();
    t.caret_slope_run        = r.read_i16();
    t.caret_offset           = r.read_i16();
    for (int i = 0; i < 4; i++) t.reserved[i] = r.read_i16();
    t.metric_data_format     = r.read_i16();
    t.number_of_h_metrics    = r.read_u16();
    return t;
}

} // namespace fontyde
