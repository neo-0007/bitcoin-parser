#include "script.h"
#include "utilities.h"

#include <stdexcept>

// opcode name map
static const std::map<uint8_t, std::string> opcode_map =
    {
        // push value
        {OP_0, "OP_0"},
        {OP_PUSHDATA1, "OP_PUSHDATA1"},
        {OP_PUSHDATA2, "OP_PUSHDATA2"},
        {OP_PUSHDATA4, "OP_PUSHDATA4"},
        {OP_1NEGATE, "OP_1NEGATE"},
        {OP_RESERVED, "OP_RESERVED"},
        {OP_1, "OP_1"},
        {OP_2, "OP_2"},
        {OP_3, "OP_3"},
        {OP_4, "OP_4"},
        {OP_5, "OP_5"},
        {OP_6, "OP_6"},
        {OP_7, "OP_7"},
        {OP_8, "OP_8"},
        {OP_9, "OP_9"},
        {OP_10, "OP_10"},
        {OP_11, "OP_11"},
        {OP_12, "OP_12"},
        {OP_13, "OP_13"},
        {OP_14, "OP_14"},
        {OP_15, "OP_15"},
        {OP_16, "OP_16"},

        // control
        {OP_NOP, "OP_NOP"},
        {OP_VER, "OP_VER"},
        {OP_IF, "OP_IF"},
        {OP_NOTIF, "OP_NOTIF"},
        {OP_VERIF, "OP_VERIF"},
        {OP_VERNOTIF, "OP_VERNOTIF"},
        {OP_ELSE, "OP_ELSE"},
        {OP_ENDIF, "OP_ENDIF"},
        {OP_VERIFY, "OP_VERIFY"},
        {OP_RETURN, "OP_RETURN"},

        // stack ops
        {OP_TOALTSTACK, "OP_TOALTSTACK"},
        {OP_FROMALTSTACK, "OP_FROMALTSTACK"},
        {OP_2DROP, "OP_2DROP"},
        {OP_2DUP, "OP_2DUP"},
        {OP_3DUP, "OP_3DUP"},
        {OP_2OVER, "OP_2OVER"},
        {OP_2ROT, "OP_2ROT"},
        {OP_2SWAP, "OP_2SWAP"},
        {OP_IFDUP, "OP_IFDUP"},
        {OP_DEPTH, "OP_DEPTH"},
        {OP_DROP, "OP_DROP"},
        {OP_DUP, "OP_DUP"},
        {OP_NIP, "OP_NIP"},
        {OP_OVER, "OP_OVER"},
        {OP_PICK, "OP_PICK"},
        {OP_ROLL, "OP_ROLL"},
        {OP_ROT, "OP_ROT"},
        {OP_SWAP, "OP_SWAP"},
        {OP_TUCK, "OP_TUCK"},

        // splice ops
        {OP_CAT, "OP_CAT"},
        {OP_SUBSTR, "OP_SUBSTR"},
        {OP_LEFT, "OP_LEFT"},
        {OP_RIGHT, "OP_RIGHT"},
        {OP_SIZE, "OP_SIZE"},

        // bit logic
        {OP_INVERT, "OP_INVERT"},
        {OP_AND, "OP_AND"},
        {OP_OR, "OP_OR"},
        {OP_XOR, "OP_XOR"},
        {OP_EQUAL, "OP_EQUAL"},
        {OP_EQUALVERIFY, "OP_EQUALVERIFY"},
        {OP_RESERVED1, "OP_RESERVED1"},
        {OP_RESERVED2, "OP_RESERVED2"},

        // numeric
        {OP_1ADD, "OP_1ADD"},
        {OP_1SUB, "OP_1SUB"},
        {OP_2MUL, "OP_2MUL"},
        {OP_2DIV, "OP_2DIV"},
        {OP_NEGATE, "OP_NEGATE"},
        {OP_ABS, "OP_ABS"},
        {OP_NOT, "OP_NOT"},
        {OP_0NOTEQUAL, "OP_0NOTEQUAL"},
        {OP_ADD, "OP_ADD"},
        {OP_SUB, "OP_SUB"},
        {OP_MUL, "OP_MUL"},
        {OP_DIV, "OP_DIV"},
        {OP_MOD, "OP_MOD"},
        {OP_LSHIFT, "OP_LSHIFT"},
        {OP_RSHIFT, "OP_RSHIFT"},
        {OP_BOOLAND, "OP_BOOLAND"},
        {OP_BOOLOR, "OP_BOOLOR"},
        {OP_NUMEQUAL, "OP_NUMEQUAL"},
        {OP_NUMEQUALVERIFY, "OP_NUMEQUALVERIFY"},
        {OP_NUMNOTEQUAL, "OP_NUMNOTEQUAL"},
        {OP_LESSTHAN, "OP_LESSTHAN"},
        {OP_GREATERTHAN, "OP_GREATERTHAN"},
        {OP_LESSTHANOREQUAL, "OP_LESSTHANOREQUAL"},
        {OP_GREATERTHANOREQUAL, "OP_GREATERTHANOREQUAL"},
        {OP_MIN, "OP_MIN"},
        {OP_MAX, "OP_MAX"},
        {OP_WITHIN, "OP_WITHIN"},

        // crypto
        {OP_RIPEMD160, "OP_RIPEMD160"},
        {OP_SHA1, "OP_SHA1"},
        {OP_SHA256, "OP_SHA256"},
        {OP_HASH160, "OP_HASH160"},
        {OP_HASH256, "OP_HASH256"},
        {OP_CODESEPARATOR, "OP_CODESEPARATOR"},
        {OP_CHECKSIG, "OP_CHECKSIG"},
        {OP_CHECKSIGVERIFY, "OP_CHECKSIGVERIFY"},
        {OP_CHECKMULTISIG, "OP_CHECKMULTISIG"},
        {OP_CHECKMULTISIGVERIFY, "OP_CHECKMULTISIGVERIFY"},

        // expansion
        {OP_NOP1, "OP_NOP1"},
        {OP_CHECKLOCKTIMEVERIFY, "OP_CHECKLOCKTIMEVERIFY"},
        {OP_CHECKSEQUENCEVERIFY, "OP_CHECKSEQUENCEVERIFY"},
        {OP_NOP4, "OP_NOP4"},
        {OP_NOP5, "OP_NOP5"},
        {OP_NOP6, "OP_NOP6"},
        {OP_NOP7, "OP_NOP7"},
        {OP_NOP8, "OP_NOP8"},
        {OP_NOP9, "OP_NOP9"},
        {OP_NOP10, "OP_NOP10"},

        // tapscript
        {OP_CHECKSIGADD, "OP_CHECKSIGADD"},

        {OP_INVALIDOPCODE, "OP_INVALIDOPCODE"}};


