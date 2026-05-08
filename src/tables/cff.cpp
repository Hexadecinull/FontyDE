//
// FontyDE — Decompiler Edition
// src/tables/cff.cpp
//

#include "../fontyde/tables/cff.h"
#include <cstring>
#include <cmath>

namespace fontyde {

static const char* CFF_STANDARD_STRINGS[] = {
    ".notdef","space","exclam","quotedbl","numbersign","dollar","percent","ampersand",
    "quoteright","parenleft","parenright","asterisk","plus","comma","hyphen","period",
    "slash","zero","one","two","three","four","five","six","seven","eight","nine",
    "colon","semicolon","less","equal","greater","question","at","A","B","C","D","E",
    "F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y",
    "Z","bracketleft","backslash","bracketright","asciicircum","underscore","quoteleft",
    "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z","braceleft","bar","braceright","asciitilde","exclamdown",
    "cent","sterling","fraction","yen","florin","section","currency","quotesingle",
    "quotedblleft","guillemotleft","guilsinglleft","guilsinglright","fi","fl","endash",
    "dagger","daggerdbl","periodcentered","paragraph","bullet","quotesinglbase",
    "quotedblbase","quotedblright","guillemotright","ellipsis","perthousand","questiondown",
    "grave","acute","circumflex","tilde","macron","breve","dotaccent","dieresis","ring",
    "cedilla","hungarumlaut","ogonek","caron","emdash","AE","ordfeminine","Lslash",
    "Oslash","OE","ordmasculine","ae","dotlessi","lslash","oslash","oe","germandbls",
    "onesuperior","logicalnot","mu","trademark","Eth","onehalf","plusminus","Thorn",
    "onequarter","divide","brokenbar","degree","thorn","threequarters","twosuperior",
    "registered","minus","eth","multiply","threesuperior","copyright","Aacute",
    "Acircumflex","Adieresis","Agrave","Aring","Atilde","Ccedilla","Eacute","Ecircumflex",
    "Edieresis","Egrave","Iacute","Icircumflex","Idieresis","Igrave","Ntilde","Oacute",
    "Ocircumflex","Odieresis","Ograve","Otilde","Scaron","Uacute","Ucircumflex",
    "Udieresis","Ugrave","Yacute","Ydieresis","Zcaron","aacute","acircumflex",
    "adieresis","agrave","aring","atilde","ccedilla","eacute","ecircumflex","edieresis",
    "egrave","iacute","icircumflex","idieresis","igrave","ntilde","oacute","ocircumflex",
    "odieresis","ograve","otilde","scaron","uacute","ucircumflex","udieresis","ugrave",
    "yacute","ydieresis","zcaron"
};

static constexpr size_t CFF_STD_STR_COUNT =
    sizeof(CFF_STANDARD_STRINGS) / sizeof(CFF_STANDARD_STRINGS[0]);

static const char* cff_op_mnemonic(int op) {
    switch (op) {
        case 1:   return "hstem";
        case 3:   return "vstem";
        case 4:   return "vmoveto";
        case 5:   return "rlineto";
        case 6:   return "hlineto";
        case 7:   return "vlineto";
        case 8:   return "rrcurveto";
        case 10:  return "callsubr";
        case 11:  return "return";
        case 14:  return "endchar";
        case 15:  return "vsindex";
        case 16:  return "blend";
        case 18:  return "hstemhm";
        case 19:  return "hintmask";
        case 20:  return "cntrmask";
        case 21:  return "rmoveto";
        case 22:  return "hmoveto";
        case 23:  return "vstemhm";
        case 24:  return "rcurveline";
        case 25:  return "rlinecurve";
        case 26:  return "vvcurveto";
        case 27:  return "hhcurveto";
        case 28:  return "shortint";
        case 29:  return "callgsubr";
        case 30:  return "vhcurveto";
        case 31:  return "hvcurveto";
        case 0x0C00: return "dotsection";
        case 0x0C03: return "and";
        case 0x0C04: return "or";
        case 0x0C05: return "not";
        case 0x0C09: return "abs";
        case 0x0C0A: return "add";
        case 0x0C0B: return "sub";
        case 0x0C0C: return "div";
        case 0x0C0E: return "neg";
        case 0x0C0F: return "eq";
        case 0x0C12: return "drop";
        case 0x0C14: return "put";
        case 0x0C15: return "get";
        case 0x0C16: return "ifelse";
        case 0x0C17: return "random";
        case 0x0C18: return "mul";
        case 0x0C1A: return "sqrt";
        case 0x0C1B: return "dup";
        case 0x0C1C: return "exch";
        case 0x0C1D: return "index";
        case 0x0C1E: return "roll";
        case 0x0C22: return "hflex";
        case 0x0C23: return "flex";
        case 0x0C24: return "hflex1";
        case 0x0C25: return "flex1";
        default:     return "unknown";
    }
}

CffIndex CffIndex::parse(BinaryReader& r) {
    CffIndex idx;
    idx.count = r.read_u16();
    if (idx.count == 0) return idx;

    u8 off_size = r.read_u8();
    idx.offsets.resize(idx.count + 1);

    auto read_offset = [&](u8 os) -> u32 {
        switch (os) {
            case 1: return r.read_u8();
            case 2: return r.read_u16();
            case 3: { u32 v = r.read_u8(); v = (v << 8) | r.read_u8(); v = (v << 8) | r.read_u8(); return v; }
            case 4: return r.read_u32();
            default: throw ParseError("CFF INDEX: invalid offSize");
        }
    };

    for (auto& o : idx.offsets) o = read_offset(off_size);

    u32 data_base = idx.offsets[0];
    u32 total     = idx.offsets[idx.count] - data_base;
    auto raw_data = r.read_bytes(total);

    idx.data.resize(idx.count);
    for (u32 i = 0; i < idx.count; i++) {
        u32 start = idx.offsets[i]   - data_base;
        u32 end   = idx.offsets[i+1] - data_base;
        idx.data[i] = std::vector<u8>(raw_data.begin() + start, raw_data.begin() + end);
    }
    return idx;
}

CffDict CffDict::parse(const u8* data, size_t size) {
    CffDict dict;
    std::vector<double> stack;
    size_t i = 0;

    while (i < size) {
        u8 b0 = data[i++];

        if (b0 == 29) {
            int32_t val = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
            stack.push_back(static_cast<double>(val));
            i += 4;
        } else if (b0 == 28) {
            int16_t val = static_cast<int16_t>((data[i] << 8) | data[i+1]);
            stack.push_back(static_cast<double>(val));
            i += 2;
        } else if (b0 >= 32 && b0 <= 246) {
            stack.push_back(static_cast<double>(b0) - 139.0);
        } else if (b0 >= 247 && b0 <= 250) {
            u8 b1 = data[i++];
            stack.push_back(static_cast<double>((b0 - 247) * 256 + b1 + 108));
        } else if (b0 >= 251 && b0 <= 254) {
            u8 b1 = data[i++];
            stack.push_back(static_cast<double>(-(b0 - 251) * 256 - b1 - 108));
        } else if (b0 == 30) {
            std::string num_str;
            bool done = false;
            while (!done && i < size) {
                u8 byte = data[i++];
                auto nibble = [&](u8 n) {
                    if (n == 0xE) num_str += '.';
                    else if (n == 0xF) done = true;
                    else if (n == 0xA) num_str += '*';
                    else if (n == 0xB) num_str += 'E';
                    else if (n == 0xC) { num_str += 'E'; num_str += '-'; }
                    else if (n == 0xD) {}
                    else num_str += static_cast<char>('0' + n);
                };
                nibble(byte >> 4);
                nibble(byte & 0x0F);
            }
            try { stack.push_back(std::stod(num_str)); } catch (...) { stack.push_back(0.0); }
        } else if (b0 == 12) {
            u8 b1 = data[i++];
            int op = 0x0C00 | b1;
            if (stack.size() == 1)
                dict.entries[op] = static_cast<int32_t>(stack[0]);
            else if (!stack.empty()) {
                std::vector<double> arr(stack.begin(), stack.end());
                dict.entries[op] = arr;
            }
            stack.clear();
        } else {
            int op = b0;
            if (stack.size() == 1)
                dict.entries[op] = static_cast<int32_t>(stack[0]);
            else if (!stack.empty()) {
                std::vector<double> arr(stack.begin(), stack.end());
                dict.entries[op] = arr;
            }
            stack.clear();
        }
    }
    return dict;
}

static std::vector<CffCharstringOp> parse_charstring(
    const u8* data, size_t size,
    const CffIndex& local_subrs,
    const CffIndex& global_subrs,
    int depth = 0)
{
    std::vector<CffCharstringOp> ops;
    std::vector<double> stack;
    size_t i = 0;

    while (i < size) {
        u8 b0 = data[i++];

        if (b0 >= 32 && b0 <= 246) {
            stack.push_back(static_cast<double>(b0) - 139.0);
        } else if (b0 >= 247 && b0 <= 250) {
            u8 b1 = data[i++];
            stack.push_back(static_cast<double>((b0 - 247) * 256 + b1 + 108));
        } else if (b0 >= 251 && b0 <= 254) {
            u8 b1 = data[i++];
            stack.push_back(static_cast<double>(-(b0 - 251) * 256 - b1 - 108));
        } else if (b0 == 28) {
            int16_t val = static_cast<int16_t>((data[i] << 8) | data[i+1]);
            stack.push_back(static_cast<double>(val));
            i += 2;
        } else if (b0 == 29) {
            int32_t val = (data[i] << 24)|(data[i+1]<<16)|(data[i+2]<<8)|data[i+3];
            stack.push_back(static_cast<double>(val));
            i += 4;
        } else {
            int op = b0;
            if (b0 == 12 && i < size) { op = 0x0C00 | data[i++]; }

            CffCharstringOp cop;
            cop.opcode   = op;
            cop.operands = stack;
            cop.mnemonic = cff_op_mnemonic(op);

            if ((op == 10 || op == 29) && !stack.empty() && depth < 8) {
                int subr_bias = 0;
                const CffIndex* subr_idx = (op == 10) ? &local_subrs : &global_subrs;
                u32 n = subr_idx->count;
                if (n < 1240) subr_bias = 107;
                else if (n < 33900) subr_bias = 1131;
                else subr_bias = 32768;
                int subr_num = static_cast<int>(stack.back()) + subr_bias;
                stack.pop_back();
                if (subr_num >= 0 && static_cast<u32>(subr_num) < n) {
                    auto& sd = subr_idx->data[subr_num];
                    auto sub_ops = parse_charstring(sd.data(), sd.size(),
                        local_subrs, global_subrs, depth + 1);
                    for (auto& so : sub_ops) ops.push_back(std::move(so));
                }
            } else {
                ops.push_back(std::move(cop));
                stack.clear();
                if (op == 14) break;
            }
        }
    }
    return ops;
}

std::string CffTable::sid_to_string(u32 sid) const {
    if (sid < CFF_STD_STR_COUNT) return CFF_STANDARD_STRINGS[sid];
    u32 custom = sid - static_cast<u32>(CFF_STD_STR_COUNT);
    if (custom < string_pool.size()) return string_pool[custom];
    return "(sid:" + std::to_string(sid) + ")";
}

CffTable CffTable::parse(BinaryReader& r) {
    CffTable t;
    t.major_version = r.read_u8();
    t.minor_version = r.read_u8();
    t.header_size   = r.read_u8();
    t.off_size      = r.read_u8();

    if (t.header_size > 4) r.skip(t.header_size - 4);

    t.name_index       = CffIndex::parse(r);
    t.top_dict_index   = CffIndex::parse(r);
    t.string_index     = CffIndex::parse(r);
    t.global_subr_index = CffIndex::parse(r);

    for (auto& s : t.string_index.data)
        t.string_pool.push_back(std::string(reinterpret_cast<const char*>(s.data()), s.size()));

    if (!t.top_dict_index.data.empty()) {
        auto& td = t.top_dict_index.data[0];
        t.top_dict = CffDict::parse(td.data(), td.size());
    }

    auto charstrings_offset_it = t.top_dict.entries.find(17);
    if (charstrings_offset_it != t.top_dict.entries.end()) {
        u32 cs_off = 0;
        if (auto* iv = std::get_if<int32_t>(&charstrings_offset_it->second))
            cs_off = static_cast<u32>(*iv);

        r.seek(cs_off);
        t.charstrings_index = CffIndex::parse(r);
    }

    auto private_it = t.top_dict.entries.find(18);
    if (private_it != t.top_dict.entries.end()) {
        if (auto* arr = std::get_if<std::vector<double>>(&private_it->second)) {
            if (arr->size() >= 2) {
                u32 priv_size   = static_cast<u32>((*arr)[0]);
                u32 priv_offset = static_cast<u32>((*arr)[1]);
                BinaryReader pr = r.sub_reader(priv_offset, priv_size);
                CffDict priv_dict = CffDict::parse(pr.raw(), priv_size);
                auto lsubr_it = priv_dict.entries.find(19);
                if (lsubr_it != priv_dict.entries.end()) {
                    u32 lsubr_off = 0;
                    if (auto* iv = std::get_if<int32_t>(&lsubr_it->second))
                        lsubr_off = static_cast<u32>(*iv);
                    BinaryReader lr = r.sub_reader(priv_offset + lsubr_off,
                        r.size() - priv_offset - lsubr_off);
                    t.local_subr_index = CffIndex::parse(lr);
                }
            }
        }
    }

    auto charset_it = t.top_dict.entries.find(15);
    std::vector<std::string> glyph_names;
    u32 cs_count = t.charstrings_index.count;

    if (charset_it != t.top_dict.entries.end()) {
        u32 charset_off = 0;
        if (auto* iv = std::get_if<int32_t>(&charset_it->second))
            charset_off = static_cast<u32>(*iv);

        if (charset_off > 2) {
            BinaryReader cr = r.sub_reader(charset_off, r.size() - charset_off);
            glyph_names.push_back(".notdef");
            u8 format = cr.read_u8();
            if (format == 0) {
                for (u32 i = 1; i < cs_count; i++) {
                    u16 sid = cr.read_u16();
                    glyph_names.push_back(t.sid_to_string(sid));
                }
            } else if (format == 1) {
                while (glyph_names.size() < cs_count) {
                    u16 first = cr.read_u16();
                    u8 nleft  = cr.read_u8();
                    for (u16 s = first; s <= first + nleft && glyph_names.size() < cs_count; s++)
                        glyph_names.push_back(t.sid_to_string(s));
                }
            } else if (format == 2) {
                while (glyph_names.size() < cs_count) {
                    u16 first  = cr.read_u16();
                    u16 nleft  = cr.read_u16();
                    for (u16 s = first; s <= first + nleft && glyph_names.size() < cs_count; s++)
                        glyph_names.push_back(t.sid_to_string(s));
                }
            }
        }
    }

    t.glyphs.resize(cs_count);
    for (u32 gid = 0; gid < cs_count; gid++) {
        CffGlyph& g = t.glyphs[gid];
        g.gid  = static_cast<u16>(gid);
        g.name = (gid < glyph_names.size()) ? glyph_names[gid] : ".glyph" + std::to_string(gid);
        if (gid < t.charstrings_index.data.size()) {
            auto& cs = t.charstrings_index.data[gid];
            try {
                g.ops = parse_charstring(cs.data(), cs.size(),
                    t.local_subr_index, t.global_subr_index);
            } catch (...) {}
        }
    }
    return t;
}

} // namespace fontyde
