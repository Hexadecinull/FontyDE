//
// FontyDE — Decompiler Edition
// src/emitter/fyde_emitter.cpp
//
// The emitter walks the parsed Font model and outputs a human-readable
// annotated text file — the "decompiled source" — in a custom .fyde format.
// The format is designed to be diff-friendly, comment-rich, and re-parseable.
//

#include "fontyde/emitter/fyde_emitter.h"
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>

namespace fontyde {

static std::string hex32(u32 v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

static std::string hex16(u16 v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << v;
    return oss.str();
}

static std::string to_str(int v)    { return std::to_string(v); }
static std::string to_str(u16 v)    { return std::to_string(v); }
static std::string to_str(u32 v)    { return std::to_string(v); }
static std::string to_str(i16 v)    { return std::to_string(v); }
static std::string to_str(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << v;
    return oss.str();
}

static std::string point_type_str(PointType t) {
    switch (t) {
        case PointType::OnCurve:  return "on";
        case PointType::OffCurve: return "off";
        case PointType::Cubic:    return "cubic";
    }
    return "?";
}

static std::string unicode_name(u32 cp) {
    if (cp == 0x20)   return "SPACE";
    if (cp == 0x0D)   return "CARRIAGE RETURN";
    if (cp == 0x0A)   return "LINE FEED";
    if (cp >= 0x21 && cp <= 0x7E) {
        return std::string("LATIN ") + static_cast<char>(cp);
    }
    return "";
}

FydeEmitter::FydeEmitter(std::ostream& out, EmitOptions opts)
    : out_(out), opts_(std::move(opts)), indent_(0) {}

std::string FydeEmitter::indent_str() const {
    return std::string(static_cast<size_t>(indent_) * 2, ' ');
}

void FydeEmitter::line(const std::string& s)  { out_ << indent_str() << s << "\n"; }
void FydeEmitter::blank()                      { out_ << "\n"; }

void FydeEmitter::open(const std::string& block) {
    out_ << indent_str() << block << " {\n";
    indent_++;
}

void FydeEmitter::close() {
    indent_--;
    out_ << indent_str() << "}\n";
}

void FydeEmitter::comment(const std::string& c) {
    out_ << indent_str() << "# " << c << "\n";
}

void FydeEmitter::kv(const std::string& key, const std::string& val, const std::string& cmt) {
    out_ << indent_str() << key << " = " << val;
    if (!cmt.empty()) out_ << "  # " << cmt;
    out_ << "\n";
}

void FydeEmitter::kv_hex(const std::string& key, uint64_t val, const std::string& cmt) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << val;
    kv(key, oss.str(), cmt);
}

void FydeEmitter::section(const std::string& title) {
    blank();
    out_ << indent_str() << "# ── " << title << " ──\n";
}

void FydeEmitter::emit_file_header(const Font& font) {
    out_ << "# ============================================================\n";
    out_ << "# FontyDE Decompiled Font Source\n";
    out_ << "# Format  : " << format_name(font.format) << "\n";
    out_ << "# Family  : " << font.family_name() << "\n";
    out_ << "# Glyphs  : " << font.num_glyphs() << "\n";
    out_ << "# Source  : " << font.source_path << "\n";
    out_ << "# Tables  : ";
    bool first = true;
    for (auto& [tag, _] : font.table_map) {
        if (!first) out_ << ", ";
        out_ << tag;
        first = false;
    }
    out_ << "\n";
    out_ << "# ============================================================\n\n";

    open("font");
    kv("format", std::string("\"") + format_name(font.format) + "\"");
    kv("source", "\"" + font.source_path + "\"");
}

void FydeEmitter::emit_head(const HeadTable& t) {
    section("head — Font Header Table");
    comment("Defined in OpenType spec §4.5 / ISO 14496-22 §5.2.2");
    comment("magic_number 0x5F0F3CF5 is always this value — validates file integrity");
    open("head");
    kv("version",            to_str(t.major_version) + "." + to_str(t.minor_version));
    kv("font_revision",      to_str(fixed_to_double(t.font_revision)),
                             "16.16 fixed-point version number set by font designer");
    kv_hex("checksum_adjustment", t.checksum_adjustment,
                             "0xB1B0AFBA - (sum of all table checksums)");
    kv_hex("magic_number",   t.magic_number, "must always be 0x5F0F3CF5");
    kv_hex("flags",          t.flags,        "bit field — see OpenType spec §head");
    kv("units_per_em",       to_str(t.units_per_em),
                             "coordinate space resolution; typically 1000 (CFF) or 2048 (TTF)");
    kv("created",            mac_epoch_to_iso(t.created),  "seconds since 1904-01-01");
    kv("modified",           mac_epoch_to_iso(t.modified), "seconds since 1904-01-01");
    kv("x_min",              to_str(t.x_min), "global bounding box in font units");
    kv("y_min",              to_str(t.y_min));
    kv("x_max",              to_str(t.x_max));
    kv("y_max",              to_str(t.y_max));
    kv_hex("mac_style",      t.mac_style,
           (t.mac_style & 1 ? "Bold " : "") +
           std::string(t.mac_style & 2 ? "Italic " : "") +
           std::string(t.mac_style & 4 ? "Underline " : ""));
    kv("lowest_rec_ppem",    to_str(t.lowest_rec_ppem),
                             "smallest readable pixel size");
    kv("index_to_loc_format", t.index_to_loc_format == 0 ? "short" : "long",
                             "0=uint16 halved offsets in loca, 1=uint32 offsets");
    kv("glyph_data_format",  to_str(t.glyph_data_format), "must be 0");
    close();
}

void FydeEmitter::emit_name(const NameTable& t) {
    section("name — Naming Table");
    comment("Stores font family, style, copyright, version, and other metadata strings");
    comment("Platform 1=Mac, 3=Windows; Encoding 1=UCS-2 (Win); Language 0x0409=en-US");
    open("name");
    kv("format", to_str(t.format), "0=basic, 1=extended with language tags");
    for (auto& nr : t.records) {
        open("record");
        kv("platform", to_str(nr.platform_id),
           std::string(NameRecord::platform_str(nr.platform_id)));
        kv("encoding", to_str(nr.encoding_id));
        kv("language", hex16(nr.language_id));
        kv("name_id",  to_str(nr.name_id),
           std::string(NameRecord::name_id_str(nr.name_id)));
        kv("value",    "\"" + nr.value + "\"");
        close();
    }
    close();
}

void FydeEmitter::emit_cmap(const CmapTable& t) {
    section("cmap — Character to Glyph Index Mapping");
    comment("Maps Unicode codepoints to glyph IDs. Format 4 covers BMP (U+0000–U+FFFF).");
    comment("Format 12 covers full Unicode including supplementary planes (U+10000+).");
    open("cmap");
    kv("version", to_str(t.version));
    for (auto& st : t.subtables) {
        open("subtable");
        kv("platform", to_str(st.platform_id));
        kv("encoding", to_str(st.encoding_id));
        kv("format",   to_str(st.format));
        kv("mappings", to_str(static_cast<u32>(st.mapping.size())),
           "total codepoint-to-glyph-id pairs");
        if (opts_.emit_cmap) {
            for (auto& [cp, gid] : st.mapping) {
                std::ostringstream entry;
                entry << "U+" << std::hex << std::uppercase
                      << std::setw(4) << std::setfill('0') << cp
                      << " -> " << std::dec << gid;
                std::string uname = unicode_name(cp);
                line(entry.str() + (uname.empty() ? "" : "  # " + uname));
            }
        }
        close();
    }
    close();
}

void FydeEmitter::emit_hhea(const HheaTable& t) {
    section("hhea — Horizontal Header");
    comment("Defines metrics for horizontally laid-out glyphs (Latin, CJK, etc.)");
    open("hhea");
    kv("version",              to_str(t.major_version) + "." + to_str(t.minor_version));
    kv("ascender",             to_str(t.ascender),  "distance from baseline to top of em square");
    kv("descender",            to_str(t.descender), "distance from baseline to bottom (negative)");
    kv("line_gap",             to_str(t.line_gap),  "typographic line gap");
    kv("advance_width_max",    to_str(t.advance_width_max));
    kv("min_left_side_bearing", to_str(t.min_left_side_bearing));
    kv("min_right_side_bearing", to_str(t.min_right_side_bearing));
    kv("x_max_extent",         to_str(t.x_max_extent));
    kv("caret_slope_rise",     to_str(t.caret_slope_rise),
                               "1=vertical caret, 0=horizontal");
    kv("caret_slope_run",      to_str(t.caret_slope_run));
    kv("caret_offset",         to_str(t.caret_offset));
    kv("number_of_h_metrics",  to_str(t.number_of_h_metrics),
                               "entries in hmtx with explicit advance width");
    close();
}

void FydeEmitter::emit_hmtx(const HmtxTable& t, u16 num_metrics) {
    section("hmtx — Horizontal Metrics");
    comment("advance_width: distance to next glyph origin in font units");
    comment("lsb: left side bearing — x offset from origin to leftmost contour point");
    open("hmtx");
    u16 shown = std::min(num_metrics, static_cast<u16>(t.h_metrics.size()));
    for (u16 i = 0; i < shown; i++) {
        open("glyph_" + to_str(i));
        kv("advance_width", to_str(t.h_metrics[i].advance_width));
        kv("lsb",           to_str(t.h_metrics[i].lsb));
        close();
    }
    if (!t.left_side_bearings.empty()) {
        comment("additional lsb entries sharing advance_width of last h_metric");
        for (size_t i = 0; i < t.left_side_bearings.size(); i++) {
            kv("extra_lsb_" + to_str(static_cast<u32>(i)),
               to_str(t.left_side_bearings[i]));
        }
    }
    close();
}

void FydeEmitter::emit_maxp(const MaxpTable& t) {
    section("maxp — Maximum Profile");
    comment("Defines memory allocation requirements for the font");
    open("maxp");
    kv("version",     to_str(fixed_to_double(t.version)),
       "0.5=CFF fonts (no TrueType hints), 1.0=TrueType");
    kv("num_glyphs",  to_str(t.num_glyphs));
    if (t.version == 0x00010000) {
        kv("max_points",               to_str(t.max_points));
        kv("max_contours",             to_str(t.max_contours));
        kv("max_composite_points",     to_str(t.max_composite_points));
        kv("max_composite_contours",   to_str(t.max_composite_contours));
        kv("max_zones",                to_str(t.max_zones),
           "1=no twilight zone, 2=twilight zone used");
        kv("max_twilight_points",      to_str(t.max_twilight_points));
        kv("max_storage",              to_str(t.max_storage));
        kv("max_function_defs",        to_str(t.max_function_defs));
        kv("max_instruction_defs",     to_str(t.max_instruction_defs));
        kv("max_stack_elements",       to_str(t.max_stack_elements));
        kv("max_size_of_instructions", to_str(t.max_size_of_instructions));
        kv("max_component_elements",   to_str(t.max_component_elements));
        kv("max_component_depth",      to_str(t.max_component_depth),
           "max nesting depth of composite glyphs");
    }
    close();
}

void FydeEmitter::emit_post(const PostTable& t) {
    section("post — PostScript Name Mapping");
    comment("Maps glyph IDs to PostScript glyph names (e.g. 'A', 'space', 'uni0041')");
    comment("version 2.0 uses custom string data; 1.0 uses the Mac standard glyph set");
    open("post");
    kv("version",             to_str(fixed_to_double(t.version)));
    kv("italic_angle",        to_str(fixed_to_double(t.italic_angle)),
                              "degrees counter-clockwise from vertical; 0=upright");
    kv("underline_position",  to_str(t.underline_position));
    kv("underline_thickness", to_str(t.underline_thickness));
    kv("is_fixed_pitch",      t.is_fixed_pitch ? "true" : "false",
                              "monospaced font flag");
    if (t.version == 0x00020000 && !t.glyph_name_index.empty()) {
        comment("glyph name index — maps GID to Mac standard (0-257) or custom string");
        for (u16 gid = 0; gid < static_cast<u16>(t.glyph_name_index.size()); gid++) {
            kv("glyph_" + to_str(gid), "\"" + t.glyph_name(gid) + "\"");
        }
    }
    close();
}

void FydeEmitter::emit_os2(const Os2Table& t) {
    section("OS/2 — Windows Metrics & Classification");
    comment("Required for Windows and Linux; ignored on old Mac. Defines embedding rights,");
    comment("weight class, width class, Unicode ranges, and vertical metrics.");
    open("OS2");
    kv("version",          to_str(t.version));
    kv("x_avg_char_width", to_str(t.x_avg_char_width));
    kv("weight_class",     to_str(t.us_weight_class), t.weight_class_name());
    kv("width_class",      to_str(t.us_width_class),  t.width_class_name());
    kv_hex("fs_type",      t.fs_type,
           "embedding licensing bits: 0x0=installable, 0x2=preview/print, 0x4=editable");
    std::string panose_str;
    for (int i = 0; i < 10; i++) {
        if (i) panose_str += " ";
        panose_str += to_str(static_cast<u16>(t.panose[i]));
    }
    kv("panose",           panose_str, "PANOSE classification [family kind, serif, weight, ...]");
    std::string vend(t.ach_vend_id, 4);
    kv("vendor_id",        "\"" + vend + "\"", "4-char type foundry identifier");
    kv_hex("fs_selection", t.fs_selection,
           (t.fs_selection & 0x01 ? "Italic " : "") +
           std::string(t.fs_selection & 0x20 ? "Bold " : "") +
           std::string(t.fs_selection & 0x40 ? "Regular " : ""));
    kv("typo_ascender",    to_str(t.s_typo_ascender));
    kv("typo_descender",   to_str(t.s_typo_descender));
    kv("typo_line_gap",    to_str(t.s_typo_line_gap));
    kv("win_ascent",       to_str(t.us_win_ascent));
    kv("win_descent",      to_str(t.us_win_descent));
    if (t.version >= 2) {
        kv("x_height",     to_str(t.sx_height),    "height of lowercase x in font units");
        kv("cap_height",   to_str(t.s_cap_height),  "height of capital H in font units");
    }
    close();
}

void FydeEmitter::emit_loca(const LocaTable& t) {
    section("loca — Glyph Location Index");
    comment("Maps GID to byte offset within the glyf table. Not emitted verbatim —");
    comment("glyph offsets are implicit in the glyph blocks below.");
    open("loca");
    kv("count", to_str(static_cast<u32>(t.offsets.size())),
       "num_glyphs + 1 (extra entry gives length of last glyph)");
    close();
}

void FydeEmitter::emit_kern(const KernTable& t) {
    section("kern — Kerning Table");
    comment("Pair-wise advance width adjustments between adjacent glyphs");
    comment("Positive value = glyphs move apart; negative = glyphs move closer");
    open("kern");
    for (size_t i = 0; i < t.subtables.size(); i++) {
        auto& st = t.subtables[i];
        open("subtable_" + to_str(static_cast<u32>(i)));
        kv("format",      to_str(static_cast<u16>(st.format())));
        kv("horizontal",  st.is_horizontal() ? "true" : "false");
        kv("minimum",     st.is_minimum()    ? "true" : "false",
           "if true, value is minimum kern, not accumulate");
        kv("cross_stream", st.is_cross_stream() ? "true" : "false",
           "kerning in perpendicular direction");
        kv("pairs",       to_str(static_cast<u32>(st.pairs.size())));
        if (opts_.emit_kerning) {
            for (auto& p : st.pairs) {
                out_ << indent_str()
                     << "pair  left=" << p.left
                     << " right=" << p.right
                     << " value=" << p.value << "\n";
            }
        }
        close();
    }
    close();
}

void FydeEmitter::emit_glyph(const Glyph& g) {
    open("glyph_" + to_str(g.gid));
    kv("name",          "\"" + g.name + "\"");
    kv("advance_width", to_str(g.advance_width));
    kv("lsb",           to_str(g.lsb));

    if (!g.codepoints.empty()) {
        std::string cps;
        for (size_t i = 0; i < g.codepoints.size(); i++) {
            if (i) cps += " ";
            std::ostringstream oss;
            oss << "U+" << std::hex << std::uppercase
                << std::setw(4) << std::setfill('0') << g.codepoints[i];
            cps += oss.str();
        }
        kv("codepoints", cps);
    }

    kv("bounding_box",
       to_str(g.x_min) + " " + to_str(g.y_min) + " " +
       to_str(g.x_max) + " " + to_str(g.y_max),
       "x_min y_min x_max y_max in font units");

    if (g.is_empty()) {
        comment("empty glyph — no contour data");
    } else if (g.is_composite()) {
        comment("composite glyph — assembled from component glyphs");
        for (auto& comp : g.components) {
            open("component");
            kv("glyph_index", to_str(comp.glyph_index));
            if (comp.args_are_xy_values) {
                kv("translate", to_str(comp.argument1) + " " + to_str(comp.argument2),
                   "dx dy in font units");
            } else {
                kv("anchor_points", to_str(comp.argument1) + " " + to_str(comp.argument2),
                   "parent_point child_point");
            }
            if (comp.has_2x2) {
                kv("matrix",
                   to_str(comp.xx) + " " + to_str(comp.yx) + " " +
                   to_str(comp.xy) + " " + to_str(comp.yy),
                   "2x2 affine transform [xx yx xy yy]");
            } else if (comp.has_scale) {
                kv("scale", to_str(comp.xx));
            }
            kv("use_my_metrics", comp.use_my_metrics ? "true" : "false",
               "inherit advance width from this component");
            close();
        }
    } else {
        comment("simple glyph — " + to_str(static_cast<i32>(g.number_of_contours)) + " contour(s)");
        comment("Contour format: point_index  x  y  on|off|cubic");
        comment("on  = on-curve point (line endpoint or TrueType quadratic anchor)");
        comment("off = off-curve quadratic Bezier control point");
        comment("cubic = off-curve cubic Bezier control point (OpenType 1.9+)");
        for (size_t ci = 0; ci < g.contours.size(); ci++) {
            open("contour_" + to_str(static_cast<u32>(ci)));
            auto& c = g.contours[ci];
            for (size_t pi = 0; pi < c.points.size(); pi++) {
                auto& pt = c.points[pi];
                out_ << indent_str()
                     << pi << "  "
                     << pt.x << "  "
                     << pt.y << "  "
                     << point_type_str(pt.type);
                if (pt.overlap_simple) out_ << "  overlap";
                out_ << "\n";
            }
            close();
        }
        if (!g.instructions.empty()) {
            comment("TrueType hint bytecode (" + to_str(static_cast<u32>(g.instructions.size())) + " bytes) — interpreter instructions for pixel grid fitting");
            out_ << indent_str() << "instructions_hex = ";
            for (size_t i = 0; i < g.instructions.size(); i++) {
                out_ << std::hex << std::uppercase
                     << std::setw(2) << std::setfill('0')
                     << static_cast<unsigned>(g.instructions[i]);
                if ((i + 1) % 16 == 0) out_ << "\n" << indent_str() << "                   ";
                else out_ << " ";
            }
            out_ << std::dec << "\n";
        }
    }
    close();
}

void FydeEmitter::emit_glyphs(const GlyfTable& t) {
    section("glyf — TrueType Glyph Outlines");
    comment("Each glyph is defined in the em square coordinate system.");
    comment("Origin is at the baseline. Y increases upward. X increases rightward.");
    open("glyf");
    if (opts_.emit_glyphs) {
        for (auto& g : t.glyphs) emit_glyph(g);
    } else {
        kv("emit_glyphs", "false", "pass --glyphs to include glyph data");
    }
    close();
}

void FydeEmitter::emit_cff_glyph(const CffGlyph& g) {
    open("glyph_" + to_str(g.gid));
    kv("name", "\"" + g.name + "\"");
    comment("CFF Type 2 charstring — stack-based drawing instructions");
    comment("Operands are pushed onto a numeric stack, consumed by operators.");
    comment("Coordinates are in design units (relative unless noted).");
    for (auto& op : g.ops) {
        out_ << indent_str() << op.mnemonic;
        if (!op.operands.empty()) {
            out_ << "  # args:";
            for (double v : op.operands) {
                out_ << " " << to_str(v);
            }
        }
        out_ << "\n";
    }
    close();
}

void FydeEmitter::emit_cff(const CffTable& t) {
    section("CFF — Compact Font Format Outlines");
    comment("CFF uses PostScript Type 2 charstrings. Each glyph is a sequence of");
    comment("drawing operators: moveto, lineto, curveto, and hint operators.");
    comment("Subr (subroutine) calls have been inlined for clarity.");
    open("CFF");
    kv("version",   to_str(static_cast<u16>(t.major_version)) + "." +
                    to_str(static_cast<u16>(t.minor_version)));
    kv("glyphs",    to_str(static_cast<u32>(t.glyphs.size())));
    kv("global_subrs", to_str(t.global_subr_index.count));
    kv("local_subrs",  to_str(t.local_subr_index.count));

    auto dict_str = [&](int key) -> std::string {
        auto it = t.top_dict.entries.find(key);
        if (it == t.top_dict.entries.end()) return "";
        if (auto* iv = std::get_if<int32_t>(&it->second)) return to_str(*iv);
        if (auto* dv = std::get_if<double>(&it->second)) return to_str(*dv);
        return "(array)";
    };

    comment("Top DICT — font-level metadata");
    open("top_dict");
    auto emit_dict_sid = [&](const char* name, int key) {
        auto it = t.top_dict.entries.find(key);
        if (it == t.top_dict.entries.end()) return;
        if (auto* iv = std::get_if<int32_t>(&it->second))
            kv(name, "\"" + t.sid_to_string(static_cast<u32>(*iv)) + "\"");
    };
    emit_dict_sid("version",     0);
    emit_dict_sid("notice",      1);
    emit_dict_sid("copyright",   0x0C00);
    emit_dict_sid("full_name",   2);
    emit_dict_sid("family_name", 3);
    emit_dict_sid("weight",      4);
    {
        auto it = t.top_dict.entries.find(5);
        if (it != t.top_dict.entries.end()) {
            if (auto* arr = std::get_if<std::vector<double>>(&it->second)) {
                std::string bb;
                for (size_t i = 0; i < arr->size(); i++) {
                    if (i) bb += " ";
                    bb += to_str((*arr)[i]);
                }
                kv("font_bbox", bb, "x_min y_min x_max y_max");
            }
        }
    }
    close();

    if (opts_.emit_glyphs) {
        for (auto& g : t.glyphs) emit_cff_glyph(g);
    }
    close();
}

void FydeEmitter::emit_font(const Font& font) {
    emit_file_header(font);
    blank();

    if (font.head) emit_head(*font.head);
    if (font.name) emit_name(*font.name);
    if (font.cmap && opts_.emit_cmap) emit_cmap(*font.cmap);
    if (font.maxp) emit_maxp(*font.maxp);
    if (font.hhea) emit_hhea(*font.hhea);
    if (font.hmtx && font.hhea) emit_hmtx(*font.hmtx, font.hhea->number_of_h_metrics);
    if (font.post) emit_post(*font.post);
    if (font.os2)  emit_os2(*font.os2);
    if (font.loca) emit_loca(*font.loca);
    if (font.kern && opts_.emit_kerning) emit_kern(*font.kern);
    if (font.glyf && opts_.emit_glyphs) emit_glyphs(*font.glyf);
    if (font.cff)  emit_cff(*font.cff);

    blank();
    close();
}

void FydeEmitter::emit_bdf(const BdfFont& bdf, const std::string& path) {
    out_ << "# ============================================================\n";
    out_ << "# FontyDE Decompiled BDF Font Source\n";
    out_ << "# Font    : " << bdf.properties.font_name << "\n";
    out_ << "# Source  : " << path << "\n";
    out_ << "# Glyphs  : " << bdf.glyphs.size() << "\n";
    out_ << "# ============================================================\n\n";

    open("bdf_font");
    open("properties");
    kv("font_name",       "\"" + bdf.properties.font_name + "\"");
    kv("point_size",      to_str(bdf.properties.point_size));
    kv("resolution",      to_str(bdf.properties.resolution_x) + "x" +
                          to_str(bdf.properties.resolution_y), "dpi");
    kv("bounding_box",    to_str(bdf.properties.bounding_box_w) + " " +
                          to_str(bdf.properties.bounding_box_h) + " " +
                          to_str(bdf.properties.bounding_box_x) + " " +
                          to_str(bdf.properties.bounding_box_y), "w h x y");
    kv("ascent",          to_str(bdf.properties.ascent));
    kv("descent",         to_str(bdf.properties.descent));
    close();

    for (auto& g : bdf.glyphs) {
        open("glyph");
        kv("name",     "\"" + g.name + "\"");
        kv("encoding", to_str(g.encoding));
        kv("dwidth",   to_str(g.d_width_x) + " " + to_str(g.d_width_y));
        kv("bbx",      to_str(g.bb_w) + " " + to_str(g.bb_h) + " " +
                       to_str(g.bb_x) + " " + to_str(g.bb_y), "w h x y");
        if (opts_.emit_bitmap_rows) {
            comment("bitmap rows — each row is one hex word, MSB first, 1=ink 0=background");
            for (size_t row = 0; row < g.bitmap_rows.size(); row++) {
                u32 bits = g.bitmap_rows[row];
                std::string pixels;
                for (int b = g.bb_w - 1; b >= 0; b--)
                    pixels += ((bits >> b) & 1) ? "#" : ".";
                out_ << indent_str()
                     << std::hex << std::uppercase
                     << std::setw(2) << std::setfill('0') << bits
                     << std::dec
                     << "  # " << pixels << "\n";
            }
        }
        close();
    }
    close();
}

void emit_to_file(const Font& font, const std::string& out_path, EmitOptions opts) {
    opts.source_path = font.source_path;
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter emitter(f, opts);
    emitter.emit_font(font);
}

void emit_bdf_to_file(const BdfFont& bdf, const std::string& src, const std::string& out_path) {
    std::ofstream f(out_path);
    if (!f) throw FontyError("cannot open output file: " + out_path);
    FydeEmitter emitter(f);
    emitter.emit_bdf(bdf, src);
}

} // namespace fontyde
