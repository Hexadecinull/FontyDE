//
// FontyDE — Decompiler Edition
// src/detector.cpp
//

#include "fontyde/detector.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fontyde {

FontFormat detect_format(const u8* data, size_t size) {
    if (size < 4) return FontFormat::Unknown;

    auto eq4 = [&](const char* tag) {
        return data[0]==(u8)tag[0] && data[1]==(u8)tag[1] &&
               data[2]==(u8)tag[2] && data[3]==(u8)tag[3];
    };

    // SFNT / OpenType family
    if (eq4("wOFF")) return FontFormat::WOFF;
    if (eq4("wOF2")) return FontFormat::WOFF2;
    if (eq4("ttcf")) return FontFormat::TTC;
    if (eq4("OTTO")) return FontFormat::OpenType_CFF;
    if (eq4("true") || eq4("typ1")) return FontFormat::TrueType;
    if (data[0]==0x00 && data[1]==0x01 && data[2]==0x00 && data[3]==0x00)
        return FontFormat::TrueType;

    // Type 1
    if (size >= 2 && data[0]==0x80 && (data[1]==0x01 || data[1]==0x02))
        return FontFormat::Type1_PFB;
    if (size >= 4 && data[0]=='%' && data[1]=='!' && data[2]=='P' && data[3]=='S')
        return FontFormat::Type1_PFA;

    // BDF
    if (size >= 9 && memcmp(data, "STARTFONT", 9) == 0)
        return FontFormat::BDF;

    // PSF1 magic: 0x36 0x04
    if (size >= 4 && data[0]==0x36 && data[1]==0x04)
        return FontFormat::PSF;
    // PSF2 magic: 0x72 0xb5 0x4a 0x86
    if (size >= 4 && data[0]==0x72 && data[1]==0xb5 && data[2]==0x4a && data[3]==0x86)
        return FontFormat::PSF;

    // PCF magic: 0x01 "fcp"
    if (size >= 4 && data[0]==0x01 && data[1]=='f' && data[2]=='c' && data[3]=='p')
        return FontFormat::PCF;

    // AngelCode BMFont binary: "BMF" + version byte
    if (size >= 4 && data[0]=='B' && data[1]=='M' && data[2]=='F')
        return FontFormat::AngelCode;

    // AngelCode BMFont text: starts with "info face="
    if (size >= 9 && memcmp(data, "info face", 9) == 0)
        return FontFormat::AngelCode;

    // AFM: starts with "StartFontMetrics"
    if (size >= 16 && memcmp(data, "StartFontMetrics", 16) == 0)
        return FontFormat::AFM;

    // SVG Font: contains <font or <?xml
    if (size >= 5 && (memcmp(data, "<?xml", 5) == 0 || memcmp(data, "<svg",4)==0 ||
                      memcmp(data, "<font",5)==0))
        return FontFormat::SVGFont;

    // Windows FNT v2/v3
    if (size >= 2 && (data[0]==0x02 || data[0]==0x03) && data[1]==0x00)
        return FontFormat::FNT;

    // FON (NE or PE DLL)
    if (size >= 2 && data[0]=='M' && data[1]=='Z') {
        if (size >= 0x40) {
            u32 ne_off = 0;
            memcpy(&ne_off, data + 0x3C, 4);
            if (ne_off + 2 < size) {
                if ((data[ne_off]=='N' && data[ne_off+1]=='E') ||
                    (data[ne_off]=='P' && data[ne_off+1]=='E'))
                    return FontFormat::FON;
            }
        }
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

    if (ext == "ttf")   return FontFormat::TrueType;
    if (ext == "otf")   return FontFormat::OpenType_CFF;
    if (ext == "woff")  return FontFormat::WOFF;
    if (ext == "woff2") return FontFormat::WOFF2;
    if (ext == "pfb")   return FontFormat::Type1_PFB;
    if (ext == "pfa")   return FontFormat::Type1_PFA;
    if (ext == "bdf")   return FontFormat::BDF;
    if (ext == "fon")   return FontFormat::FON;
    if (ext == "ttc")   return FontFormat::TTC;
    if (ext == "otc")   return FontFormat::OTC;
    if (ext == "psf" || ext == "psf2") return FontFormat::PSF;
    if (ext == "pcf")   return FontFormat::PCF;
    if (ext == "afm")   return FontFormat::AFM;
    if (ext == "svg")   return FontFormat::SVGFont;
    // .fnt is ambiguous: Windows FNT vs AngelCode — defer to magic byte
    return FontFormat::Unknown;
}

std::string detect_sfnt_flavor(const u8* data, size_t size) {
    if (size < 4) return "unknown";
    if (data[0]==0x00 && data[1]==0x01 && data[2]==0x00 && data[3]==0x00) return "TrueType";
    if (data[0]=='O' && data[1]=='T' && data[2]=='T' && data[3]=='O') return "CFF";
    if (data[0]=='t' && data[1]=='r' && data[2]=='u' && data[3]=='e') return "TrueType (Apple)";
    return "unknown";
}

} // namespace fontyde