// Disassemble byte script to ASM string
std::string disassemble_script(const std::vector<uint8_t> &script)
{
    std::string result;
    size_t i = 0;

    while (i < script.size())
    {
        uint8_t opcode = script[i++];

        // Small push (1–75 bytes)
        if (opcode >= 0x01 && opcode <= 0x4b)
        {
            // Clamp to available bytes — non-standard/coinbase scripts can have
            // truncated pushes; emit what we have rather than throwing.
            size_t avail = script.size() - i;
            size_t take  = (opcode <= avail) ? opcode : avail;

            std::vector<uint8_t> data(
                script.begin() + i,
                script.begin() + i + take);

            result += "OP_PUSHBYTES_" + std::to_string(opcode);
            result += " ";
            result += bytes_to_hex(data);

            i += take;
        }

        // OP_PUSHDATA1
        else if (opcode == OP_PUSHDATA1)
        {
            if (i >= script.size())
            {
                result += "OP_PUSHDATA1";
                break;
            }

            uint8_t length = script[i++];
            size_t avail   = script.size() - i;
            size_t take    = (length <= avail) ? length : avail;

            std::vector<uint8_t> data(
                script.begin() + i,
                script.begin() + i + take);

            result += "OP_PUSHDATA1 ";
            result += bytes_to_hex(data);

            i += take;
        }
        // OP_PUSHDATA2
        else if (opcode == OP_PUSHDATA2)
        {
            if (i + 2 > script.size())
            {
                result += "OP_PUSHDATA2";
                break;
            }

            uint16_t length = read_uint16_le(script, i);
            i += 2;

            size_t avail = script.size() - i;
            size_t take  = (length <= avail) ? static_cast<size_t>(length) : avail;

            std::vector<uint8_t> data(
                script.begin() + i,
                script.begin() + i + take);

            result += "OP_PUSHDATA2 ";
            result += bytes_to_hex(data);

            i += take;
        }
        // OP_PUSHDATA4
        else if (opcode == OP_PUSHDATA4)
        {
            if (i + 4 > script.size())
            {
                result += "OP_PUSHDATA4";
                break;
            }

            uint32_t length = read_uint32_le(script, i);
            i += 4;

            size_t avail = script.size() - i;
            size_t take  = (static_cast<uint64_t>(length) <= avail)
                               ? static_cast<size_t>(length)
                               : avail;

            std::vector<uint8_t> data(
                script.begin() + i,
                script.begin() + i + take);

            result += "OP_PUSHDATA4 ";
            result += bytes_to_hex(data);

            i += take;
        }

        else
        {
            // Lookup opcode name
            auto it = opcode_map.find(opcode);

            if (it != opcode_map.end())
            {
                result += it->second;
            }
            else
            {
                result += "OP_UNKNOWN_0x";
                result += byte_to_hex(opcode);
            }
        }

        if (i < script.size())
            result += " ";
    }

    return result;
}

