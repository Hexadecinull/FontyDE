//
// FontyDE — Decompiler Edition
// src/formats/fnt.cpp
//
// Windows FNT is a legacy bitmap font format used by Windows 1.x–3.x.
// FON is a NE-format (New Executable) DLL that bundles one or more FNT resources.
// The NE format predates PE; it uses a segment table and resource table.
//

#include "fontyde/formats/fnt.h"
#include "fontyde/reader.h"

namespace fontyde {

static Font parse_fnt_raw(BinaryReader& r, const std::string& path, FontFormat fmt) {
    Font font;
    font.format      = fmt;
    font.source_path = path;

    FntHeader hdr = {};
    hdr.version    = r.read_u16();
    hdr.file_size  = r.read_u32();

    for (int i = 0; i < 60; i++) hdr.copyright[i] = static_cast<char>(r.read_u8());

    hdr.type            = r.read_u16();
    hdr.points          = r.read_u16();
    hdr.vert_res        = r.read_u16();
    hdr.horiz_res       = r.read_u16();
    hdr.ascent          = r.read_u16();
    hdr.internal_leading = r.read_u16();
    hdr.external_leading = r.read_u16();
    hdr.italic          = r.read_u8();
    hdr.underline       = r.read_u8();
    hdr.strike_out      = r.read_u8();
    hdr.weight          = r.read_u16();
    hdr.char_set        = r.read_u8();
    hdr.pix_width       = r.read_u16();
    hdr.pix_height      = r.read_u16();
    hdr.pitch_and_family = r.read_u8();
    hdr.avg_width       = r.read_u16();
    hdr.max_width       = r.read_u16();
    hdr.first_char      = r.read_u8();
    hdr.last_char       = r.read_u8();
    hdr.default_char    = r.read_u8();
    hdr.break_char      = r.read_u8();
    hdr.width_bytes     = r.read_u16();
    hdr.device_offset   = r.read_u32();
    hdr.face_offset     = r.read_u32();
    hdr.bits_pointer    = r.read_u32();
    hdr.bits_offset     = r.read_u32();
    hdr.reserved        = r.read_u8();

    if (hdr.version >= 0x0300) {
        hdr.flags   = r.read_u32();
        hdr.a_space = r.read_u16();
        hdr.b_space = r.read_u16();
        hdr.c_space = r.read_u16();
    }

    NameTable name_tbl;
    name_tbl.format = 0;
    if (hdr.face_offset > 0 && hdr.face_offset < r.size()) {
        BinaryReader fr = r.sub_reader(hdr.face_offset, r.size() - hdr.face_offset);
        std::string face_name;
        while (!fr.eof()) {
            char c = static_cast<char>(fr.read_u8());
            if (c == 0) break;
            face_name += c;
        }
        if (!face_name.empty()) {
            NameRecord nr;
            nr.platform_id = 1; nr.encoding_id = 0; nr.language_id = 0;
            nr.name_id = 4; nr.value = face_name;
            nr.length = static_cast<u16>(face_name.size()); nr.offset = 0;
            name_tbl.records.push_back(nr);
            name_tbl.count = 1;
        }
    }
    font.name = std::move(name_tbl);

    HeadTable head = {};
    head.major_version  = 1;
    head.units_per_em   = hdr.pix_height;
    head.x_max          = static_cast<i16>(hdr.max_width);
    head.y_max          = static_cast<i16>(hdr.ascent);
    font.head = head;

    MaxpTable maxp = {};
    maxp.version    = 0x00005000;
    maxp.num_glyphs = static_cast<u16>(hdr.last_char - hdr.first_char + 2);
    font.maxp = maxp;

    return font;
}

Font parse_fnt(const std::vector<u8>& data, const std::string& path) {
    BinaryReader r(data);
    r.set_big_endian(false);
    return parse_fnt_raw(r, path, FontFormat::FNT);
}

Font parse_fon(const std::vector<u8>& data, const std::string& path) {
    BinaryReader r(data);
    r.set_big_endian(false);

    if (data.size() < 0x40) throw ParseError("FON: file too small");

    u16 mz_magic = r.read_u16();
    if (mz_magic != 0x5A4D) throw ParseError("FON: missing MZ header");

    r.seek(0x3C);
    u32 ne_offset = r.read_u32();

    if (ne_offset + 2 > data.size()) throw ParseError("FON: NE offset out of bounds");

    r.seek(ne_offset);
    u16 ne_magic = r.read_u16();
    if (ne_magic != 0x454E && ne_magic != 0x4550)
        throw ParseError("FON: not a NE/PE executable");

    r.seek(ne_offset + 0x24);
    u16 res_tbl_offset = r.read_u16();

    size_t res_tbl_pos = ne_offset + res_tbl_offset;
    if (res_tbl_pos + 2 > data.size()) throw ParseError("FON: resource table out of bounds");

    r.seek(res_tbl_pos);
    u16 align_shift = r.read_u16();

    while (!r.eof()) {
        u16 type_id = r.read_u16();
        if (type_id == 0) break;
        u16 count = r.read_u16();
        r.skip(4);

        for (u16 i = 0; i < count; i++) {
            u16 offset_shifted = r.read_u16();
            u16 length_shifted = r.read_u16();
            r.skip(8);
            u32 res_offset = static_cast<u32>(offset_shifted) << align_shift;
            u32 res_length = static_cast<u32>(length_shifted) << align_shift;

            if (type_id == 0x8008 && res_offset + res_length <= data.size()) {
                std::vector<u8> fnt_data(data.begin() + res_offset,
                                         data.begin() + res_offset + res_length);
                try {
                    return parse_fnt(fnt_data, path);
                } catch (...) {}
            }
        }
    }

    Font font;
    font.format      = FontFormat::FON;
    font.source_path = path;
    return font;
}

} // namespace fontyde
