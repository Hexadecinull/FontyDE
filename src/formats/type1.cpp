//
// FontyDE — Decompiler Edition
// src/formats/type1.cpp
//
// Type 1 fonts use a two-level encryption scheme:
//   1. The PFB binary wrapper segments the data into ASCII, binary, and EOF blocks.
//   2. The "eexec" section is encrypted with a linear congruential cipher (key=55665).
//   Charstrings inside are encrypted with a second pass (key=4330).
//

#include "../fontyde/formats/type1.h"
#include "../fontyde/reader.h"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace fontyde {

static constexpr u16 EEXEC_KEY       = 55665;
static constexpr u16 CHARSTRING_KEY  = 4330;
static constexpr u16 EEXEC_C1        = 52845;
static constexpr u16 EEXEC_C2        = 22719;

static std::vector<u8> t1_decrypt(const std::vector<u8>& input, u16 key, int skip_bytes) {
    u16 r = key;
    std::vector<u8> out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        u8 cipher = input[i];
        u8 plain  = cipher ^ (r >> 8);
        r = static_cast<u16>((cipher + r) * EEXEC_C1 + EEXEC_C2);
        if (static_cast<int>(i) >= skip_bytes) out.push_back(plain);
    }
    return out;
}

static u8 hex_nibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<u8>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<u8>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<u8>(c - 'A' + 10);
    return 0;
}

static std::vector<u8> hex_decode(const std::string& s) {
    std::vector<u8> out;
    bool hi = true;
    u8 acc = 0;
    for (char c : s) {
        if (!isxdigit(static_cast<unsigned char>(c))) continue;
        if (hi) { acc = hex_nibble(c) << 4; hi = false; }
        else    { out.push_back(acc | hex_nibble(c)); hi = true; }
    }
    return out;
}

std::vector<Type1Segment> parse_pfb_segments(const std::vector<u8>& data) {
    std::vector<Type1Segment> segs;
    size_t i = 0;
    while (i + 6 <= data.size()) {
        if (data[i] != 0x80) throw ParseError("PFB: missing segment marker");
        u8 type = data[i + 1];
        if (type == 3) break;
        u32 length = static_cast<u32>(data[i+2]) |
                     (static_cast<u32>(data[i+3]) << 8) |
                     (static_cast<u32>(data[i+4]) << 16) |
                     (static_cast<u32>(data[i+5]) << 24);
        i += 6;
        if (i + length > data.size()) throw ParseError("PFB: segment data truncated");
        Type1Segment seg;
        seg.type = type;
        seg.data = std::vector<u8>(data.begin() + i, data.begin() + i + length);
        segs.push_back(std::move(seg));
        i += length;
    }
    return segs;
}

std::string decode_eexec(const std::vector<u8>& data) {
    auto dec = t1_decrypt(data, EEXEC_KEY, 4);
    return std::string(dec.begin(), dec.end());
}

static Font build_type1_font(const std::string& ascii_src,
                              const std::string& eexec_src,
                              const std::string& path,
                              FontFormat fmt) {
    Font font;
    font.format      = fmt;
    font.source_path = path;

    NameTable name_tbl;
    name_tbl.format = 0;

    auto extract = [&](const std::string& src, const std::string& key) -> std::string {
        size_t pos = src.find(key);
        if (pos == std::string::npos) return "";
        pos += key.size();
        while (pos < src.size() && src[pos] == ' ') pos++;
        if (pos < src.size() && src[pos] == '(') {
            size_t end = src.find(')', pos);
            if (end != std::string::npos) return src.substr(pos + 1, end - pos - 1);
        }
        size_t end = pos;
        while (end < src.size() && src[end] != '\n' && src[end] != '\r') end++;
        return src.substr(pos, end - pos);
    };

    auto make_record = [&](u16 name_id, const std::string& val) {
        if (val.empty()) return;
        NameRecord nr;
        nr.platform_id = 1;
        nr.encoding_id = 0;
        nr.language_id = 0;
        nr.name_id     = name_id;
        nr.length      = static_cast<u16>(val.size());
        nr.offset      = 0;
        nr.value       = val;
        name_tbl.records.push_back(nr);
    };

    make_record(0,  extract(ascii_src, "/Notice"));
    make_record(4,  extract(ascii_src, "/FullName"));
    make_record(1,  extract(ascii_src, "/FamilyName"));
    make_record(2,  extract(ascii_src, "/Weight"));
    make_record(6,  extract(ascii_src, "/FontName /").empty()
                    ? extract(ascii_src, "/FontName")
                    : extract(ascii_src, "/FontName /"));

    name_tbl.count = static_cast<u16>(name_tbl.records.size());
    font.name = std::move(name_tbl);

    return font;
}

Font parse_type1_pfb(const std::vector<u8>& data, const std::string& path) {
    auto segs = parse_pfb_segments(data);
    if (segs.empty()) throw ParseError("PFB: no segments found");

    std::string ascii_src(segs[0].data.begin(), segs[0].data.end());
    std::string eexec_src;
    if (segs.size() >= 2) eexec_src = decode_eexec(segs[1].data);

    return build_type1_font(ascii_src, eexec_src, path, FontFormat::Type1_PFB);
}

Font parse_type1_pfa(const std::vector<u8>& data, const std::string& path) {
    std::string src(data.begin(), data.end());

    size_t eexec_pos = src.find("eexec");
    std::string ascii_src = (eexec_pos != std::string::npos)
                            ? src.substr(0, eexec_pos) : src;
    std::string eexec_src;

    if (eexec_pos != std::string::npos) {
        size_t hex_start = eexec_pos + 5;
        while (hex_start < src.size() && isspace(static_cast<unsigned char>(src[hex_start])))
            hex_start++;
        auto hex_bytes = hex_decode(src.substr(hex_start));
        eexec_src = decode_eexec(hex_bytes);
    }

    return build_type1_font(ascii_src, eexec_src, path, FontFormat::Type1_PFA);
}

} // namespace fontyde
