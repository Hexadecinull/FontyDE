#include <functional>
//
// FontyDE — Decompiler Edition
// src/formats/svgfont.cpp
//
// Minimal SAX-like XML parser — no external XML library needed.
//

#include "../fontyde/formats/svgfont.h"
#include "../fontyde/emitter/fyde_emitter.h"
#include <fstream>
#include <algorithm>
#include <cstdlib>

namespace fontyde {

static std::string xml_attr(const std::string& tag_body, const std::string& name) {
    std::string search = name + "=\"";
    size_t pos = tag_body.find(search);
    if (pos == std::string::npos) {
        search = name + "='";
        pos = tag_body.find(search);
        if (pos == std::string::npos) return "";
        size_t start = pos + search.size();
        size_t end = tag_body.find('\'', start);
        return end == std::string::npos ? "" : tag_body.substr(start, end - start);
    }
    size_t start = pos + search.size();
    size_t end = tag_body.find('"', start);
    return end == std::string::npos ? "" : tag_body.substr(start, end - start);
}

static void parse_tags(const std::string& xml,
                        std::function<void(const std::string&, const std::string&)> on_tag) {
    size_t i = 0;
    size_t n = xml.size();
    while (i < n) {
        size_t lt = xml.find('<', i);
        if (lt == std::string::npos) break;
        size_t gt = xml.find('>', lt);
        if (gt == std::string::npos) break;
        std::string tag = xml.substr(lt + 1, gt - lt - 1);
        if (!tag.empty() && tag[0] != '/' && tag[0] != '!' && tag[0] != '?') {
            size_t sp = tag.find(' ');
            std::string name = (sp == std::string::npos) ? tag : tag.substr(0, sp);
            on_tag(name, tag);
        }
        i = gt + 1;
    }
}

SvgFont parse_svg_font(const std::vector<u8>& data, const std::string&) {
    SvgFont font = {};
    std::string xml(data.begin(), data.end());

    parse_tags(xml, [&](const std::string& name, const std::string& body) {
        if (name == "font") {
            font.id           = xml_attr(body, "id");
            std::string adv   = xml_attr(body, "horiz-adv-x");
            if (!adv.empty()) font.horiz_adv_x = std::atof(adv.c_str());
        } else if (name == "font-face") {
            font.font_face.font_family = xml_attr(body, "font-family");
            std::string upm = xml_attr(body, "units-per-em");
            if (!upm.empty()) font.font_face.units_per_em = std::atof(upm.c_str());
            std::string asc = xml_attr(body, "ascent");
            if (!asc.empty()) font.font_face.ascent = std::atof(asc.c_str());
            std::string dsc = xml_attr(body, "descent");
            if (!dsc.empty()) font.font_face.descent = std::atof(dsc.c_str());
            std::string xh = xml_attr(body, "x-height");
            if (!xh.empty()) font.font_face.x_height = std::atof(xh.c_str());
            std::string ch = xml_attr(body, "cap-height");
            if (!ch.empty()) font.font_face.cap_height = std::atof(ch.c_str());
            std::string sl = xml_attr(body, "slope");
            if (!sl.empty()) font.font_face.slope = std::atof(sl.c_str());
            font.font_face.panose_1 = xml_attr(body, "panose-1");
            std::string up = xml_attr(body, "underline-position");
            if (!up.empty()) font.font_face.underline_position = std::atof(up.c_str());
            std::string ut = xml_attr(body, "underline-thickness");
            if (!ut.empty()) font.font_face.underline_thickness = std::atof(ut.c_str());
        } else if (name == "missing-glyph") {
            font.missing_glyph.d          = xml_attr(body, "d");
            std::string adv               = xml_attr(body, "horiz-adv-x");
            if (!adv.empty()) font.missing_glyph.horiz_adv_x = std::atof(adv.c_str());
        } else if (name == "glyph") {
            SvgGlyph g;
            g.unicode    = xml_attr(body, "unicode");
            g.glyph_name = xml_attr(body, "glyph-name");
            g.d          = xml_attr(body, "d");
            std::string adv = xml_attr(body, "horiz-adv-x");
            g.horiz_adv_x = adv.empty() ? font.horiz_adv_x : std::atof(adv.c_str());
            font.glyphs.push_back(std::move(g));
        }
    });

    return font;
}

void emit_svg_font_to_file(const SvgFont& svg, const std::string& src,
                            const std::string& out_path) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter e(f);
    e.emit_svg_font(svg, src);
}

} // namespace fontyde
