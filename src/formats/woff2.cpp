//
// FontyDE — Decompiler Edition
// src/formats/woff2.cpp
//
// WOFF2 uses Brotli compression for the font data and a transformed table
// format (spec: https://www.w3.org/TR/WOFF2/).
// Enable at build time with -DFONTYDE_WOFF2=ON (requires libbrotlidec).
//

#include "fontyde/formats/woff2.h"
#include "fontyde/formats/ttf.h"
#include "fontyde/reader.h"

#ifdef FONTYDE_ENABLE_WOFF2
#include <brotli/decode.h>
#endif

namespace fontyde {

#ifdef FONTYDE_ENABLE_WOFF2

static const std::string WOFF2_KNOWN_TAGS[] = {
    "cmap","head","hhea","hmtx","maxp","name","OS/2","post","cvt ","fpgm",
    "glyf","loca","prep","CFF ","VORG","EBDT","EBLC","gasp","hdmx","kern",
    "LTSH","PCLT","VDMX","vhea","vmtx","BASE","GDEF","GPOS","GSUB","EBSC",
    "JSTF","MATH","CBDT","CBLC","COLR","CPAL","SVG ","sbix","acnt","avar",
    "bdat","bloc","bsln","cvar","fdsc","feat","fmtx","fvar","gvar","hsty",
    "just","lcar","mort","morx","opbd","prop","trak","Zapf","Silf","Glat",
    "Gloc","Feat","Sill"
};

static constexpr size_t WOFF2_TAG_COUNT =
    sizeof(WOFF2_KNOWN_TAGS) / sizeof(WOFF2_KNOWN_TAGS[0]);

struct Woff2Header {
    u32 signature;
    u32 flavor;
    u32 length;
    u16 num_tables;
    u16 reserved;
    u32 total_sfnt_size;
    u32 total_compressed_size;
    u16 major_version;
    u16 minor_version;
    u32 meta_offset;
    u32 meta_length;
    u32 meta_orig_length;
    u32 priv_offset;
    u32 priv_length;
};

struct Woff2TableEntry {
    u8          flags;
    std::string tag;
    u32         orig_length;
    u32         transform_length;
};

static u32 read_255_uint16(BinaryReader& r) {
    u8 code = r.read_u8();
    if (code == 253) return r.read_u16();
    if (code == 255) return static_cast<u32>(r.read_u8()) + 253u;
    if (code == 254) return static_cast<u32>(r.read_u8()) + 506u;
    return code;
}

static u64 read_base128(BinaryReader& r) {
    u64 result = 0;
    for (int i = 0; i < 5; i++) {
        u8 b = r.read_u8();
        if (i == 0 && b == 0x80) throw ParseError("WOFF2: base128 leading zero");
        result = (result << 7) | (b & 0x7F);
        if (!(b & 0x80)) return result;
    }
    throw ParseError("WOFF2: base128 overflow");
}

std::vector<u8> unwrap_woff2(const std::vector<u8>& data) {
    BinaryReader r(data);

    Woff2Header hdr;
    hdr.signature              = r.read_u32();
    hdr.flavor                 = r.read_u32();
    hdr.length                 = r.read_u32();
    hdr.num_tables             = r.read_u16();
    hdr.reserved               = r.read_u16();
    hdr.total_sfnt_size        = r.read_u32();
    hdr.total_compressed_size  = r.read_u32();
    hdr.major_version          = r.read_u16();
    hdr.minor_version          = r.read_u16();
    hdr.meta_offset            = r.read_u32();
    hdr.meta_length            = r.read_u32();
    hdr.meta_orig_length       = r.read_u32();
    hdr.priv_offset            = r.read_u32();
    hdr.priv_length            = r.read_u32();

    if (hdr.signature != 0x774F4632) throw ParseError("WOFF2: invalid signature");

    std::vector<Woff2TableEntry> entries(hdr.num_tables);
    for (auto& e : entries) {
        e.flags = r.read_u8();
        u8 tag_index = e.flags & 0x3F;
        if (tag_index == 63) {
            e.tag = r.read_tag();
        } else if (tag_index < WOFF2_TAG_COUNT) {
            e.tag = WOFF2_KNOWN_TAGS[tag_index];
        } else {
            e.tag = "????";
        }
        e.orig_length      = static_cast<u32>(read_base128(r));
        u8 transform_ver   = (e.flags >> 6) & 0x03;
        e.transform_length = 0;
        if (transform_ver == 0 && (e.tag == "glyf" || e.tag == "loca"))
            e.transform_length = static_cast<u32>(read_base128(r));
        else if (transform_ver != 0 && (e.tag == "glyf" || e.tag == "loca"))
            e.transform_length = static_cast<u32>(read_base128(r));
    }

    size_t compressed_start = r.pos();
    if (compressed_start + hdr.total_compressed_size > data.size())
        throw ParseError("WOFF2: compressed data out of bounds");

    size_t decomp_size = hdr.total_sfnt_size * 2;
    std::vector<u8> uncompressed(decomp_size);
    size_t actual_decomp = decomp_size;
    BrotliDecoderResult res = BrotliDecoderDecompress(
        hdr.total_compressed_size,
        data.data() + compressed_start,
        &actual_decomp,
        uncompressed.data());

    if (res != BROTLI_DECODER_RESULT_SUCCESS)
        throw ParseError("WOFF2: Brotli decompression failed");
    uncompressed.resize(actual_decomp);

    size_t table_count = entries.size();
    u32 search_range = 1;
    u32 entry_sel = 0;
    while (search_range * 2 <= table_count) { search_range *= 2; entry_sel++; }
    search_range *= 16;
    u32 range_shift = static_cast<u32>(table_count * 16) - search_range;

    std::vector<u8> sfnt(hdr.total_sfnt_size, 0);
    u8* sfnt_ptr = sfnt.data();

    auto write16 = [&](size_t p, u16 v) {
        sfnt_ptr[p] = (v >> 8); sfnt_ptr[p+1] = v & 0xFF;
    };
    auto write32 = [&](size_t p, u32 v) {
        sfnt_ptr[p]   = (v >> 24); sfnt_ptr[p+1] = (v >> 16) & 0xFF;
        sfnt_ptr[p+2] = (v >>  8) & 0xFF; sfnt_ptr[p+3] = v & 0xFF;
    };

    write32(0, hdr.flavor);
    write16(4, static_cast<u16>(table_count));
    write16(6, static_cast<u16>(search_range));
    write16(8, static_cast<u16>(entry_sel));
    write16(10, static_cast<u16>(range_shift));

    u32 table_dir = 12;
    u32 data_off  = 12 + static_cast<u32>(table_count) * 16;
    if (data_off % 4) data_off += 4 - (data_off % 4);

    u32 src_off = 0;
    for (auto& e : entries) {
        u32 len = (e.transform_length > 0) ? e.transform_length : e.orig_length;
        memcpy(sfnt_ptr + table_dir, e.tag.c_str(), 4);
        write32(table_dir + 4,  0);
        write32(table_dir + 8,  data_off);
        write32(table_dir + 12, e.orig_length);
        if (src_off + len <= uncompressed.size())
            memcpy(sfnt_ptr + data_off, uncompressed.data() + src_off, len);
        src_off  += len;
        data_off += (e.orig_length + 3) & ~3u;
        table_dir += 16;
    }

    return sfnt;
}

Font parse_woff2(const std::vector<u8>& data, const std::string& path) {
    auto sfnt = unwrap_woff2(data);
    Font f = parse_sfnt(sfnt, path);
    f.format = FontFormat::WOFF2;
    return f;
}

#else

std::vector<u8> unwrap_woff2(const std::vector<u8>&) {
    throw UnsupportedError("WOFF2 support not compiled in — rebuild with -DFONTYDE_WOFF2=ON");
}

Font parse_woff2(const std::vector<u8>&, const std::string&) {
    throw UnsupportedError("WOFF2 support not compiled in — rebuild with -DFONTYDE_WOFF2=ON");
}

#endif

} // namespace fontyde
