//
// FontyDE — Decompiler Edition
// src/main.cpp
//

#include "fontyde/types.h"
#include "fontyde/detector.h"
#include "fontyde/font.h"
#include "fontyde/formats/ttf.h"
#include "fontyde/formats/woff.h"
#include "fontyde/formats/woff2.h"
#include "fontyde/formats/type1.h"
#include "fontyde/formats/bdf.h"
#include "fontyde/formats/fnt.h"
#include "fontyde/formats/angelcode.h"
#include "fontyde/formats/psf.h"
#include "fontyde/formats/pcf.h"
#include "fontyde/formats/afm.h"
#include "fontyde/formats/svgfont.h"
#include "fontyde/emitter/fyde_emitter.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <sys/types.h>
#  define MKDIR(p) mkdir((p), 0755)
#endif

namespace fontyde {

static std::string path_stem(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    std::string filename = path.substr(start);
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos) filename = filename.substr(0, dot);
    return filename;
}

static std::string path_join(const std::string& dir, const std::string& file) {
    if (dir.empty() || dir == ".") return file;
#ifdef _WIN32
    if (dir.back() == '\\' || dir.back() == '/') return dir + file;
    return dir + "\\" + file;
#else
    if (dir.back() == '/') return dir + file;
    return dir + "/" + file;
#endif
}

static bool dir_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static void ensure_dir(const std::string& path) {
    if (!path.empty() && !dir_exists(path)) MKDIR(path.c_str());
}

} // namespace fontyde

using namespace fontyde;

static void print_usage(const char* prog) {
    std::cerr <<
        "FontyDE - Decompiler Edition v1.0.0\n"
        "Decompiles font files into annotated human-readable .fyde source.\n\n"
        "Usage:\n"
        "  " << prog << " [options] <font_file> [font_file ...]\n\n"
        "Options:\n"
        "  -o, --output <dir>    Output directory (default: stdout)\n"
        "  --no-glyphs           Skip glyph outline data\n"
        "  --no-cmap             Skip codepoint mapping table\n"
        "  --no-kern             Skip kerning pair table\n"
        "  --no-bitmaps          Skip bitmap rows (BDF/PSF/PCF/AngelCode)\n"
        "  --ttc-index <n>       For TTC/OTC, decompile only font at index n\n"
        "  -h, --help            Print this message\n\n"
        "Supported Formats:\n"
        "  .ttf .otf .woff .woff2 .ttc .otc\n"
        "  .pfb .pfa  (Type 1)\n"
        "  .bdf       (X11 Bitmap)\n"
        "  .fon .fnt  (Windows FNT/FON or AngelCode BMFont)\n"
        "  .psf .psf2 (PC Screen Font / Linux console)\n"
        "  .pcf       (Portable Compiled Format / X11)\n"
        "  .afm       (Adobe Font Metrics)\n"
        "  .svg       (SVG Font)\n\n";
}

static std::vector<u8> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FontyError("cannot open file: " + path);
    f.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<u8> buf(size);
    if (size > 0 && !f.read(reinterpret_cast<char*>(buf.data()),
                             static_cast<std::streamsize>(size)))
        throw FontyError("read failed: " + path);
    return buf;
}

static std::string make_out_path(const std::string& in_path,
                                  const std::string& out_dir, int index = -1) {
    std::string stem = path_stem(in_path);
    if (index >= 0) stem += "_" + std::to_string(index);
    return path_join(out_dir, stem + ".fyde");
}

static void emit_font_to(const Font& font, const std::string& out_path,
                          const EmitOptions& opts) {
    if (out_path.empty()) {
        FydeEmitter e(std::cout, opts);
        e.emit_font(font);
    } else {
        emit_to_file(font, out_path, opts);
        std::cerr << "  => " << out_path << "\n";
    }
}

