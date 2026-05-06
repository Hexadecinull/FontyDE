//
// FontyDE — Decompiler Edition
// src/detector.cpp
//

#include "fontyde/detector.h"
#include "fontyde/reader.h"
#include <algorithm>
#include <cctype>

namespace fontyde {

FontFormat detect_format(const u8* data, size_t size) {
    if (size < 4) return FontFormat::Unknown;

    auto eq4 = [&](const char* tag) {
        return data[0] == (u8)tag[0] &&
               data[1] == (u8)tag[1] &&
               data[2] == (u8)tag[2] &&
               data[3] == (u8)tag[3];
    };

    if (eq4("wOFF")) return FontFormat::WOFF;
    if (eq4("wOF2")) return FontFormat::WOFF2;
    if (eq4("ttcf")) return FontFormat::TTC;
    if (eq4("OTTO")) return FontFormat::OpenType_CFF;
    if (eq4("true") || eq4("typ1")) return FontFormat::TrueType;

    if (data[0] == 0x00 && data[1] == 0x01 && data[2] == 0x00 && data[3] == 0x00)
        return FontFormat::TrueType;

    if (data[0] == 0x80 && data[1] == 0x01) return FontFormat::Type1_PFB;
    if (data[0] == 0x80 && data[1] == 0x02) return FontFormat::Type1_PFB;

    if (size >= 8 && data[0] == '%' && data[1] == '!' && data[2] == 'P' && data[3] == 'S')
        return FontFormat::Type1_PFA;

    if (size > 12) {
        static const char bdf_magic[] = "STARTFONT";
        if (memcmp(data, bdf_magic, 9) == 0) return FontFormat::BDF;
    }

    if (size >= 4 && data[0] == 0x02 && data[1] == 0x00) return FontFormat::FNT;
    if (size >= 4 && data[0] == 0x03 && data[1] == 0x00) return FontFormat::FNT;

    if (size >= 4) {
        u32 ne_off = 0;
        memcpy(&ne_off, data + 0x3C, 4);
        if (ne_off + 2 < size && data[ne_off] == 'N' && data[ne_off+1] == 'E')
            return FontFormat::FON;
        if (ne_off + 2 < size && data[ne_off] == 'P' && data[ne_off+1] == 'E')
            return FontFormat::FON;
    }

    return FontFormat::Unknown;
}

FontFormat detect_format(const std::vector<u8>& data) {
    return detect_format(data.data(), data.size());
}

FontFormat detect_format_from_path(const std::string& path) {
    auto ext_pos = path.rfind('.');
    if (ext_pos == std::string::npos) return FontFormat::Unknown;
    std::string ext = path.substr(ext_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "ttf")  return FontFormat::TrueType;
    if (ext == "otf")  return FontFormat::OpenType_CFF;
    if (ext == "woff") return FontFormat::WOFF;
    if (ext == "woff2") return FontFormat::WOFF2;
    if (ext == "pfb")  return FontFormat::Type1_PFB;
    if (ext == "pfa")  return FontFormat::Type1_PFA;
    if (ext == "bdf")  return FontFormat::BDF;
    if (ext == "fnt")  return FontFormat::FNT;
    if (ext == "fon")  return FontFormat::FON;
    if (ext == "ttc")  return FontFormat::TTC;
    if (ext == "otc")  return FontFormat::OTC;
    return FontFormat::Unknown;
}

std::string detect_sfnt_flavor(const u8* data, size_t size) {
    if (size < 4) return "unknown";
    if (data[0] == 0x00 && data[1] == 0x01 && data[2] == 0x00 && data[3] == 0x00)
        return "TrueType";
    if (data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O')
        return "CFF";
    if (data[0] == 't' && data[1] == 'r' && data[2] == 'u' && data[3] == 'e')
        return "TrueType (Apple)";
    return "unknown";
}

} // namespace fontyde
