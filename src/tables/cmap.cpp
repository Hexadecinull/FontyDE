//
// FontyDE — Decompiler Edition
// src/tables/cmap.cpp
//

#include "fontyde/tables/cmap.h"

namespace fontyde {

static CmapSubtable parse_format4(BinaryReader& r, u16 platform_id, u16 encoding_id) {
    CmapSubtable st;
    st.format      = 4;
    st.platform_id = platform_id;
    st.encoding_id = encoding_id;

    u16 length       = r.read_u16();
    u16 language     = r.read_u16();
    u16 seg_count2   = r.read_u16();
    u16 seg_count    = seg_count2 / 2;
    r.skip(6);

    std::vector<u16> end_codes(seg_count);
    for (auto& e : end_codes) e = r.read_u16();
    r.skip(2);
    std::vector<u16> start_codes(seg_count);
    for (auto& s : start_codes) s = r.read_u16();
    std::vector<i16> id_deltas(seg_count);
    for (auto& d : id_deltas) d = r.read_i16();
    std::vector<u16> id_range_offsets(seg_count);
    size_t range_offset_base = r.pos();
    for (auto& ro : id_range_offsets) ro = r.read_u16();

    for (u16 i = 0; i < seg_count; i++) {
        if (start_codes[i] == 0xFFFF) break;
        CmapSegment seg;
        seg.start_code       = start_codes[i];
        seg.end_code         = end_codes[i];
        seg.id_delta         = id_deltas[i];
        seg.id_range_offset  = id_range_offsets[i];
        st.segments.push_back(seg);

        if (id_range_offsets[i] == 0) {
            for (u16 cp = start_codes[i]; cp <= end_codes[i]; cp++) {
                u16 gid = static_cast<u16>(cp + id_deltas[i]);
                if (gid != 0) st.mapping[cp] = gid;
            }
        } else {
            for (u16 cp = start_codes[i]; cp <= end_codes[i]; cp++) {
                size_t idx = range_offset_base + (i * 2) + id_range_offsets[i] + (cp - start_codes[i]) * 2;
                if (idx + 2 <= r.size()) {
                    u16 gid = (static_cast<u16>(r.raw_at(idx)[0]) << 8) | r.raw_at(idx)[1];
                    if (gid != 0) {
                        gid = static_cast<u16>(gid + id_deltas[i]);
                        st.mapping[cp] = gid;
                    }
                }
            }
        }
    }
    (void)length; (void)language;
    return st;
}

static CmapSubtable parse_format12(BinaryReader& r, u16 platform_id, u16 encoding_id) {
    CmapSubtable st;
    st.format      = 12;
    st.platform_id = platform_id;
    st.encoding_id = encoding_id;
    r.skip(2);
    u32 length    = r.read_u32();
    u32 language  = r.read_u32();
    u32 num_groups = r.read_u32();
    for (u32 i = 0; i < num_groups; i++) {
        u32 start_cp  = r.read_u32();
        u32 end_cp    = r.read_u32();
        u32 start_gid = r.read_u32();
        for (u32 cp = start_cp; cp <= end_cp; cp++) {
            st.mapping[cp] = static_cast<u16>(start_gid + (cp - start_cp));
        }
    }
    (void)length; (void)language;
    return st;
}

static CmapSubtable parse_format0(BinaryReader& r, u16 platform_id, u16 encoding_id) {
    CmapSubtable st;
    st.format      = 0;
    st.platform_id = platform_id;
    st.encoding_id = encoding_id;
    u16 length   = r.read_u16();
    u16 language = r.read_u16();
    for (u16 cp = 0; cp < 256; cp++) {
        u8 gid = r.read_u8();
        if (gid != 0) st.mapping[cp] = gid;
    }
    (void)length; (void)language;
    return st;
}

static CmapSubtable parse_format6(BinaryReader& r, u16 platform_id, u16 encoding_id) {
    CmapSubtable st;
    st.format      = 6;
    st.platform_id = platform_id;
    st.encoding_id = encoding_id;
    u16 length      = r.read_u16();
    u16 language    = r.read_u16();
    u16 first_code  = r.read_u16();
    u16 entry_count = r.read_u16();
    for (u16 i = 0; i < entry_count; i++) {
        u16 gid = r.read_u16();
        if (gid != 0) st.mapping[first_code + i] = gid;
    }
    (void)length; (void)language;
    return st;
}

CmapTable CmapTable::parse(BinaryReader& r) {
    CmapTable t;
    size_t table_start = r.pos();

    t.version    = r.read_u16();
    t.num_tables = r.read_u16();

    t.encodings.resize(t.num_tables);
    for (auto& enc : t.encodings) {
        enc.platform_id = r.read_u16();
        enc.encoding_id = r.read_u16();
        enc.offset      = r.read_u32();
    }

    for (auto& enc : t.encodings) {
        size_t st_pos = table_start + enc.offset;
        if (st_pos + 2 > r.size()) continue;
        u16 format = (static_cast<u16>(r.raw_at(st_pos)[0]) << 8) | r.raw_at(st_pos)[1];

        BinaryReader sr = r.sub_reader(st_pos + 2, r.size() - st_pos - 2);
        CmapSubtable st;
        try {
            if      (format == 0)  st = parse_format0(sr, enc.platform_id, enc.encoding_id);
            else if (format == 4)  st = parse_format4(sr, enc.platform_id, enc.encoding_id);
            else if (format == 6)  st = parse_format6(sr, enc.platform_id, enc.encoding_id);
            else if (format == 12) st = parse_format12(sr, enc.platform_id, enc.encoding_id);
            else continue;
            t.subtables.push_back(std::move(st));
        } catch (...) {}
    }
    return t;
}

u16 CmapTable::glyph_id_for(u32 codepoint) const {
    for (auto& st : subtables) {
        if (st.platform_id == 3 || st.platform_id == 0) {
            auto it = st.mapping.find(codepoint);
            if (it != st.mapping.end()) return it->second;
        }
    }
    for (auto& st : subtables) {
        auto it = st.mapping.find(codepoint);
        if (it != st.mapping.end()) return it->second;
    }
    return 0;
}

} // namespace fontyde
