//
// FontyDE — Decompiler Edition
// src/formats/angelcode.cpp
//
// AngelCode BMFont format parser.
// Text variant: key=value lines. Binary variant: block-based with block type IDs.
//

#include "../fontyde/formats/angelcode.h"
#include "../fontyde/reader.h"
#include "../fontyde/emitter/fyde_emitter.h"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <fstream>

namespace fontyde {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::map<std::string, std::string> parse_kv_line(const std::string& line) {
    std::map<std::string, std::string> kv;
    size_t i = 0;
    size_t n = line.size();
    while (i < n) {
        while (i < n && isspace((u8)line[i])) i++;
        size_t key_start = i;
        while (i < n && line[i] != '=' && !isspace((u8)line[i])) i++;
        std::string key = line.substr(key_start, i - key_start);
        if (key.empty()) { i++; continue; }
        if (i >= n || line[i] != '=') continue;
        i++;
        std::string val;
        if (i < n && line[i] == '"') {
            i++;
            while (i < n && line[i] != '"') val += line[i++];
            if (i < n) i++;
        } else {
            while (i < n && !isspace((u8)line[i])) val += line[i++];
        }
        kv[key] = val;
    }
    return kv;
}

static int kv_int(const std::map<std::string, std::string>& kv,
                  const std::string& k, int def = 0) {
    auto it = kv.find(k);
    if (it == kv.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

static std::string kv_str(const std::map<std::string, std::string>& kv,
                           const std::string& k, const std::string& def = "") {
    auto it = kv.find(k);
    return it == kv.end() ? def : it->second;
}

static BmfFont parse_text(const std::vector<u8>& data) {
    BmfFont font = {};
    std::istringstream ss(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        std::string tag  = (sp == std::string::npos) ? line : line.substr(0, sp);
        std::string rest = (sp == std::string::npos) ? "" : line.substr(sp + 1);
        auto kv = parse_kv_line(rest);

        if (tag == "info") {
            font.info.face      = kv_str(kv, "face");
            font.info.size      = kv_int(kv, "size");
            font.info.bold      = kv_int(kv, "bold") != 0;
            font.info.italic    = kv_int(kv, "italic") != 0;
            font.info.charset   = kv_str(kv, "charset");
            font.info.unicode   = kv_int(kv, "unicode") != 0;
            font.info.stretch_h = kv_int(kv, "stretchH", 100);
            font.info.smooth    = kv_int(kv, "smooth") != 0;
            font.info.aa        = kv_int(kv, "aa", 1);
            font.info.outline   = kv_int(kv, "outline");
        } else if (tag == "common") {
            font.common.line_height = kv_int(kv, "lineHeight");
            font.common.base        = kv_int(kv, "base");
            font.common.scale_w     = kv_int(kv, "scaleW");
            font.common.scale_h     = kv_int(kv, "scaleH");
            font.common.pages       = kv_int(kv, "pages", 1);
            font.common.packed      = kv_int(kv, "packed") != 0;
            font.common.alpha_chnl  = kv_int(kv, "alphaChnl");
            font.common.red_chnl    = kv_int(kv, "redChnl");
            font.common.green_chnl  = kv_int(kv, "greenChnl");
            font.common.blue_chnl   = kv_int(kv, "blueChnl");
        } else if (tag == "page") {
            int id = kv_int(kv, "id");
            std::string file = kv_str(kv, "file");
            if (id >= static_cast<int>(font.pages.size()))
                font.pages.resize(static_cast<size_t>(id + 1));
            font.pages[static_cast<size_t>(id)] = file;
        } else if (tag == "char") {
            BmfChar c;
            c.id        = static_cast<u32>(kv_int(kv, "id"));
            c.x         = kv_int(kv, "x");
            c.y         = kv_int(kv, "y");
            c.width     = kv_int(kv, "width");
            c.height    = kv_int(kv, "height");
            c.x_offset  = kv_int(kv, "xoffset");
            c.y_offset  = kv_int(kv, "yoffset");
            c.x_advance = kv_int(kv, "xadvance");
            c.page      = kv_int(kv, "page");
            c.chnl      = kv_int(kv, "chnl");
            font.chars.push_back(c);
        } else if (tag == "kerning") {
            BmfKerning k;
            k.first  = static_cast<u32>(kv_int(kv, "first"));
            k.second = static_cast<u32>(kv_int(kv, "second"));
            k.amount = kv_int(kv, "amount");
            font.kernings.push_back(k);
        }
    }
    return font;
}

static BmfFont parse_binary(const std::vector<u8>& data) {
    BmfFont font = {};
    BinaryReader r(data);
    r.set_big_endian(false);
    r.skip(3); // "BMF"
    u8 version = r.read_u8();
    (void)version;

    while (!r.eof()) {
        u8  block_type = r.read_u8();
        u32 block_size = r.read_u32();
        size_t block_end = r.pos() + block_size;

        if (block_type == 1) {
            font.info.size      = r.read_i16();
            u8 bits             = r.read_u8();
            font.info.smooth    = (bits >> 7) & 1;
            font.info.unicode   = (bits >> 6) & 1;
            font.info.italic    = (bits >> 5) & 1;
            font.info.bold      = (bits >> 4) & 1;
            font.info.outline   = (bits & 0x08) ? 1 : 0;
            r.read_u8();
            font.info.stretch_h = r.read_u16();
            font.info.aa        = r.read_u8();
            for (int i = 0; i < 4; i++) font.info.padding[i] = r.read_u8();
            for (int i = 0; i < 2; i++) font.info.spacing[i] = r.read_u8();
            font.info.outline   = r.read_u8();
            while (r.pos() < block_end && !r.eof()) {
                char c = static_cast<char>(r.read_u8());
                if (c == 0) break;
                font.info.face += c;
            }
        } else if (block_type == 2) {
            font.common.line_height = r.read_u16();
            font.common.base        = r.read_u16();
            font.common.scale_w     = r.read_u16();
            font.common.scale_h     = r.read_u16();
            font.common.pages       = r.read_u16();
            u8 bits2                = r.read_u8();
            font.common.packed      = (bits2 & 0x01) != 0;
            font.common.alpha_chnl  = r.read_u8();
            font.common.red_chnl    = r.read_u8();
            font.common.green_chnl  = r.read_u8();
            font.common.blue_chnl   = r.read_u8();
        } else if (block_type == 3) {
            size_t total = block_size;
            std::string all_names;
            all_names.resize(total);
            for (size_t i = 0; i < total && !r.eof(); i++)
                all_names[i] = static_cast<char>(r.read_u8());
            std::istringstream names_ss(all_names);
            std::string page_name;
            while (std::getline(names_ss, page_name, '\0'))
                if (!page_name.empty()) font.pages.push_back(page_name);
        } else if (block_type == 4) {
            size_t num_chars = block_size / 20;
            for (size_t i = 0; i < num_chars && !r.eof(); i++) {
                BmfChar c;
                c.id        = r.read_u32();
                c.x         = r.read_u16();
                c.y         = r.read_u16();
                c.width     = r.read_u16();
                c.height    = r.read_u16();
                c.x_offset  = r.read_i16();
                c.y_offset  = r.read_i16();
                c.x_advance = r.read_i16();
                c.page      = r.read_u8();
                c.chnl      = r.read_u8();
                font.chars.push_back(c);
            }
        } else if (block_type == 5) {
            size_t num_kern = block_size / 10;
            for (size_t i = 0; i < num_kern && !r.eof(); i++) {
                BmfKerning k;
                k.first  = r.read_u32();
                k.second = r.read_u32();
                k.amount = r.read_i16();
                font.kernings.push_back(k);
            }
        }

        if (r.pos() < block_end) r.seek(block_end);
    }
    return font;
}

BmfFont parse_angelcode(const std::vector<u8>& data, const std::string&) {
    if (data.size() >= 4 && data[0]=='B' && data[1]=='M' && data[2]=='F')
        return parse_binary(data);
    return parse_text(data);
}

void emit_angelcode_to_file(const BmfFont& bmf, const std::string& src,
                             const std::string& out_path, const EmitOptions& opts) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter e(f, opts);
    e.emit_angelcode(bmf, src, opts);
}

} // namespace fontyde
