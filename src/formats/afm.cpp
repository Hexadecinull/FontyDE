//
// FontyDE — Decompiler Edition
// src/formats/afm.cpp
//

#include "fontyde/formats/afm.h"
#include "fontyde/emitter/fyde_emitter.h"
#include <sstream>
#include <cstdlib>
#include <fstream>

namespace fontyde {

static std::string afm_trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

AfmFont parse_afm(const std::vector<u8>& data, const std::string&) {
    AfmFont font = {};
    std::istringstream ss(std::string(data.begin(), data.end()));
    std::string line;
    bool in_metrics = false;
    bool in_kern    = false;

    while (std::getline(ss, line)) {
        line = afm_trim(line);
        if (line.empty() || line[0] == '%') continue;

        size_t sp = line.find(' ');
        std::string key  = (sp == std::string::npos) ? line : line.substr(0, sp);
        std::string rest = (sp == std::string::npos) ? "" : afm_trim(line.substr(sp + 1));

        if      (key == "FontName")          font.font_name         = rest;
        else if (key == "FullName")          font.full_name         = rest;
        else if (key == "FamilyName")        font.family_name       = rest;
        else if (key == "Weight")            font.weight            = rest;
        else if (key == "EncodingScheme")    font.encoding_scheme   = rest;
        else if (key == "MetricsSets")       font.metric_sets       = rest;
        else if (key == "StartFontMetrics")  font.afm_version       = rest;
        else if (key == "IsFixedPitch")      font.is_fixed_pitch    = (rest == "true");
        else if (key == "ItalicAngle")       font.italic_angle      = std::atof(rest.c_str());
        else if (key == "UnderlinePosition") font.underline_position= std::atof(rest.c_str());
        else if (key == "UnderlineThickness")font.underline_thickness=std::atof(rest.c_str());
        else if (key == "CapHeight")         font.cap_height        = std::atof(rest.c_str());
        else if (key == "XHeight")           font.x_height          = std::atof(rest.c_str());
        else if (key == "Ascender")          font.ascender          = std::atof(rest.c_str());
        else if (key == "Descender")         font.descender         = std::atof(rest.c_str());
        else if (key == "FontBBox") {
            std::istringstream bss(rest);
            bss >> font.llx >> font.lly >> font.urx >> font.ury;
        }
        else if (key == "StartCharMetrics") { in_metrics = true; }
        else if (key == "EndCharMetrics")   { in_metrics = false; }
        else if (key == "StartKernPairs")   { in_kern = true; }
        else if (key == "EndKernPairs")     { in_kern = false; }
        else if (in_metrics && key == "C") {
            AfmCharMetric m = {};
            std::istringstream mss(line);
            std::string tok;
            while (mss >> tok) {
                if (tok == "C")  mss >> m.code;
                else if (tok == "WX") mss >> m.width_x;
                else if (tok == "W0X") mss >> m.width_x;
                else if (tok == "WY") mss >> m.width_y;
                else if (tok == "N")  mss >> m.name;
                else if (tok == "B") {
                    mss >> m.llx >> m.lly >> m.urx >> m.ury;
                }
            }
            font.char_metrics.push_back(m);
        }
        else if (in_kern && key == "KPX") {
            AfmKernPair kp = {};
            std::istringstream kss(rest);
            kss >> kp.name1 >> kp.name2 >> kp.dx;
            kp.dy = 0;
            font.kern_pairs.push_back(kp);
        }
        else if (in_kern && key == "KP") {
            AfmKernPair kp = {};
            std::istringstream kss(rest);
            kss >> kp.name1 >> kp.name2 >> kp.dx >> kp.dy;
            font.kern_pairs.push_back(kp);
        }
    }
    return font;
}

void emit_afm_to_file(const AfmFont& afm, const std::string& src,
                      const std::string& out_path) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter e(f);
    e.emit_afm(afm, src);
}

} // namespace fontyde
