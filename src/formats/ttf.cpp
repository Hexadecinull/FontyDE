//
// FontyDE — Decompiler Edition
// src/formats/ttf.cpp
//

#include "../fontyde/formats/ttf.h"
#include "../fontyde/reader.h"
#include <algorithm>
#include "../fontyde/tables/head.h"
#include "../fontyde/tables/name.h"
#include "../fontyde/tables/cmap.h"
#include "../fontyde/tables/hhea.h"
#include "../fontyde/tables/hmtx.h"
#include "../fontyde/tables/maxp.h"
#include "../fontyde/tables/post.h"
#include "../fontyde/tables/os2.h"
#include "../fontyde/tables/loca.h"
#include "../fontyde/tables/glyf.h"
#include "../fontyde/tables/cff.h"
#include "../fontyde/tables/kern.h"

namespace fontyde {

static SfntHeader parse_sfnt_header(BinaryReader& r) {
    SfntHeader hdr;
    hdr.sfVersion    = r.read_u32();
    hdr.numTables    = r.read_u16();
    hdr.searchRange  = r.read_u16();
    hdr.entrySelector = r.read_u16();
    hdr.rangeShift   = r.read_u16();

    hdr.tables.resize(hdr.numTables);
    for (auto& tbl : hdr.tables) {
        tbl.tag      = r.read_tag();
        tbl.checksum = r.read_u32();
        tbl.offset   = r.read_u32();
        tbl.length   = r.read_u32();
    }
    return hdr;
}

Font parse_sfnt(const std::vector<u8>& data, const std::string& path) {
    BinaryReader r(data);
    Font font;
    font.source_path  = path;
    font.sfnt_header  = parse_sfnt_header(r);

    for (auto& tbl : font.sfnt_header.tables)
        font.table_map[tbl.tag] = tbl;

    u32 ver = font.sfnt_header.sfVersion;
    if (ver == 0x4F54544F)
        font.format = FontFormat::OpenType_CFF;
    else
        font.format = FontFormat::TrueType;

    auto read_table = [&](const std::string& tag) -> std::optional<BinaryReader> {
        auto it = font.table_map.find(tag);
        if (it == font.table_map.end()) return std::nullopt;
        auto& rec = it->second;
        if (rec.offset + rec.length > data.size())
            throw ParseError("table '" + tag + "' extends past end of file");
        return BinaryReader(data.data() + rec.offset, rec.length);
    };

    if (auto br = read_table("head")) { font.head = HeadTable::parse(*br); }
    if (auto br = read_table("name")) { font.name = NameTable::parse(*br); }
    if (auto br = read_table("cmap")) { font.cmap = CmapTable::parse(*br); }
    if (auto br = read_table("maxp")) { font.maxp = MaxpTable::parse(*br); }
    if (auto br = read_table("post")) { font.post = PostTable::parse(*br); }
    if (auto br = read_table("OS/2")) { font.os2  = Os2Table::parse(*br); }
    if (auto br = read_table("hhea")) { font.hhea = HheaTable::parse(*br); }
    if (auto br = read_table("kern")) { font.kern = KernTable::parse(*br); }

    if (font.hhea && font.maxp) {
        if (auto br = read_table("hmtx")) {
            font.hmtx = HmtxTable::parse(*br,
                font.hhea->number_of_h_metrics,
                font.maxp->num_glyphs);
        }
    }

    if (font.head && font.maxp) {
        if (auto br = read_table("loca")) {
            font.loca = LocaTable::parse(*br,
                font.maxp->num_glyphs,
                font.head->index_to_loc_format);
        }
    }

    if (font.loca && font.maxp) {
        if (auto br = read_table("glyf")) {
            font.glyf = GlyfTable::parse(*br, *font.loca, font.maxp->num_glyphs);
        }
    }

    if (font.format == FontFormat::OpenType_CFF) {
        if (auto br = read_table("CFF ")) {
            font.cff = CffTable::parse(*br);
        }
    }

    if (font.glyf && font.post && font.cmap && font.hmtx) {
        auto& glyphs = font.glyf->glyphs;
        for (auto& g : glyphs) {
            g.name = font.post->glyph_name(g.gid);
            if (g.gid < font.hmtx->h_metrics.size()) {
                g.advance_width = font.hmtx->h_metrics[g.gid].advance_width;
                g.lsb           = font.hmtx->h_metrics[g.gid].lsb;
            }
        }
        for (auto& st : font.cmap->subtables) {
            for (auto& [cp, gid] : st.mapping) {
                if (gid < glyphs.size())
                    glyphs[gid].codepoints.push_back(cp);
            }
        }
        for (auto& g : glyphs) {
            std::sort(g.codepoints.begin(), g.codepoints.end());
            g.codepoints.erase(
                std::unique(g.codepoints.begin(), g.codepoints.end()),
                g.codepoints.end());
        }
    }

    if (font.cff && font.cmap) {
        for (auto& st : font.cmap->subtables) {
            for (auto& [cp, gid] : st.mapping) {
                if (gid < font.cff->glyphs.size())
                    font.cff->glyphs[gid].name = font.cff->glyphs[gid].name;
            }
        }
    }

    return font;
}

Font parse_ttf(const std::vector<u8>& data, const std::string& path) {
    return parse_sfnt(data, path);
}

Font parse_otf(const std::vector<u8>& data, const std::string& path) {
    return parse_sfnt(data, path);
}

std::vector<Font> parse_ttc(const std::vector<u8>& data, const std::string& path) {
    BinaryReader r(data);

    std::string tag = r.read_tag();
    if (tag != "ttcf") throw ParseError("not a TTC file: bad magic");

    u16 major = r.read_u16();
    u16 minor = r.read_u16();
    u32 num_fonts = r.read_u32();

    std::vector<u32> offsets(num_fonts);
    for (auto& off : offsets) off = r.read_u32();

    if (major >= 2) {
        u32 dsig_tag    = r.read_u32();
        u32 dsig_length = r.read_u32();
        u32 dsig_offset = r.read_u32();
        (void)dsig_tag; (void)dsig_length; (void)dsig_offset;
    }
    (void)minor;

    std::vector<Font> fonts;
    fonts.reserve(num_fonts);

    for (u32 i = 0; i < num_fonts; i++) {
        if (offsets[i] >= data.size()) continue;
        std::vector<u8> sub(data.begin() + offsets[i], data.end());
        try {
            Font f = parse_sfnt(sub, path + "#" + std::to_string(i));
            f.format = FontFormat::TTC;
            fonts.push_back(std::move(f));
        } catch (const FontyError& e) {
            (void)e;
        }
    }
    return fonts;
}

} // namespace fontyde
