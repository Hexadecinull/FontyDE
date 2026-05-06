//
// FontyDE — Decompiler Edition
// src/tables/head.cpp
//

#include "fontyde/tables/head.h"

namespace fontyde {

HeadTable HeadTable::parse(BinaryReader& r) {
    HeadTable t;
    t.major_version        = r.read_u16();
    t.minor_version        = r.read_u16();
    t.font_revision        = r.read_fixed();
    t.checksum_adjustment  = r.read_u32();
    t.magic_number         = r.read_u32();
    t.flags                = r.read_u16();
    t.units_per_em         = r.read_u16();
    t.created              = r.read_longdatetime();
    t.modified             = r.read_longdatetime();
    t.x_min                = r.read_i16();
    t.y_min                = r.read_i16();
    t.x_max                = r.read_i16();
    t.y_max                = r.read_i16();
    t.mac_style            = r.read_u16();
    t.lowest_rec_ppem      = r.read_u16();
    t.font_direction_hint  = r.read_i16();
    t.index_to_loc_format  = r.read_i16();
    t.glyph_data_format    = r.read_i16();
    return t;
}

} // namespace fontyde
