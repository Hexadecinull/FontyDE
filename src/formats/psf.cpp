//
// FontyDE — Decompiler Edition
// src/formats/psf.cpp
//
// PSF1: magic 0x36 0x04, fixed 8-wide, 256 or 512 glyphs.
// PSF2: magic 0x72 0xb5 0x4a 0x86, variable width/height, unicode table.
//

#include "fontyde/formats/psf.h"
#include "fontyde/reader.h"
#include "fontyde/emitter/fyde_emitter.h"
#include <fstream>

namespace fontyde {

static const u8 PSF1_MAGIC[] = { 0x36, 0x04 };
static const u8 PSF2_MAGIC[] = { 0x72, 0xb5, 0x4a, 0x86 };

static constexpr u8 PSF1_MODE512       = 0x01;
static constexpr u8 PSF1_MODEHASTAB    = 0x02;
static constexpr u8 PSF1_MODEHASSEQ    = 0x04;

static constexpr u32 PSF2_HAS_UNICODE_TABLE = 0x01;

PsfFont parse_psf(const std::vector<u8>& data, const std::string&) {
    PsfFont font = {};
    BinaryReader r(data);
    r.set_big_endian(false);

    if (data.size() >= 2 && data[0] == PSF1_MAGIC[0] && data[1] == PSF1_MAGIC[1]) {
        font.version          = 1;
        r.skip(2);
        u8 mode               = r.read_u8();
        u8 charsize           = r.read_u8();
        font.bytes_per_glyph  = charsize;
        font.width            = 8;
        font.height           = charsize;
        font.glyph_count      = (mode & PSF1_MODE512) ? 512 : 256;
        font.has_unicode_table = (mode & PSF1_MODEHASTAB) != 0;

        font.glyphs.resize(font.glyph_count);
        for (u32 i = 0; i < font.glyph_count; i++) {
            font.glyphs[i].index  = i;
            font.glyphs[i].bitmap = r.read_bytes(charsize);
        }

        if (font.has_unicode_table) {
            for (u32 i = 0; i < font.glyph_count && !r.eof(); i++) {
                while (!r.eof()) {
                    u16 uc = r.read_u16();
                    if (uc == 0xFFFF) break;
                    if (uc == 0xFFFE) continue;
                    font.glyphs[i].codepoints.push_back(uc);
                }
            }
        }
    } else if (data.size() >= 4 &&
               data[0]==PSF2_MAGIC[0] && data[1]==PSF2_MAGIC[1] &&
               data[2]==PSF2_MAGIC[2] && data[3]==PSF2_MAGIC[3]) {
        font.version = 2;
        r.skip(4);
        u32 hdr_version       = r.read_u32();
        u32 header_size       = r.read_u32();
        font.flags            = r.read_u32();
        font.glyph_count      = r.read_u32();
        font.bytes_per_glyph  = r.read_u32();
        font.height           = r.read_u32();
        font.width            = r.read_u32();
        font.has_unicode_table = (font.flags & PSF2_HAS_UNICODE_TABLE) != 0;
        (void)hdr_version;

        if (header_size > 32) r.skip(header_size - 32);

        font.glyphs.resize(font.glyph_count);
        for (u32 i = 0; i < font.glyph_count && !r.eof(); i++) {
            font.glyphs[i].index  = i;
            font.glyphs[i].bitmap = r.read_bytes(
                std::min(static_cast<size_t>(font.bytes_per_glyph), r.size() - r.pos()));
        }

        if (font.has_unicode_table && !r.eof()) {
            for (u32 i = 0; i < font.glyph_count && !r.eof(); i++) {
                while (!r.eof()) {
                    u8 b0 = r.read_u8();
                    if (b0 == 0xFF) break;
                    u32 cp = 0;
                    if (b0 < 0x80) {
                        cp = b0;
                    } else if ((b0 & 0xE0) == 0xC0 && !r.eof()) {
                        cp = ((b0 & 0x1F) << 6) | (r.read_u8() & 0x3F);
                    } else if ((b0 & 0xF0) == 0xE0 && r.size() - r.pos() >= 2) {
                        cp = ((b0 & 0x0F) << 12) |
                             ((r.read_u8() & 0x3F) << 6) |
                             (r.read_u8() & 0x3F);
                    } else if ((b0 & 0xF8) == 0xF0 && r.size() - r.pos() >= 3) {
                        cp = ((b0 & 0x07) << 18) |
                             ((r.read_u8() & 0x3F) << 12) |
                             ((r.read_u8() & 0x3F) << 6) |
                             (r.read_u8() & 0x3F);
                    }
                    font.glyphs[i].codepoints.push_back(cp);
                }
            }
        }
    } else {
        throw ParseError("PSF: unrecognized magic bytes");
    }

    return font;
}

void emit_psf_to_file(const PsfFont& psf, const std::string& src,
                      const std::string& out_path, const EmitOptions& opts) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter e(f, opts);
    e.emit_psf(psf, src, opts);
}

} // namespace fontyde
