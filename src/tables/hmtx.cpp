//
// FontyDE — Decompiler Edition
// src/tables/hmtx.cpp
//

#include "../fontyde/tables/hmtx.h"

namespace fontyde {

HmtxTable HmtxTable::parse(BinaryReader& r, u16 num_h_metrics, u16 num_glyphs) {
    HmtxTable t;
    t.h_metrics.resize(num_h_metrics);
    for (auto& m : t.h_metrics) {
        m.advance_width = r.read_u16();
        m.lsb           = r.read_i16();
    }
    u16 remaining = num_glyphs > num_h_metrics ? num_glyphs - num_h_metrics : 0;
    t.left_side_bearings.resize(remaining);
    for (auto& lsb : t.left_side_bearings) lsb = r.read_i16();
    return t;
}

} // namespace fontyde
