//
// FontyDE — Decompiler Edition
// src/tables/post.cpp
//

#include "../fontyde/tables/post.h"
#include <cstring>

namespace fontyde {

static const char* MAC_GLYPH_NAMES[] = {
    ".notdef","null","nonmarkingreturn","space","exclam","quotedbl","numbersign",
    "dollar","percent","ampersand","quotesingle","parenleft","parenright","asterisk",
    "plus","comma","hyphen","period","slash","zero","one","two","three","four","five",
    "six","seven","eight","nine","colon","semicolon","less","equal","greater","question",
    "at","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S",
    "T","U","V","W","X","Y","Z","bracketleft","backslash","bracketright","asciicircum",
    "underscore","grave","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
    "p","q","r","s","t","u","v","w","x","y","z","braceleft","bar","braceright",
    "asciitilde","Adieresis","Aring","Ccedilla","Eacute","Ntilde","Odieresis",
    "Udieresis","aacute","agrave","acircumflex","adieresis","atilde","aring",
    "ccedilla","eacute","egrave","ecircumflex","edieresis","iacute","igrave",
    "icircumflex","idieresis","ntilde","oacute","ograve","ocircumflex","odieresis",
    "otilde","uacute","ugrave","ucircumflex","udieresis","dagger","degree","cent",
    "sterling","section","bullet","paragraph","germandbls","registered","copyright",
    "trademark","acute","dieresis","notequal","AE","Oslash","infinity","plusminus",
    "lessequal","greaterequal","yen","mu","partialdiff","summation","product","pi",
    "integral","ordfeminine","ordmasculine","Omega","ae","oslash","questiondown",
    "exclamdown","logicalnot","radical","florin","approxequal","Delta","guillemotleft",
    "guillemotright","ellipsis","nonbreakingspace","Agrave","Atilde","Otilde","OE",
    "oe","endash","emdash","quotedblleft","quotedblright","quoteleft","quoteright",
    "divide","lozenge","ydieresis","Ydieresis","fraction","currency","guilsinglleft",
    "guilsinglright","fi","fl","daggerdbl","periodcentered","quotesinglbase",
    "quotedblbase","perthousand","Acircumflex","Ecircumflex","Aacute","Edieresis",
    "Egrave","Iacute","Icircumflex","Idieresis","Igrave","Oacute","Ocircumflex",
    "apple","Ograve","Uacute","Ucircumflex","Ugrave","dotlessi","circumflex","tilde",
    "macron","breve","dotaccent","ring","cedilla","hungarumlaut","ogonek","caron",
    "Lslash","lslash","Scaron","scaron","Zcaron","zcaron","brokenbar","Eth","eth",
    "Yacute","yacute","Thorn","thorn","minus","multiply","onesuperior","twosuperior",
    "threesuperior","onehalf","onequarter","threequarters","franc","Gbreve","gbreve",
    "Idotaccent","Scedilla","scedilla","Cacute","cacute","Ccaron","ccaron","dcroat"
};

static constexpr size_t MAC_GLYPH_NAME_COUNT =
    sizeof(MAC_GLYPH_NAMES) / sizeof(MAC_GLYPH_NAMES[0]);

const char* PostTable::mac_glyph_name(u16 index) {
    if (index < MAC_GLYPH_NAME_COUNT) return MAC_GLYPH_NAMES[index];
    return ".undefined";
}

std::string PostTable::glyph_name(u16 gid) const {
    if (version == 0x00010000) {
        if (gid < MAC_GLYPH_NAME_COUNT) return MAC_GLYPH_NAMES[gid];
        return ".undefined";
    }
    if (version == 0x00020000) {
        if (gid >= glyph_name_index.size()) return ".undefined";
        u16 idx = glyph_name_index[gid];
        if (idx < 258) return MAC_GLYPH_NAMES[idx < MAC_GLYPH_NAME_COUNT ? idx : 0];
        u16 custom_idx = idx - 258;
        if (custom_idx < string_data.size()) return string_data[custom_idx];
    }
    return ".undefined";
}

PostTable PostTable::parse(BinaryReader& r) {
    PostTable t = {};
    t.version             = r.read_fixed();
    t.italic_angle        = r.read_fixed();
    t.underline_position  = r.read_i16();
    t.underline_thickness = r.read_i16();
    t.is_fixed_pitch      = r.read_u32();
    t.min_mem_type42      = r.read_u32();
    t.max_mem_type42      = r.read_u32();
    t.min_mem_type1       = r.read_u32();
    t.max_mem_type1       = r.read_u32();

    if (t.version == 0x00020000) {
        t.num_glyphs = r.read_u16();
        t.glyph_name_index.resize(t.num_glyphs);
        for (auto& ni : t.glyph_name_index) ni = r.read_u16();
        while (!r.eof()) {
            u8 len = r.read_u8();
            if (len == 0) break;
            t.string_data.push_back(r.read_string(len));
        }
    }
    return t;
}

} // namespace fontyde
