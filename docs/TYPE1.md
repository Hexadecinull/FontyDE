# Type 1 Font Encryption Notes

Type 1 fonts use two layers of encryption, both based on a linear congruential generator (LCG).

---

## The Cipher

The cipher is defined as:

```
plain_byte  = cipher_byte XOR (r >> 8)
r_new       = (cipher_byte + r_old) * 52845 + 22719
```

This is a running-key cipher where the key state `r` updates on every byte using the *ciphertext* byte (not the plaintext). This means decryption is sequential — you cannot parallelize or seek.

---

## eexec Encryption

The eexec section (the private portion of a Type 1 font — charstrings, hints, subrs) is encrypted with initial key `r = 55665`. The first **4 bytes** of the decrypted output are random seed bytes and must be discarded before parsing PostScript.

In PFB files, the eexec segment is stored as raw binary (segment type 2). In PFA files, it is ASCII hex-encoded and must be hex-decoded first.

---

## Charstring Encryption

Individual charstrings (glyph drawing instructions) within the eexec-decrypted data are encrypted again with initial key `r = 4330`. The first **4 bytes** of each charstring are again seed bytes to discard.

---

## PFB Segment Format

A `.pfb` file consists of segments:

```
0x80 <type> <length:uint32_LE> <data...>
```

| Type | Meaning |
|------|---------|
| 0x01 | ASCII text (PostScript source) |
| 0x02 | Binary data (eexec section) |
| 0x03 | End of file |

A minimal PFB has: segment(type=1, ASCII header) → segment(type=2, eexec) → segment(type=3, EOF marker).

---

## What's in the ASCII Header

The ASCII portion is PostScript and contains:

```postscript
%!PS-AdobeFont-1.0: FontName 001.007
%%Title: FontName
%Version: 001.007
%%CreationDate: ...
%%VMusage: 25000 55000
% Copyright ...
11 dict begin
/FontInfo 9 dict dup begin
  /version (001.007) readonly def
  /Notice (Copyright ...) readonly def
  /FullName (FontName) readonly def
  /FamilyName (FontFamily) readonly def
  /Weight (Regular) readonly def
  /ItalicAngle 0 def
  /isFixedPitch false def
  /UnderlinePosition -100 def
  /UnderlineThickness 50 def
end readonly def
/FontName /FontName def
/PaintType 0 def
/FontType 1 def
/FontMatrix [0.001 0 0 0.001 0 0] readonly def
/UniqueID 12345 def
/FontBBox {-166 -225 1000 931} readonly def
/Encoding 256 array
...
```

The bounding box, encoding array, and font metadata are all in this plain-text section. The charstrings, subrs, and hint data are in the encrypted eexec section.

---

## What's in the eexec Section

After decrypting and discarding 4 seed bytes:

```postscript
dup /Private 8 dict dup begin
  /RD {string currentfile exch readstring pop} executeonly def
  /ND {def} executeonly def
  /NP {put} executeonly def
  /BlueValues [-20 0 472 490 562 580 ...] def
  /OtherBlues [-200 -180] def
  /StdHW [40] def
  /StdVW [70] def
  /Subrs 5 array
  dup 0 <encrypted_subr_bytes> RD <n> NP
  ...
  /CharStrings 228 dict dup begin
    /.notdef <n> RD <encrypted_charstring> ND
    /A <n> RD <encrypted_charstring> ND
    ...
```

Each charstring is further encrypted (key 4330) and must be decrypted separately. After discarding 4 seed bytes, the result is Type 2 charstring bytecode identical to CFF charstrings.