static void decompile_one(const std::vector<u8>& data, const std::string& path,
                           FontFormat fmt, const std::string& out_path,
                           const EmitOptions& opts) {
    switch (fmt) {
        case FontFormat::TrueType:
        case FontFormat::OpenType_CFF:
        case FontFormat::OpenType_CFF2:
            emit_font_to(parse_sfnt(data, path), out_path, opts); break;

        case FontFormat::WOFF:
            emit_font_to(parse_woff(data, path), out_path, opts); break;

        case FontFormat::WOFF2:
            emit_font_to(parse_woff2(data, path), out_path, opts); break;

        case FontFormat::Type1_PFB:
            emit_font_to(parse_type1_pfb(data, path), out_path, opts); break;

        case FontFormat::Type1_PFA:
            emit_font_to(parse_type1_pfa(data, path), out_path, opts); break;

        case FontFormat::BDF: {
            auto bdf = parse_bdf_raw(data);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_bdf(bdf, path); }
            else { emit_bdf_to_file(bdf, path, out_path); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        case FontFormat::FNT:
            emit_font_to(parse_fnt(data, path), out_path, opts); break;

        case FontFormat::FON:
            emit_font_to(parse_fon(data, path), out_path, opts); break;

        case FontFormat::AngelCode: {
            auto bmf = parse_angelcode(data, path);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_angelcode(bmf, path, opts); }
            else { emit_angelcode_to_file(bmf, path, out_path, opts); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        case FontFormat::PSF: {
            auto psf = parse_psf(data, path);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_psf(psf, path, opts); }
            else { emit_psf_to_file(psf, path, out_path, opts); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        case FontFormat::PCF: {
            auto pcf = parse_pcf(data, path);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_pcf(pcf, path); }
            else { emit_pcf_to_file(pcf, path, out_path); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        case FontFormat::AFM: {
            auto afm = parse_afm(data, path);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_afm(afm, path); }
            else { emit_afm_to_file(afm, path, out_path); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        case FontFormat::SVGFont: {
            auto svg = parse_svg_font(data, path);
            if (out_path.empty()) { FydeEmitter e(std::cout, opts); e.emit_svg_font(svg, path); }
            else { emit_svg_font_to_file(svg, path, out_path); std::cerr << "  => " << out_path << "\n"; }
            break;
        }
        default:
            throw UnsupportedError(std::string("format not yet supported: ") + format_name(fmt));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::vector<std::string> inputs;
    std::string output_dir;
    EmitOptions opts;
    int ttc_index = -1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")            { print_usage(argv[0]); return 0; }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) { output_dir = argv[++i]; }
        else if (arg == "--no-glyphs")  { opts.emit_glyphs      = false; }
        else if (arg == "--no-cmap")    { opts.emit_cmap        = false; }
        else if (arg == "--no-kern")    { opts.emit_kerning     = false; }
        else if (arg == "--no-bitmaps") { opts.emit_bitmap_rows = false; }
        else if (arg == "--ttc-index" && i + 1 < argc) { ttc_index = std::stoi(argv[++i]); }
        else if (!arg.empty() && arg[0] != '-') { inputs.push_back(arg); }
        else { std::cerr << "Unknown option: " << arg << "\n"; print_usage(argv[0]); return 1; }
    }

    if (inputs.empty()) { std::cerr << "Error: no input files.\n"; print_usage(argv[0]); return 1; }
    if (!output_dir.empty()) ensure_dir(output_dir);

    bool any_error = false;
    for (auto& path : inputs) {
        std::cerr << "FontyDE: processing " << path << " ...\n";
        try {
            auto data = read_file(path);
            FontFormat fmt = detect_format(data);
            if (fmt == FontFormat::Unknown) fmt = detect_format_from_path(path);
            if (fmt == FontFormat::Unknown)
                throw UnsupportedError("cannot determine format of: " + path);
            std::cerr << "  format: " << format_name(fmt) << "\n";

            if (fmt == FontFormat::TTC || fmt == FontFormat::OTC) {
                auto fonts = parse_ttc(data, path);
                std::cerr << "  collection: " << fonts.size() << " font(s)\n";
                for (size_t idx = 0; idx < fonts.size(); idx++) {
                    if (ttc_index >= 0 && static_cast<size_t>(ttc_index) != idx) continue;
                    std::string out = output_dir.empty() ? ""
                        : make_out_path(path, output_dir, static_cast<int>(idx));
                    std::cerr << "  [" << idx << "] " << fonts[idx].family_name() << "\n";
                    emit_font_to(fonts[idx], out, opts);
                }
            } else {
                std::string out = output_dir.empty() ? "" : make_out_path(path, output_dir);
                decompile_one(data, path, fmt, out, opts);
            }
        } catch (const FontyError& e) {
            std::cerr << "Error: " << e.what() << "\n"; any_error = true;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected error: " << e.what() << "\n"; any_error = true;
        }
    }
    return any_error ? 1 : 0;
}
