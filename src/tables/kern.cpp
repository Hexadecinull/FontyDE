//
// FontyDE — Decompiler Edition
// src/tables/kern.cpp
//

#include "fontyde/tables/kern.h"

namespace fontyde {

KernTable KernTable::parse(BinaryReader& r) {
    KernTable t;
    t.version      = r.read_u16();
    t.num_subtables = r.read_u16();

    for (u16 i = 0; i < t.num_subtables; i++) {
        KernSubtable st;
        st.version  = r.read_u16();
        st.length   = r.read_u16();
        st.coverage = r.read_u16();

        if (st.format() == 0) {
            u16 n_pairs       = r.read_u16();
            u16 search_range  = r.read_u16();
            u16 entry_selector = r.read_u16();
            u16 range_shift   = r.read_u16();
            st.pairs.resize(n_pairs);
            for (auto& p : st.pairs) {
                p.left  = r.read_u16();
                p.right = r.read_u16();
                p.value = r.read_i16();
            }
            (void)search_range; (void)entry_selector; (void)range_shift;
        }
        t.subtables.push_back(std::move(st));
    }
    return t;
}

} // namespace fontyde
