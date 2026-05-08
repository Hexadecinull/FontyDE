//
// FontyDE — Decompiler Edition
// src/tables/glyf.cpp
//

#include "../fontyde/tables/glyf.h"
#include "../fontyde/tables/loca.h"

namespace fontyde {

static constexpr u8 FLAG_ON_CURVE        = 0x01;
static constexpr u8 FLAG_X_SHORT         = 0x02;
static constexpr u8 FLAG_Y_SHORT         = 0x04;
static constexpr u8 FLAG_REPEAT          = 0x08;
static constexpr u8 FLAG_X_SAME_OR_POS   = 0x10;
static constexpr u8 FLAG_Y_SAME_OR_POS   = 0x20;
static constexpr u8 FLAG_OVERLAP_SIMPLE  = 0x40;
static constexpr u8 FLAG_CUBIC           = 0x80;

static constexpr u16 COMP_ARG_1_2_WORDS     = 0x0001;
static constexpr u16 COMP_ARGS_XY_VALUES    = 0x0002;
static constexpr u16 COMP_ROUND_XY_TO_GRID  = 0x0004;
static constexpr u16 COMP_WE_HAVE_A_SCALE   = 0x0008;
static constexpr u16 COMP_MORE_COMPONENTS   = 0x0020;
static constexpr u16 COMP_WE_HAVE_AN_X_AND_Y_SCALE = 0x0040;
static constexpr u16 COMP_WE_HAVE_A_TWO_BY_TWO     = 0x0080;
static constexpr u16 COMP_USE_MY_METRICS            = 0x0200;
static constexpr u16 COMP_SCALED_COMPONENT_OFFSET   = 0x0800;
static constexpr u16 COMP_UNSCALED_COMPONENT_OFFSET = 0x1000;

static Glyph parse_simple_glyph(BinaryReader& r, u16 gid, i16 num_contours,
                                  i16 x_min, i16 y_min, i16 x_max, i16 y_max) {
    Glyph g;
    g.gid              = gid;
    g.number_of_contours = num_contours;
    g.x_min            = x_min;
    g.y_min            = y_min;
    g.x_max            = x_max;
    g.y_max            = y_max;

    std::vector<u16> end_pts(num_contours);
    for (auto& e : end_pts) e = r.read_u16();

    u16 instruction_length = r.read_u16();
    g.instructions = r.read_bytes(instruction_length);

    u16 total_points = end_pts.empty() ? 0 : end_pts.back() + 1;

    std::vector<u8> flags;
    flags.reserve(total_points);
    while (flags.size() < total_points) {
        u8 flag = r.read_u8();
        flags.push_back(flag);
        if (flag & FLAG_REPEAT) {
            u8 repeat_count = r.read_u8();
            for (u8 k = 0; k < repeat_count; k++) flags.push_back(flag);
        }
    }

    std::vector<i16> x_coords(total_points, 0);
    for (u16 i = 0; i < total_points; i++) {
        u8 f = flags[i];
        i16 delta = 0;
        if (f & FLAG_X_SHORT) {
            delta = static_cast<i16>(r.read_u8());
            if (!(f & FLAG_X_SAME_OR_POS)) delta = -delta;
        } else if (!(f & FLAG_X_SAME_OR_POS)) {
            delta = r.read_i16();
        }
        x_coords[i] = (i > 0 ? x_coords[i-1] : 0) + delta;
    }

    std::vector<i16> y_coords(total_points, 0);
    for (u16 i = 0; i < total_points; i++) {
        u8 f = flags[i];
        i16 delta = 0;
        if (f & FLAG_Y_SHORT) {
            delta = static_cast<i16>(r.read_u8());
            if (!(f & FLAG_Y_SAME_OR_POS)) delta = -delta;
        } else if (!(f & FLAG_Y_SAME_OR_POS)) {
            delta = r.read_i16();
        }
        y_coords[i] = (i > 0 ? y_coords[i-1] : 0) + delta;
    }

    u16 pt_idx = 0;
    for (i16 c = 0; c < num_contours; c++) {
        GlyphContour contour;
        u16 end = end_pts[c];
        while (pt_idx <= end) {
            GlyphPoint pt;
            pt.x = x_coords[pt_idx];
            pt.y = y_coords[pt_idx];
            u8 f = flags[pt_idx];
            if (f & FLAG_CUBIC)
                pt.type = PointType::Cubic;
            else if (f & FLAG_ON_CURVE)
                pt.type = PointType::OnCurve;
            else
                pt.type = PointType::OffCurve;
            pt.overlap_simple = (f & FLAG_OVERLAP_SIMPLE) != 0;
            contour.points.push_back(pt);
            pt_idx++;
        }
        g.contours.push_back(std::move(contour));
    }
    return g;
}

static Glyph parse_composite_glyph(BinaryReader& r, u16 gid,
                                    i16 x_min, i16 y_min, i16 x_max, i16 y_max) {
    Glyph g;
    g.gid              = gid;
    g.number_of_contours = -1;
    g.x_min            = x_min;
    g.y_min            = y_min;
    g.x_max            = x_max;
    g.y_max            = y_max;

    bool more = true;
    while (more) {
        ComponentGlyph comp = {};
        comp.flags       = r.read_u16();
        comp.glyph_index = r.read_u16();

        if (comp.flags & COMP_ARG_1_2_WORDS) {
            comp.argument1 = r.read_i16();
            comp.argument2 = r.read_i16();
        } else {
            comp.argument1 = r.read_i8();
            comp.argument2 = r.read_i8();
        }

        comp.args_are_xy_values        = (comp.flags & COMP_ARGS_XY_VALUES) != 0;
        comp.round_xy_to_grid          = (comp.flags & COMP_ROUND_XY_TO_GRID) != 0;
        comp.use_my_metrics            = (comp.flags & COMP_USE_MY_METRICS) != 0;
        comp.scaled_component_offset   = (comp.flags & COMP_SCALED_COMPONENT_OFFSET) != 0;
        comp.unscaled_component_offset = (comp.flags & COMP_UNSCALED_COMPONENT_OFFSET) != 0;

        comp.xx = 1.0f; comp.yy = 1.0f;
        comp.xy = 0.0f; comp.yx = 0.0f;

        if (comp.flags & COMP_WE_HAVE_A_SCALE) {
            comp.xx = comp.yy = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.has_scale = true;
        } else if (comp.flags & COMP_WE_HAVE_AN_X_AND_Y_SCALE) {
            comp.xx = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.yy = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.has_scale = true;
        } else if (comp.flags & COMP_WE_HAVE_A_TWO_BY_TWO) {
            comp.xx = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.yx = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.xy = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.yy = static_cast<float>(f2dot14_to_double(r.read_f2dot14()));
            comp.has_2x2 = true;
        }

        more = (comp.flags & COMP_MORE_COMPONENTS) != 0;
        g.components.push_back(comp);
    }

    u16 instruction_length = 0;
    if (!g.components.empty() && (g.components.back().flags & 0x0100)) {
        instruction_length = r.read_u16();
        g.instructions = r.read_bytes(instruction_length);
    }
    return g;
}

GlyfTable GlyfTable::parse(BinaryReader& glyf_reader, const LocaTable& loca, u16 num_glyphs) {
    GlyfTable t;
    t.glyphs.reserve(num_glyphs);

    for (u16 gid = 0; gid < num_glyphs; gid++) {
        u32 offset = loca.glyph_offset(gid);
        u32 length = loca.glyph_length(gid);

        if (length == 0) {
            Glyph g;
            g.gid              = gid;
            g.number_of_contours = 0;
            t.glyphs.push_back(std::move(g));
            continue;
        }

        BinaryReader gr = glyf_reader.sub_reader(offset, length);
        i16 num_contours = gr.read_i16();
        i16 x_min = gr.read_i16();
        i16 y_min = gr.read_i16();
        i16 x_max = gr.read_i16();
        i16 y_max = gr.read_i16();

        Glyph g;
        try {
            if (num_contours >= 0) {
                g = parse_simple_glyph(gr, gid, num_contours, x_min, y_min, x_max, y_max);
            } else {
                g = parse_composite_glyph(gr, gid, x_min, y_min, x_max, y_max);
            }
        } catch (...) {
            g.gid              = gid;
            g.number_of_contours = num_contours;
            g.x_min = x_min; g.y_min = y_min;
            g.x_max = x_max; g.y_max = y_max;
        }
        t.glyphs.push_back(std::move(g));
    }
    return t;
}

} // namespace fontyde
