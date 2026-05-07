//
// FontyDE — Decompiler Edition
// src/formats/pcf.cpp
//
// PCF table types we parse:
//   PCF_PROPERTIES(1), PCF_METRICS(4), PCF_BITMAPS(8), PCF_BDF_ENCODINGS(16)
//

#include "fontyde/formats/pcf.h"
#include "fontyde/reader.h"
#include "fontyde/emitter/fyde_emitter.h"
#include <fstream>

namespace fontyde {

static constexpr u32 PCF_PROPERTIES    = (1 << 0);
static constexpr u32 PCF_METRICS       = (1 << 2);
static constexpr u32 PCF_BITMAPS       = (1 << 3);
static constexpr u32 PCF_BDF_ENCODINGS = (1 << 4);

static constexpr u32 PCF_DEFAULT_FORMAT = 0x00000000;
static constexpr u32 PCF_BYTE_MASK      = (1 << 2);
static constexpr u32 PCF_BIT_MASK       = (1 << 3);
static constexpr u32 PCF_GLYPH_PAD_MASK = 3;

struct PcfToc {
    u32 type;
    u32 format;
    u32 size;
    u32 offset;
};

static PcfMetric read_metric_compressed(BinaryReader& r) {
    PcfMetric m;
    m.left_side_bearing  = static_cast<i16>(r.read_u8()) - 0x80;
    m.right_side_bearing = static_cast<i16>(r.read_u8()) - 0x80;
    m.character_width    = static_cast<i16>(r.read_u8()) - 0x80;
    m.character_ascent   = static_cast<i16>(r.read_u8()) - 0x80;
    m.character_descent  = static_cast<i16>(r.read_u8()) - 0x80;
    m.character_attributes = 0;
    return m;
}

static PcfMetric read_metric_uncompressed(BinaryReader& r) {
    PcfMetric m;
    m.left_side_bearing    = r.read_i16();
    m.right_side_bearing   = r.read_i16();
    m.character_width      = r.read_i16();
    m.character_ascent     = r.read_i16();
    m.character_descent    = r.read_i16();
    m.character_attributes = r.read_u16();
    return m;
}

PcfFont parse_pcf(const std::vector<u8>& data, const std::string&) {
    PcfFont font = {};
    BinaryReader r(data);
    r.set_big_endian(false);

    u32 magic = r.read_u32();
    if (magic != 0x70636601u && magic != 0x01666370u)
        throw ParseError("PCF: bad magic");

    u32 table_count = r.read_u32();
    std::vector<PcfToc> toc(table_count);
    for (auto& t : toc) {
        t.type   = r.read_u32();
        t.format = r.read_u32();
        t.size   = r.read_u32();
        t.offset = r.read_u32();
    }

    std::vector<PcfMetric> all_metrics;

    for (auto& t : toc) {
        if (t.offset + t.size > data.size()) continue;
        BinaryReader tr = r.sub_reader(t.offset, t.size);
        u32 fmt = tr.read_u32();
        bool little = !(fmt & PCF_BYTE_MASK);
        tr.set_big_endian(!little);

        if (t.type == PCF_PROPERTIES) {
            u32 nprops = tr.read_u32();
            struct RawProp { i32 name_off; u8 is_string; i32 value; };
            std::vector<RawProp> raw(nprops);
            for (auto& rp : raw) {
                rp.name_off = tr.read_i32();
                rp.is_string = tr.read_u8();
                rp.value    = tr.read_i32();
            }
            u32 pad = (nprops & 3) ? (4 - (nprops & 3)) : 0;
            for (u32 i = 0; i < pad; i++) tr.skip(1);
            u32 str_size = tr.read_u32();
            auto str_blob = tr.read_bytes(str_size);
            for (auto& rp : raw) {
                if (rp.name_off < 0 || static_cast<u32>(rp.name_off) >= str_size) continue;
                std::string name;
                for (u32 i = static_cast<u32>(rp.name_off);
                     i < str_size && str_blob[i]; i++)
                    name += static_cast<char>(str_blob[i]);
                if (rp.is_string) {
                    std::string val;
                    if (rp.value >= 0 && static_cast<u32>(rp.value) < str_size) {
                        for (u32 i = static_cast<u32>(rp.value);
                             i < str_size && str_blob[i]; i++)
                            val += static_cast<char>(str_blob[i]);
                    }
                    font.props.string_props[name] = val;
                } else {
                    font.props.int_props[name] = rp.value;
                }
            }
            auto fit = font.props.int_props.find("FONT_ASCENT");
            if (fit != font.props.int_props.end()) font.font_ascent = fit->second;
            fit = font.props.int_props.find("FONT_DESCENT");
            if (fit != font.props.int_props.end()) font.font_descent = fit->second;
            auto sit = font.props.int_props.find("DEFAULT_CHAR");
            if (sit != font.props.int_props.end()) font.default_char = static_cast<u32>(sit->second);

        } else if (t.type == PCF_METRICS) {
            bool compressed = (fmt & 0xFF) == 0x00 &&
                              ((fmt >> 8) & 0xFF) == 0x00 &&
                              ((fmt >> 16) & 0xFF) == 0x04;
            if (!compressed) {
                u32 n = tr.read_u32();
                all_metrics.resize(n);
                for (auto& m : all_metrics) m = read_metric_uncompressed(tr);
            } else {
                u16 n = tr.read_u16();
                all_metrics.resize(n);
                for (auto& m : all_metrics) m = read_metric_compressed(tr);
            }

        } else if (t.type == PCF_BITMAPS) {
            u32 glyph_count = tr.read_u32();
            std::vector<u32> offsets(glyph_count);
            for (auto& off : offsets) off = tr.read_u32();
            u32 bitmap_sizes[4];
            for (auto& bs : bitmap_sizes) bs = tr.read_u32();
            u32 glyph_pad = 1u << (fmt & PCF_GLYPH_PAD_MASK);
            u32 bmp_size  = bitmap_sizes[fmt & PCF_GLYPH_PAD_MASK];
            auto bitmap_data = tr.read_bytes(bmp_size);

            font.glyphs.resize(glyph_count);
            for (u32 i = 0; i < glyph_count; i++) {
                font.glyphs[i].index = i;
                if (i < all_metrics.size()) font.glyphs[i].metric = all_metrics[i];
                auto& m = font.glyphs[i].metric;
                int row_w = m.right_side_bearing - m.left_side_bearing;
                int h     = m.character_ascent   + m.character_descent;
                if (row_w <= 0 || h <= 0) continue;
                u32 row_bytes = ((static_cast<u32>(row_w) + (glyph_pad * 8) - 1) /
                                 (glyph_pad * 8)) * glyph_pad;
                u32 glyph_bytes = row_bytes * static_cast<u32>(h);
                if (offsets[i] + glyph_bytes <= bmp_size)
                    font.glyphs[i].bitmap = std::vector<u8>(
                        bitmap_data.begin() + offsets[i],
                        bitmap_data.begin() + offsets[i] + glyph_bytes);
            }
        }
    }

    if (font.glyphs.empty() && !all_metrics.empty()) {
        font.glyphs.resize(all_metrics.size());
        for (size_t i = 0; i < all_metrics.size(); i++) {
            font.glyphs[i].index  = static_cast<u32>(i);
            font.glyphs[i].metric = all_metrics[i];
        }
    }
    return font;
}

void emit_pcf_to_file(const PcfFont& pcf, const std::string& src,
                      const std::string& out_path) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter e(f);
    e.emit_pcf(pcf, src);
}

} // namespace fontyde
