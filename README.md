# FontyDE — Decompiler Edition

**FontyDE** is a universal font decompiler written in C++17 that converts binary font files back into rich, annotated human-readable source — like [dnSpy](https://github.com/dnSpy/dnSpy) does for .NET assemblies, but for fonts.

It is the companion tool to **Fonty** (font converter). Where Fonty converts between formats, FontyDE tears formats apart so you can understand exactly what's inside them.

---

## Supported Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| TrueType | `.ttf` | Apple/Microsoft outline font, quadratic Bézier |
| OpenType/CFF | `.otf` | Adobe/ISO outline font, cubic Bézier charstrings |
| WOFF | `.woff` | Web Open Font Format (zlib-compressed SFNT) |
| WOFF2 | `.woff2` | Web Open Font Format 2 (Brotli-compressed SFNT) |
| TrueType Collection | `.ttc` | Multiple TTF fonts in one file |
| OpenType Collection | `.otc` | Multiple OTF fonts in one file |
| Type 1 Binary | `.pfb` | Legacy PostScript font, eexec-encrypted |
| Type 1 ASCII | `.pfa` | Same as PFB but ASCII hex-encoded |
| BDF | `.bdf` | X11 Bitmap Distribution Format |
| Windows FNT | `.fnt` | Legacy Windows bitmap font |
| Windows FON | `.fon` | NE-format DLL containing one or more FNT resources |

---

## What "decompile" means for fonts

A binary font file (`.ttf`, `.otf`, etc.) is not source code — it's a tightly packed binary blob of tables. Understanding what's in it requires parsing:

- The **SFNT container** (a 4-byte magic, a table directory, and named binary chunks)
- Each **table** (dozens of them, each with a well-defined binary layout from the OpenType spec)
- **Glyph outlines** (TrueType: quadratic Bézier contours with delta-encoded coordinates; CFF: Type 2 charstring bytecode with a stack machine)

FontyDE parses all of this from scratch — no FreeType, no HarfBuzz — and emits a `.fyde` file: a text format that labels every field, explains every flag, and prints every glyph contour point-by-point with curve type annotations.

**Example output for one glyph:**

```
glyph_36 {
  name         = "A"
  advance_width = 1366
  lsb           = 8
  codepoints    = U+0041  # LATIN A
  bounding_box  = 8 0 1358 1456  # x_min y_min x_max y_max in font units

  # simple glyph — 2 contour(s)
  # Contour format: point_index  x  y  on|off|cubic
  # on  = on-curve point (line endpoint or TrueType quadratic anchor)
  # off = off-curve quadratic Bezier control point

  contour_0 {
    0   8    0    on
    1   596  1456 on
    2   770  1456 on
    3   1358 0    on
    4   1135 0    on
    5   985  434  on
    6   381  434  on
    7   229  0    on
  }
  contour_1 {
    0   420  594  on
    1   682  1311 on
    2   948  594  on
  }
}
```

---

## Building

### Prerequisites

- CMake 3.16+
- A C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- zlib (for WOFF support)
- *(optional)* libbrotlidec (for WOFF2 support)

### Basic build (TTF/OTF/WOFF/Type1/BDF/FNT)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### With WOFF2 support

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFONTYDE_WOFF2=ON
cmake --build build -j$(nproc)
```

### Install

```bash
cmake --install build --prefix /usr/local
```

### Windows (MSVC)

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Windows (MINGW64 / MSYS2)

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-zlib
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

---

## Usage

```
fontyde [options] <font_file> [font_file ...]
```

### Options

| Flag | Description |
|------|-------------|
| `-o, --output <path>` | Output file or directory. Omit to print to stdout. |
| `--no-glyphs` | Skip glyph outline data (much faster for large fonts) |
| `--no-cmap` | Skip the codepoint mapping table |
| `--no-kern` | Skip kerning pair data |
| `--no-bitmaps` | Skip bitmap row data (BDF/FNT) |
| `--ttc-index <n>` | For TTC/OTC collections, decompile only font at index `n` |
| `-h, --help` | Print usage |

### Examples

```bash
# Decompile to stdout
fontyde Inter.ttf

# Decompile to a .fyde file in the same directory
fontyde -o . Inter.ttf
# => ./Inter.fyde

# Decompile multiple files to a directory
fontyde -o decompiled/ Inter.ttf RobotoMono.otf SourceSansPro.woff

# Skip glyph data (useful for inspecting metadata only)
fontyde --no-glyphs -o . NotoSans.ttf

# Decompile one font from a collection
fontyde --ttc-index 0 NotoSans.ttc

# Decompile all fonts from a collection
fontyde -o out/ NotoSans.ttc
```

---

## Output Format (.fyde)

The `.fyde` format is a custom text format. It uses a simple nested block structure (`key = value` inside `block_name { }`) with `#` comments.

### Design goals

- **Every field is explained.** Values include comments from the OpenType spec.
- **Every flag is decoded.** Bit fields are shown in hex with the set bits named.
- **Glyph outlines are readable.** Points are listed with their coordinates and curve type.
- **Diff-friendly.** One field per line, stable ordering, predictable output.
- **Re-parseable.** The format is simple enough to write a parser for.

### Table coverage

| Block | Source table | Content |
|-------|-------------|---------|
| `head` | `head` | Version, UPM, bounding box, flags, timestamps |
| `name` | `name` | All name strings (family, copyright, version, etc.) |
| `cmap` | `cmap` | All subtable formats with full U+XXXX → GID mappings |
| `maxp` | `maxp` | Memory profile, glyph count |
| `hhea` | `hhea` | Horizontal metrics (ascender, descender, line gap) |
| `hmtx` | `hmtx` | Per-glyph advance width and LSB |
| `post` | `post` | PostScript glyph names |
| `OS2`  | `OS/2` | Weight, width, embedding flags, PANOSE, vendor ID |
| `loca` | `loca` | Glyph offset index summary |
| `kern` | `kern` | All kerning pairs |
| `glyf` | `glyf` | Per-glyph contours with point coordinates and types |
| `CFF`  | `CFF ` | Type 2 charstrings with inlined subroutines |

---

## Architecture

```
FontyDE/
├── include/fontyde/
│   ├── types.h          — u8/u16/u32/Fixed/F2Dot14, error types, format enum
│   ├── reader.h         — Big/little-endian binary reader with bounds checking
│   ├── detector.h       — Magic byte / extension based format detection
│   ├── font.h           — Central Font model (all parsed tables in one struct)
│   ├── tables/
│   │   ├── head.h/cff.h/cmap.h/glyf.h/...   — Per-table data structs + parse()
│   ├── formats/
│   │   ├── ttf.h        — SFNT/TTF/OTF/TTC parser
│   │   ├── woff.h       — WOFF unwrapper (zlib)
│   │   ├── woff2.h      — WOFF2 unwrapper (brotli, optional)
│   │   ├── type1.h      — Type 1 PFB/PFA parser + eexec decryption
│   │   ├── bdf.h        — BDF bitmap font parser
│   │   └── fnt.h        — Windows FNT/FON parser
│   └── emitter/
│       └── fyde_emitter.h  — .fyde source emitter
├── src/
│   ├── main.cpp         — CLI entry point, arg parsing, dispatch
│   ├── detector.cpp
│   ├── tables/          — Table parser implementations
│   └── formats/         — Format parser implementations
│       emitter/         — Emitter implementation
├── docs/
│   ├── FYDE_FORMAT.md   — .fyde format specification
│   ├── TABLES.md        — OpenType table reference notes
│   └── TYPE1.md         — Type 1 encryption notes
└── CMakeLists.txt
```

### Key design decisions

**No external font library.** FontyDE parses every byte itself. This is intentional — the point is to be a reference implementation you can read to understand how fonts work, not to wrap FreeType.

**BinaryReader is bounds-checked.** Every read operation verifies it won't exceed the buffer. A malformed font will throw a `ParseError`, not crash.

**Emitter is table-driven.** Each table has its own `emit_*()` method in `FydeEmitter`. Adding a new table means: add a struct in `tables/`, implement `parse()`, add a field to `Font`, call `parse()` in `ttf.cpp`, and add `emit_*()` in the emitter.

**CFF charstrings are inlined.** Subroutine calls are resolved and inlined by the charstring parser so the emitted output shows the complete drawing sequence without cross-references.

**WOFF unwrapping produces a real SFNT.** `unwrap_woff()` and `unwrap_woff2()` produce a byte-accurate SFNT binary that is then passed to `parse_sfnt()` — no special-casing in the parser.

---

## Adding a New Table

1. Create `include/fontyde/tables/mytable.h` — define `MyTable` struct and declare `static MyTable parse(BinaryReader& r)`.
2. Create `src/tables/mytable.cpp` — implement `parse()`.
3. Add `#include "fontyde/tables/mytable.h"` and `std::optional<MyTable> my_table;` to `font.h`.
4. In `src/formats/ttf.cpp`, add `if (auto br = read_table("MYTG")) { font.my_table = MyTable::parse(*br); }`.
5. Add `void emit_mytable(const MyTable& t);` to `FydeEmitter` and implement it in `fyde_emitter.cpp`.
6. Call `if (font.my_table) emit_mytable(*font.my_table);` in `FydeEmitter::emit_font()`.

---

## Technical Notes

### TrueType Glyph Coordinates

TrueType glyph coordinates are stored as deltas, not absolute values. The `glyf.cpp` parser accumulates deltas into absolute coordinates before emitting. Flags control whether each delta is a signed 8-bit value, a signed 16-bit value, or zero (repeat previous).

### CFF Charstrings

CFF Type 2 charstrings are a stack-based bytecode. Numbers are pushed onto a stack; operators consume them. Values in the range 32–246 encode small integers directly in one byte. FontyDE decodes all number encodings and inlines subroutine calls so you see the flat drawing sequence.

### WOFF Checksum

After assembling the SFNT from WOFF table data, FontyDE recomputes the `head.checksumAdjustment` field so the produced SFNT is spec-compliant. The algorithm: checksum all bytes as big-endian u32 words, then store `0xB1B0AFBA - total_checksum` at offset 8 in the `head` table.

### Type 1 Encryption

Type 1 uses a linear congruential generator (LCG) cipher. The eexec section uses key `55665`; individual charstrings use key `4330`. Both share the same update rule: `r_new = (cipher_byte + r_old) * 52845 + 22719`. The first 4 bytes of decrypted output are random seed bytes and are discarded.

---

## License

MIT License. See [LICENSE](LICENSE).

---

## Related

- **Fonty** — font format converter (the original tool this DE edition accompanies)
- [OpenType Specification](https://docs.microsoft.com/en-us/typography/opentype/spec/) — the definitive reference
- [The Type 1 Font Format](https://adobe-type-tools.github.io/font-tech-notes/pdfs/T1_SPEC.pdf) — Adobe's spec
- [WOFF2 Specification](https://www.w3.org/TR/WOFF2/) — W3C
