//
// FontyDE — Decompiler Edition
// src/tables/name.cpp
//

#include "fontyde/tables/name.h"
#include <cstring>

namespace fontyde {

static std::string decode_name_string(const u8* data, u16 len, u16 platform_id, u16 encoding_id) {
    if (platform_id == 0 || (platform_id == 3 && encoding_id == 1) ||
        (platform_id == 3 && encoding_id == 10)) {
        std::string result;
        for (u16 i = 0; i + 1 < len; i += 2) {
            u16 cp = (static_cast<u16>(data[i]) << 8) | data[i + 1];
            if (cp < 0x80) {
                result += static_cast<char>(cp);
            } else if (cp < 0x800) {
                result += static_cast<char>(0xC0 | (cp >> 6));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (cp >> 12));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        return result;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
}

const char* NameRecord::name_id_str(u16 id) {
    switch (id) {
        case 0:  return "Copyright Notice";
        case 1:  return "Family Name";
        case 2:  return "Subfamily Name";
        case 3:  return "Unique Font Identifier";
        case 4:  return "Full Font Name";
        case 5:  return "Version String";
        case 6:  return "PostScript Name";
        case 7:  return "Trademark";
        case 8:  return "Manufacturer";
        case 9:  return "Designer";
        case 10: return "Description";
        case 11: return "Vendor URL";
        case 12: return "Designer URL";
        case 13: return "License Description";
        case 14: return "License Info URL";
        case 16: return "Typographic Family Name";
        case 17: return "Typographic Subfamily Name";
        case 18: return "Compatible Full (Mac)";
        case 19: return "Sample Text";
        case 20: return "PostScript CID findfont Name";
        case 21: return "WWS Family Name";
        case 22: return "WWS Subfamily Name";
        case 23: return "Light Background Palette";
        case 24: return "Dark Background Palette";
        case 25: return "Variations PostScript Name Prefix";
        default: return "Reserved";
    }
}

const char* NameRecord::platform_str(u16 id) {
    switch (id) {
        case 0: return "Unicode";
        case 1: return "Macintosh";
        case 2: return "ISO (deprecated)";
        case 3: return "Windows";
        case 4: return "Custom";
        default: return "Unknown";
    }
}

NameTable NameTable::parse(BinaryReader& r) {
    NameTable t;
    size_t table_start = r.pos();

    t.format         = r.read_u16();
    t.count          = r.read_u16();
    t.storage_offset = r.read_u16();

    t.records.resize(t.count);
    for (auto& nr : t.records) {
        nr.platform_id = r.read_u16();
        nr.encoding_id = r.read_u16();
        nr.language_id = r.read_u16();
        nr.name_id     = r.read_u16();
        nr.length      = r.read_u16();
        nr.offset      = r.read_u16();
    }

    if (t.format == 1) {
        u16 lang_tag_count = r.read_u16();
        t.lang_tags.resize(lang_tag_count);
        for (auto& lt : t.lang_tags) {
            lt.length = r.read_u16();
            lt.offset = r.read_u16();
        }
        for (auto& lt : t.lang_tags) {
            size_t str_pos = table_start + t.storage_offset + lt.offset;
            lt.tag = std::string(reinterpret_cast<const char*>(r.raw_at(str_pos)), lt.length);
        }
    }

    for (auto& nr : t.records) {
        size_t str_pos = table_start + t.storage_offset + nr.offset;
        nr.value = decode_name_string(r.raw_at(str_pos), nr.length, nr.platform_id, nr.encoding_id);
    }

    return t;
}

} // namespace fontyde
