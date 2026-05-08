//
// FontyDE — Decompiler Edition
// src/tables/loca.cpp
//

#include "../fontyde/tables/loca.h"

namespace fontyde {

LocaTable LocaTable::parse(BinaryReader& r, u16 num_glyphs, i16 index_to_loc_format) {
    LocaTable t;
    u16 count = num_glyphs + 1;
    t.offsets.resize(count);

    if (index_to_loc_format == 0) {
        for (auto& off : t.offsets) off = static_cast<u32>(r.read_u16()) * 2;
    } else {
        for (auto& off : t.offsets) off = r.read_u32();
    }
    return t;
}

} // namespace fontyde
