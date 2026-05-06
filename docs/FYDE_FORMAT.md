# The .fyde Format Specification

FontyDE emits `.fyde` files — **FontyDE Annotated Source**. This document is the format spec.

---

## Overview

`.fyde` is a plain-text, line-oriented format. It is:

- **Human-readable** — every field has a comment explaining it
- **Diff-friendly** — one field per line, deterministic ordering
- **Nestable** — blocks can be nested to arbitrary depth
- **Comment-rich** — `#` begins a comment to end of line
- **Re-parseable** — trivial to write a reader

---

## Syntax

### Comments

```
# This is a comment
key = value  # This is an inline comment
```

### Key-Value Pairs

```
key = value
key = "string with spaces"
key = 0x1F4A9
key = 1.234567
key = true
key = false
```

### Blocks

```
block_name {
  key = value
  nested_block {
    key = value
  }
}
```

Blocks can be named with arbitrary identifiers. Glyph blocks use `glyph_N` where N is the glyph ID.

### Glyph Contours

Inside a `contour_N` block, each line is a point:

```
contour_0 {
  # point_index  x  y  type  [overlap]
  0   0     0    on
  1   500   700  off
  2   1000  0    on
}
```

Point types:
- `on` — on-curve point. Line endpoint, or anchor for a quadratic spline.
- `off` — off-curve control point for a quadratic (TrueType) Bézier.
- `cubic` — off-curve control point for a cubic (OpenType 1.9+) Bézier.
- `overlap` — the optional `overlap` token means the `OVERLAP_SIMPLE` flag was set.

### CFF Charstrings

Inside a CFF glyph block, each line is a mnemonic with optional `# args:` annotation:

```
glyph_36 {
  name = "A"
  rmoveto  # args: 8 0
  rlineto  # args: 588 1456
  rlineto  # args: 174 0
  rlineto  # args: 762 -1456
  ...
  endchar
}
```

### Bitmap Rows (BDF/FNT)

Inside a `glyph` block for bitmap fonts, each bitmap row is:

```
3C  # ..####..
42  # .#....#.
42  # .#....#.
7E  # .######.
42  # .#....#.
42  # .#....#.
```

The hex value is the raw bitmap word (MSB first). The `#` visual is the rendered row where `#` = ink pixel and `.` = background pixel.

---

## Top-Level Structure

```
# ============================================================
# FontyDE Decompiled Font Source
# Format  : TrueType (TTF)
# Family  : Inter
# Glyphs  : 2548
# Source  : /path/to/Inter.ttf
# Tables  : head, name, cmap, maxp, OS/2, ...
# ============================================================

font {
  format = "TrueType (TTF)"
  source = "/path/to/Inter.ttf"

  # ── head — Font Header Table ──
  head { ... }

  # ── name — Naming Table ──
  name { ... }

  # ── cmap — Character to Glyph Index Mapping ──
  cmap { ... }

  # ── maxp — Maximum Profile ──
  maxp { ... }

  # ── hhea — Horizontal Header ──
  hhea { ... }

  # ── hmtx — Horizontal Metrics ──
  hmtx { ... }

  # ── post — PostScript Name Mapping ──
  post { ... }

  # ── OS/2 — Windows Metrics & Classification ──
  OS2 { ... }

  # ── loca — Glyph Location Index ──
  loca { ... }

  # ── kern — Kerning Table ──
  kern { ... }

  # ── glyf — TrueType Glyph Outlines ──
  glyf {
    glyph_0 { ... }
    glyph_1 { ... }
    ...
  }
}
```

---

## cmap Block

The `cmap` block contains one `subtable` per encoding in the font. Each subtable
lists all codepoint → glyph ID mappings:

```
cmap {
  version = 0
  subtable {
    platform = 3  # Windows
    encoding = 1  # UCS-2
    format   = 4  # Segment mapping
    mappings = 652
    U+0020 -> 3   # SPACE
    U+0041 -> 36  # LATIN A
    U+0042 -> 37  # LATIN B
    ...
  }
}
```

---

## Parsing Notes

- Indentation is 2 spaces per level (not significant, for readability only).
- String values are always double-quoted.
- Numeric values are unquoted. Hex values use `0x` prefix.
- The `# ── Section ──` lines are section separators, not structural.
- A `block_name {` must have the `{` on the same line.
- A `}` closes the innermost open block and must be on its own line.
- In contour blocks, columns are space-separated with variable width — parse by splitting on whitespace.

---

## Versioning

The `.fyde` format is versioned implicitly by the FontyDE version that wrote it.
Future versions may add new block types and fields; parsers should silently ignore unknown keys.
