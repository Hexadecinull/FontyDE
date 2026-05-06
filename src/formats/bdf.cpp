//
// FontyDE — Decompiler Edition
// src/formats/bdf.cpp
//

#include "fontyde/formats/bdf.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace fontyde {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

BdfFont parse_bdf_raw(const std::vector<u8>& data) {
    BdfFont font;
    std::istringstream stream(std::string(data.begin(), data.end()));
    std::string line;

    bool in_char    = false;
    bool in_bitmap  = false;
    BdfGlyph cur_glyph;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tok = split_line(line);
        if (tok.empty()) continue;

        std::string key = tok[0];

        if (key == "FONT" && tok.size() > 1) {
            font.properties.font_name = tok[1];
        } else if (key == "SIZE" && tok.size() >= 4) {
            font.properties.point_size   = std::stoi(tok[1]);
            font.properties.resolution_x = std::stoi(tok[2]);
            font.properties.resolution_y = std::stoi(tok[3]);
        } else if (key == "FONTBOUNDINGBOX" && tok.size() >= 5) {
            font.properties.bounding_box_w = std::stoi(tok[1]);
            font.properties.bounding_box_h = std::stoi(tok[2]);
            font.properties.bounding_box_x = std::stoi(tok[3]);
            font.properties.bounding_box_y = std::stoi(tok[4]);
        } else if (key == "FONT_ASCENT" && tok.size() >= 2) {
            font.properties.ascent = std::stoi(tok[1]);
        } else if (key == "FONT_DESCENT" && tok.size() >= 2) {
            font.properties.descent = std::stoi(tok[1]);
        } else if (key == "DEFAULT_CHAR" && tok.size() >= 2) {
            font.properties.default_char = std::stoi(tok[1]);
        } else if (key == "STARTCHAR") {
            in_char     = true;
            in_bitmap   = false;
            cur_glyph   = BdfGlyph{};
            cur_glyph.name = (tok.size() > 1) ? tok[1] : "";
        } else if (key == "ENDCHAR") {
            font.glyphs.push_back(cur_glyph);
            in_char   = false;
            in_bitmap = false;
        } else if (in_char) {
            if (key == "ENCODING" && tok.size() >= 2) {
                cur_glyph.encoding = std::stoi(tok[1]);
            } else if (key == "DWIDTH" && tok.size() >= 3) {
                cur_glyph.d_width_x = std::stoi(tok[1]);
                cur_glyph.d_width_y = std::stoi(tok[2]);
            } else if (key == "BBX" && tok.size() >= 5) {
                cur_glyph.bb_w = std::stoi(tok[1]);
                cur_glyph.bb_h = std::stoi(tok[2]);
                cur_glyph.bb_x = std::stoi(tok[3]);
                cur_glyph.bb_y = std::stoi(tok[4]);
            } else if (key == "BITMAP") {
                in_bitmap = true;
            } else if (in_bitmap) {
                try {
                    cur_glyph.bitmap_rows.push_back(
                        static_cast<u32>(std::stoul(key, nullptr, 16)));
                } catch (...) {}
            }
        }
    }
    return font;
}

Font parse_bdf(const std::vector<u8>& data, const std::string& path) {
    auto bdf = parse_bdf_raw(data);
    Font font;
    font.format      = FontFormat::BDF;
    font.source_path = path;

    NameTable name_tbl;
    name_tbl.format = 0;
    NameRecord nr;
    nr.platform_id = 1; nr.encoding_id = 0; nr.language_id = 0;
    nr.name_id = 4; nr.value = bdf.properties.font_name;
    nr.length  = static_cast<u16>(nr.value.size()); nr.offset = 0;
    name_tbl.records.push_back(nr);
    name_tbl.count = 1;
    font.name = std::move(name_tbl);

    MaxpTable maxp = {};
    maxp.version    = 0x00005000;
    maxp.num_glyphs = static_cast<u16>(bdf.glyphs.size());
    font.maxp = maxp;

    HeadTable head = {};
    head.major_version  = 1;
    head.units_per_em   = static_cast<u16>(bdf.properties.bounding_box_h);
    head.x_min = static_cast<i16>(bdf.properties.bounding_box_x);
    head.y_min = static_cast<i16>(bdf.properties.bounding_box_y);
    head.x_max = static_cast<i16>(bdf.properties.bounding_box_x + bdf.properties.bounding_box_w);
    head.y_max = static_cast<i16>(bdf.properties.bounding_box_y + bdf.properties.bounding_box_h);
    font.head = head;

    return font;
}

} // namespace fontyde
