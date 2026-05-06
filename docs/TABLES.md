# OpenType Table Reference

A quick reference for the tables FontyDE parses. For full specs, see
[docs.microsoft.com/typography/opentype/spec](https://docs.microsoft.com/en-us/typography/opentype/spec/).

---

## SFNT Container

All TrueType, OpenType, WOFF, and WOFF2 fonts use the SFNT container. It is:

```
Offset  Type   Field
0       uint32 sfVersion       Magic: 0x00010000 (TTF), 0x4F54544F ("OTTO" = CFF)
4       uint16 numTables
6       uint16 searchRange     (largest power of 2 ≤ numTables) * 16
8       uint16 entrySelector   log2(largest power of 2 ≤ numTables)
10      uint16 rangeShift      numTables*16 - searchRange

Then numTables * TableRecord:
  uint32 tag          4 ASCII chars, e.g. "head", "glyf"
  uint32 checksum     sum of all uint32s in table (big-endian)
  uint32 offset       byte offset from beginning of file
  uint32 length       byte length of table
```

Tables must be 4-byte aligned. Padding bytes must be zero for checksum purposes.

---

## Required Tables (TrueType)

| Tag | Description |
|-----|-------------|
| `cmap` | Character to glyph index mapping |
| `head` | Font header (version, UPM, bounding box, flags) |
| `hhea` | Horizontal header (ascender, descender, metrics count) |
| `hmtx` | Horizontal metrics (advance width, LSB per glyph) |
| `maxp` | Maximum profile (glyph count, memory requirements) |
| `name` | Naming table (family name, copyright, etc.) |
| `OS/2` | Windows metrics (weight class, embedding flags) |
| `post` | PostScript glyph names, italic angle |
| `loca` | Glyph offset index (maps GID → offset in glyf) |
| `glyf` | Glyph data (outlines as contours of Bézier points) |

## Required Tables (CFF/OpenType)

Same as TrueType except `loca`/`glyf` are replaced by:

| Tag | Description |
|-----|-------------|
| `CFF ` | Compact Font Format outlines (note trailing space in tag) |

---

## head Table Detail

```
uint16  majorVersion        = 1
uint16  minorVersion        = 0
Fixed   fontRevision        designer-specified version (16.16)
uint32  checksumAdjustment  0xB1B0AFBA - sum of all table checksums
uint32  magicNumber         = 0x5F0F3CF5 (always)
uint16  flags               bit field:
  bit 0:  baseline at y=0
  bit 1:  left sidebearing at x=0
  bit 2:  instructions depend on point size
  bit 3:  internal use by Microsoft
  bit 4:  instructions alter advance width
  ...
uint16  unitsPerEm          typically 1000 (CFF) or 2048 (TTF)
LONGDATETIME created        seconds since 1904-01-01 00:00:00 UTC
LONGDATETIME modified
int16   xMin, yMin, xMax, yMax   global bounding box in font units
uint16  macStyle            bit 0=bold, 1=italic, 2=underline, 3=outline...
uint16  lowestRecPPEM       smallest readable pixel size
int16   fontDirectionHint   deprecated, set to 2
int16   indexToLocFormat    0=short (uint16*2), 1=long (uint32)
int16   glyphDataFormat     = 0
```

---

## cmap Formats

| Format | Typical use |
|--------|-------------|
| 0  | Single-byte mapping (256 entries), legacy Mac |
| 4  | Segmented BMP mapping (U+0000–U+FFFF), most fonts |
| 6  | Dense range mapping |
| 12 | Full Unicode mapping (U+0000–U+10FFFF) |
| 14 | Unicode Variation Sequences |

Format 4 uses segments `[start_code, end_code]` with either a delta or an offset into a GID array. It is the most complex format and the most common.

---

## glyf Table — Simple Glyph Flags

Each point flag byte encodes:

```
Bit  Name                Meaning
0    ON_CURVE_POINT      1=on-curve, 0=off-curve (quadratic control)
1    X_SHORT_VECTOR      x coord is uint8 (else int16)
2    Y_SHORT_VECTOR      y coord is uint8 (else int16)
3    REPEAT_FLAG         next byte = repeat count for this flag
4    X_IS_SAME_OR_POS    if X_SHORT: 1=positive, 0=negative
                         if not X_SHORT: 1=same as previous x, 0=read int16
5    Y_IS_SAME_OR_POS    same logic for y
6    OVERLAP_SIMPLE      rendering hint: this contour may overlap others
7    CUBIC_POINT         off-curve cubic control point (OpenType 1.9+)
```

Quadratic splines: two consecutive off-curve points imply a synthetic on-curve point at their midpoint (TrueType convention).

---

## Composite Glyph Flags

```
Bit   Name
0     ARG_1_AND_2_ARE_WORDS       1=int16 args, 0=int8 args
1     ARGS_ARE_XY_VALUES          1=translation, 0=anchor point indices
2     ROUND_XY_TO_GRID
3     WE_HAVE_A_SCALE             one F2Dot14 scale follows
5     MORE_COMPONENTS             another component follows
6     WE_HAVE_AN_X_AND_Y_SCALE   two F2Dot14 (xx, yy) follow
7     WE_HAVE_A_TWO_BY_TWO       four F2Dot14 (xx, yx, xy, yy) follow
8     WE_HAVE_INSTRUCTIONS        instructions follow last component
9     USE_MY_METRICS              inherit advance width from this component
10    OVERLAP_COMPOUND
11    SCALED_COMPONENT_OFFSET
12    UNSCALED_COMPONENT_OFFSET
```

---

## OS/2 Weight Classes

| Value | Name |
|-------|------|
| 100 | Thin |
| 200 | ExtraLight / UltraLight |
| 300 | Light |
| 400 | Normal / Regular |
| 500 | Medium |
| 600 | SemiBold / DemiBold |
| 700 | Bold |
| 800 | ExtraBold / UltraBold |
| 900 | Black / Heavy |

---

## OS/2 Embedding Flags (fsType)

| Bits | Meaning |
|------|---------|
| 0x0000 | Installable embedding — unrestricted |
| 0x0002 | Preview & Print embedding only |
| 0x0004 | Editable embedding |
| 0x0008 | No subsetting |
| 0x0100 | Bitmap embedding only |

---

## CFF Top DICT Keys

| Key | Operator | Meaning |
|-----|----------|---------|
| 0   | version  | SID — version string |
| 1   | Notice   | SID — copyright notice |
| 2   | FullName | SID — full font name |
| 3   | FamilyName | SID — family name |
| 4   | Weight   | SID — weight string |
| 5   | FontBBox | array — x_min y_min x_max y_max |
| 15  | charset  | offset to charset data |
| 17  | CharStrings | offset to charstrings INDEX |
| 18  | Private  | [size, offset] of Private DICT |

---

## CFF Type 2 Charstring Operators (common)

| Op | Mnemonic | Stack effect |
|----|----------|-------------|
| 1  | hstem    | dy+ → — |
| 3  | vstem    | dx+ → — |
| 4  | vmoveto  | dy → — |
| 5  | rlineto  | {dx dy}+ → — |
| 6  | hlineto  | dx {dy dx}* → — |
| 7  | vlineto  | dy {dx dy}* → — |
| 8  | rrcurveto | {dx1 dy1 dx2 dy2 dx3 dy3}+ → — |
| 14 | endchar  | — terminates charstring |
| 21 | rmoveto  | dx dy → — |
| 22 | hmoveto  | dx → — |
| 24 | rcurveline | curve+ line |
| 25 | rlinecurve | line+ curve |
| 26 | vvcurveto | |
| 27 | hhcurveto | |
| 30 | vhcurveto | |
| 31 | hvcurveto | |

Numbers in the range 32–246 encode integers directly: `value = byte - 139`.
Two-byte integers: `value = (b0-247)*256 + b1 + 108` (for b0 in 247–250).
