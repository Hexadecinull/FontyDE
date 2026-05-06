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
#include "fontyde/emitter/fyde_emitter.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
using namespace fontyde;

static void print_usage(const char* prog) {
    std::cerr <<
        "FontyDE — Decompiler Edition v1.0.0\n"
        "Decompiles font files into annotated human-readable source.\n\n"
        "Usage:\n"
        "  " << prog << " [options] <font_file> [font_file ...]\n\n"
        "Options:\n"
        "  -o, --output <path>   Output file or directory (default: stdout)\n"
        "  --no-glyphs           Skip glyph outline data\n"
        "  --no-cmap             Skip codepoint mapping table\n"
        "  --no-kern             Skip kerning pair table\n"
        "  --no-bitmaps          Skip bitmap row data (BDF/FNT)\n"
        "  --ttc-index <n>       For TTC/OTC, decompile font at index n (default: all)\n"
        "  -h, --help            Print this message\n\n"
        "Supported Formats:\n"
        "  .ttf  TrueType\n"
        "  .otf  OpenType/CFF\n"
        "  .woff WOFF (zlib compressed)\n"
        "  .woff2 WOFF2 (brotli, requires -DFONTYDE_WOFF2=ON)\n"
        "  .ttc / .otc  TrueType/OpenType Collection\n"
        "  .pfb  Type 1 binary\n"
        "  .pfa  Type 1 ASCII\n"
        "  .bdf  X11 Bitmap Distribution Format\n"
        "  .fnt  Windows FNT\n"
        "  .fon  Windows FON (NE DLL)\n\n"
        "Output Format:\n"
        "  .fyde — FontyDE annotated source. Each table is a named block.\n"
        "  Fields include comments explaining every value's meaning.\n"
        "  Glyph contours are printed point-by-point with curve type annotations.\n\n"
        "Examples:\n"
        "  " << prog << " Inter.ttf\n"
        "  " << prog << " --no-glyphs -o out/ font.otf font2.woff\n"
        "  " << prog << " --ttc-index 0 NotoSans.ttc\n";
}

static std::vector<u8> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw FontyError("cannot open file: " + path);
    f.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<u8> buf(size);
    if (!f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size)))
        throw FontyError("read failed: " + path);
    return buf;
}

static std::string output_name(const std::string& in_path,
                                const std::string& out_dir,
                                int index = -1) {
    fs::path p(in_path);
    std::string stem = p.stem().string();
    if (index >= 0) stem += "_" + std::to_string(index);
    fs::path out = out_dir.empty() ? p.parent_path() : fs::path(out_dir);
    return (out / (stem + ".fyde")).string();
}

static void decompile_one(const std::vector<u8>& data,
                           const std::string& path,
                           FontFormat fmt,
                           const std::string& out_path,
                           const EmitOptions& opts) {
    Font font;

    switch (fmt) {
        case FontFormat::TrueType:
        case FontFormat::OpenType_CFF:
        case FontFormat::OpenType_CFF2:
            font = parse_sfnt(data, path);
            break;
        case FontFormat::WOFF:
            font = parse_woff(data, path);
            break;
        case FontFormat::WOFF2:
            font = parse_woff2(data, path);
            break;
        case FontFormat::Type1_PFB:
            font = parse_type1_pfb(data, path);
            break;
        case FontFormat::Type1_PFA:
            font = parse_type1_pfa(data, path);
            break;
        case FontFormat::BDF: {
            auto bdf = parse_bdf_raw(data);
            if (out_path.empty()) {
                FydeEmitter emitter(std::cout, opts);
                emitter.emit_bdf(bdf, path);
            } else {
                emit_bdf_to_file(bdf, path, out_path);
                std::cerr << "  => " << out_path << "\n";
            }
            return;
        }
        case FontFormat::FNT:
            font = parse_fnt(data, path);
            break;
        case FontFormat::FON:
            font = parse_fon(data, path);
            break;
        default:
            throw UnsupportedError("format not yet supported: " + std::string(format_name(fmt)));
    }

    if (out_path.empty()) {
        FydeEmitter emitter(std::cout, opts);
        emitter.emit_font(font);
    } else {
        emit_to_file(font, out_path, opts);
        std::cerr << "  => " << out_path << "\n";
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
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]); return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--no-glyphs") {
            opts.emit_glyphs = false;
        } else if (arg == "--no-cmap") {
            opts.emit_cmap = false;
        } else if (arg == "--no-kern") {
            opts.emit_kerning = false;
        } else if (arg == "--no-bitmaps") {
            opts.emit_bitmap_rows = false;
        } else if (arg == "--ttc-index" && i + 1 < argc) {
            ttc_index = std::stoi(argv[++i]);
        } else if (!arg.empty() && arg[0] != '-') {
            inputs.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (inputs.empty()) {
        std::cerr << "Error: no input files specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!output_dir.empty() && !fs::exists(output_dir)) {
        try { fs::create_directories(output_dir); }
        catch (...) {
            std::cerr << "Error: cannot create output directory: " << output_dir << "\n";
            return 1;
        }
    }

    bool any_error = false;

    for (auto& path : inputs) {
        std::cerr << "FontyDE: processing " << path << " ...\n";
        try {
            auto data = read_file(path);
            FontFormat fmt = detect_format(data);
            if (fmt == FontFormat::Unknown) {
                fmt = detect_format_from_path(path);
            }
            if (fmt == FontFormat::Unknown) {
                throw UnsupportedError("cannot determine format of: " + path);
            }

            std::cerr << "  format: " << format_name(fmt) << "\n";

            if (fmt == FontFormat::TTC || fmt == FontFormat::OTC) {
                auto fonts = parse_ttc(data, path);
                std::cerr << "  collection: " << fonts.size() << " font(s)\n";

                for (size_t idx = 0; idx < fonts.size(); idx++) {
                    if (ttc_index >= 0 && static_cast<size_t>(ttc_index) != idx) continue;
                    std::string out = output_dir.empty()
                        ? ""
                        : output_name(path, output_dir, static_cast<int>(idx));
                    std::cerr << "  [" << idx << "] " << fonts[idx].family_name() << "\n";
                    if (output_dir.empty()) {
                        FydeEmitter emitter(std::cout, opts);
                        emitter.emit_font(fonts[idx]);
                    } else {
                        emit_to_file(fonts[idx], out, opts);
                        std::cerr << "    => " << out << "\n";
                    }
                }
            } else {
                std::string out = output_dir.empty()
                    ? ""
                    : output_name(path, output_dir);
                decompile_one(data, path, fmt, out, opts);
            }

        } catch (const FontyError& e) {
            std::cerr << "Error: " << e.what() << "\n";
            any_error = true;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected error: " << e.what() << "\n";
            any_error = true;
        }
    }

    return any_error ? 1 : 0;
}