// Hex wrapper
std::string disassemble_script_hex(const std::string &hex_script)
{
    std::vector<uint8_t> script = hex_to_bytes(hex_script);
    return disassemble_script(script);
}

// Script classification
OutputScriptType classify_output_script(const std::vector<uint8_t> &script)
{
    // P2PKH: OP_DUP OP_HASH160 OP_PUSHBYTES_20 <20B> OP_EQUALVERIFY OP_CHECKSIG
    if (script.size() == 25 &&
        script[0] == OP_DUP &&
        script[1] == OP_HASH160 &&
        script[2] == 0x14 &&
        script[23] == OP_EQUALVERIFY &&
        script[24] == OP_CHECKSIG)
    {
        return OutputScriptType::P2PKH;
    }
    // P2SH: OP_HASH160 OP_PUSHBYTES_20 <20B> OP_EQUAL
    if (script.size() == 23 &&
        script[0] == OP_HASH160 &&
        script[1] == 0x14 &&
        script[22] == OP_EQUAL)
    {
        return OutputScriptType::P2SH;
    }
    // P2WPKH: OP_0 OP_PUSHBYTES_20 <20B>
    if (script.size() == 22 &&
        script[0] == OP_0 &&
        script[1] == 0x14)
    {
        return OutputScriptType::P2WPKH;
    }
    // P2WSH: OP_0 OP_PUSHBYTES_32 <32B>
    if (script.size() == 34 &&
        script[0] == OP_0 &&
        script[1] == 0x20)
    {
        return OutputScriptType::P2WSH;
    }
    // P2TR: OP_1 OP_PUSHBYTES_32 <32B>
    if (script.size() == 34 &&
        script[0] == OP_1 &&
        script[1] == 0x20)
    {
        return OutputScriptType::P2TR;
    }
    // OP_RETURN
    if (!script.empty() && script[0] == OP_RETURN)
    {
        return OutputScriptType::OP_RETURN;
    }
    return OutputScriptType::UNKNOWN;
}