//
// FontyDE — Decompiler Edition
// src/formats/woff.cpp
//

#include "fontyde/formats/woff.h"
#include "fontyde/formats/ttf.h"
#include "fontyde/reader.h"
#include <zlib.h>
#include <cstring>

namespace fontyde {

static std::vector<u8> zlib_inflate(const u8* src, size_t src_len, size_t expected_len) {
    std::vector<u8> out(expected_len);
    uLongf dest_len = static_cast<uLongf>(expected_len);
    int result = uncompress(out.data(), &dest_len, src, static_cast<uLong>(src_len));
    if (result != Z_OK) throw ParseError("WOFF: zlib inflate failed (code " + std::to_string(result) + ")");
    out.resize(dest_len);
    return out;
}

static u32 calc_checksum(const u8* data, u32 length) {
    u32 sum = 0;
    u32 nlongs = (length + 3) / 4;
    for (u32 i = 0; i < nlongs; i++) {
        u32 val = 0;
        size_t off = i * 4;
        if (off + 4 <= length) {
            memcpy(&val, data + off, 4);
            val = ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) |
                  (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
        } else {
            for (size_t j = 0; j < 4 && off + j < length; j++)
                val |= static_cast<u32>(data[off + j]) << (24 - j * 8);
        }
        sum += val;
    }
    return sum;
}

std::vector<u8> unwrap_woff(const std::vector<u8>& data) {
    BinaryReader r(data);

    WoffHeader hdr;
    hdr.signature        = r.read_u32();
    hdr.flavor           = r.read_u32();
    hdr.length           = r.read_u32();
    hdr.num_tables       = r.read_u16();
    hdr.reserved         = r.read_u16();
    hdr.total_sfnt_size  = r.read_u32();
    hdr.major_version    = r.read_u16();
    hdr.minor_version    = r.read_u16();
    hdr.meta_offset      = r.read_u32();
    hdr.meta_length      = r.read_u32();
    hdr.meta_orig_length = r.read_u32();
    hdr.priv_offset      = r.read_u32();
    hdr.priv_length      = r.read_u32();

    if (hdr.signature != 0x774F4646) throw ParseError("WOFF: invalid signature");
    if (hdr.reserved  != 0)          throw ParseError("WOFF: reserved field must be zero");

    std::vector<WoffTableEntry> entries(hdr.num_tables);
    for (auto& e : entries) {
        e.tag           = r.read_tag();
        e.offset        = r.read_u32();
        e.comp_length   = r.read_u32();
        e.orig_length   = r.read_u32();
        e.orig_checksum = r.read_u32();
    }

    u32 sfnt_offset = 12 + hdr.num_tables * 16;
    u32 search_range   = 1;
    u32 entry_selector = 0;
    while (search_range * 2 <= hdr.num_tables) { search_range *= 2; entry_selector++; }
    search_range  *= 16;
    u32 range_shift = hdr.num_tables * 16 - search_range;

    std::vector<u8> sfnt;
    sfnt.resize(hdr.total_sfnt_size, 0);

    u8* out = sfnt.data();
    auto write_u32 = [&](size_t pos, u32 val) {
        out[pos]   = (val >> 24) & 0xFF;
        out[pos+1] = (val >> 16) & 0xFF;
        out[pos+2] = (val >>  8) & 0xFF;
        out[pos+3] = (val >>  0) & 0xFF;
    };
    auto write_u16 = [&](size_t pos, u16 val) {
        out[pos]   = (val >> 8) & 0xFF;
        out[pos+1] = (val >> 0) & 0xFF;
    };

    write_u32(0, hdr.flavor);
    write_u16(4, hdr.num_tables);
    write_u16(6, static_cast<u16>(search_range));
    write_u16(8, static_cast<u16>(entry_selector));
    write_u16(10, static_cast<u16>(range_shift));

    u32 table_dir_offset = 12;
    u32 table_data_offset = sfnt_offset;

    for (auto& e : entries) {
        write_u32(table_dir_offset,      0);
        write_u32(table_dir_offset +  4, e.orig_checksum);
        write_u32(table_dir_offset +  8, table_data_offset);
        write_u32(table_dir_offset + 12, e.orig_length);
        memcpy(out + table_dir_offset, e.tag.c_str(), 4);
        table_dir_offset += 16;

        if (e.comp_length < e.orig_length) {
            auto inflated = zlib_inflate(data.data() + e.offset, e.comp_length, e.orig_length);
            memcpy(out + table_data_offset, inflated.data(), inflated.size());
        } else {
            if (e.offset + e.orig_length > data.size())
                throw ParseError("WOFF: table data out of bounds");
            memcpy(out + table_data_offset, data.data() + e.offset, e.orig_length);
        }

        table_data_offset += (e.orig_length + 3) & ~3u;
    }

    u32 checksum_adj_pos = 0;
    for (u16 i = 0; i < hdr.num_tables; i++) {
        if (memcmp(sfnt.data() + 12 + i * 16, "head", 4) == 0) {
            checksum_adj_pos = ((sfnt.data() + 12 + i * 16)[8] << 24) |
                               ((sfnt.data() + 12 + i * 16)[9] << 16) |
                               ((sfnt.data() + 12 + i * 16)[10] << 8) |
                               ((sfnt.data() + 12 + i * 16)[11]);
            break;
        }
    }
    if (checksum_adj_pos + 8 <= sfnt.size()) {
        u32 full_cs = calc_checksum(sfnt.data(), static_cast<u32>(sfnt.size()));
        u32 adj = 0xB1B0AFBA - full_cs;
        write_u32(checksum_adj_pos + 8, adj);
    }

    return sfnt;
}

Font parse_woff(const std::vector<u8>& data, const std::string& path) {
    auto sfnt = unwrap_woff(data);
    Font f = parse_sfnt(sfnt, path);
    f.format = FontFormat::WOFF;
    return f;
}

} // namespace fontyde
