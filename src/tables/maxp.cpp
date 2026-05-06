//
// FontyDE — Decompiler Edition
// src/tables/maxp.cpp
//

#include "fontyde/tables/maxp.h"

namespace fontyde {

MaxpTable MaxpTable::parse(BinaryReader& r) {
    MaxpTable t = {};
    t.version    = r.read_fixed();
    t.num_glyphs = r.read_u16();

    if (t.version == 0x00010000) {
        t.max_points              = r.read_u16();
        t.max_contours            = r.read_u16();
        t.max_composite_points    = r.read_u16();
        t.max_composite_contours  = r.read_u16();
        t.max_zones               = r.read_u16();
        t.max_twilight_points     = r.read_u16();
        t.max_storage             = r.read_u16();
        t.max_function_defs       = r.read_u16();
        t.max_instruction_defs    = r.read_u16();
        t.max_stack_elements      = r.read_u16();
        t.max_size_of_instructions = r.read_u16();
        t.max_component_elements  = r.read_u16();
        t.max_component_depth     = r.read_u16();
    }
    return t;
}

} // namespace fontyde
