#include "script_processor.h"

// Extract last pushed data element from script
// Used for redeemScript detection in P2SH inputs
static std::optional<std::vector<uint8_t>>
extract_last_push(const std::vector<uint8_t>& script)
{
    size_t i = 0;
    std::vector<uint8_t> last;

    while (i < script.size())
    {
        uint8_t opcode = script[i++];

        size_t length = 0;

        if (opcode >= 0x01 && opcode <= 0x4b)
        {
            length = opcode;
        }
        else if (opcode == OP_PUSHDATA1)
        {
            if (i >= script.size()) return std::nullopt;
            length = script[i++];
        }
        else if (opcode == OP_PUSHDATA2)
        {
            if (i + 2 > script.size()) return std::nullopt;
            length = read_uint16_le(script, i);
            i += 2;
        }
        else if (opcode == OP_PUSHDATA4)
        {
            if (i + 4 > script.size()) return std::nullopt;
            length = read_uint32_le(script, i);
            i += 4;
        }
        else
        {
            return std::nullopt;
        }

        if (i + length > script.size())
            return std::nullopt;

        last.assign(script.begin() + i,
                    script.begin() + i + length);

        i += length;
    }

    if (last.empty())
        return std::nullopt;

    return last;
}

// Parse OP_RETURN payload
static OPReturnPayload
parse_op_return(const std::vector<uint8_t>& script)
{
    OPReturnPayload payload;
    payload.protocol = OPReturnProtocol::UNKNOWN;

    if (script.empty() || script[0] != OP_RETURN)
        return payload;

    std::vector<uint8_t> data;
    size_t i = 1;

    while (i < script.size())
    {
        uint8_t opcode = script[i++];

        size_t length = 0;

        if (opcode >= 0x01 && opcode <= 0x4b)
        {
            length = opcode;
        }
        else if (opcode == OP_PUSHDATA1)
        {
            if (i >= script.size()) break;
            length = script[i++];
        }
        else if (opcode == OP_PUSHDATA2)
        {
            if (i + 2 > script.size()) break;
            length = read_uint16_le(script, i);
            i += 2;
        }
        else if (opcode == OP_PUSHDATA4)
        {
            if (i + 4 > script.size()) break;
            length = read_uint32_le(script, i);
            i += 4;
        }
        else
        {
            break; // non-push opcode
        }

        if (i + length > script.size())
            break;

        data.insert(data.end(),
                    script.begin() + i,
                    script.begin() + i + length);

        i += length;
    }

    payload.data = data;

    //Protocol detection

    if (data.size() >= 4 &&
        data[0] == 0x6f && // o
        data[1] == 0x6d && // m
        data[2] == 0x6e && // n
        data[3] == 0x69)   // i
    {
        payload.protocol = OPReturnProtocol::OMNI;
    }
    else if (data.size() >= 5 &&
             data[0] == 0x01 &&
             data[1] == 0x09 &&
             data[2] == 0xf9 &&
             data[3] == 0x11 &&
             data[4] == 0x02)
    {
        payload.protocol = OPReturnProtocol::OPENTIMESTAMPS;
    }

    //UTF8 detection
    if (!data.empty() && is_valid_utf8(data))
    {
        payload.utf8 = std::string(data.begin(), data.end());
    }

    return payload;
}


ProcessedScriptPubKey
process_output_script(const std::vector<uint8_t>& script)
{
    ProcessedScriptPubKey result;
    result.type = classify_output_script(script);

    switch (result.type)
    {
        case OutputScriptType::P2PKH:
        {
            std::vector<uint8_t> hash(script.begin() + 3,
                                      script.begin() + 23);

            result.address = encode_p2pkh_address(hash);
            break;
        }

        case OutputScriptType::P2SH:
        {
            std::vector<uint8_t> hash(script.begin() + 2,
                                      script.begin() + 22);

            result.address = encode_p2sh_address(hash);
            break;
        }

        case OutputScriptType::P2WPKH:
        {
            std::vector<uint8_t> program(script.begin() + 2,
                                         script.begin() + 22);

            result.address = encode_segwit_address(0, program);
            break;
        }

        case OutputScriptType::P2WSH:
        {
            std::vector<uint8_t> program(script.begin() + 2,
                                         script.begin() + 34);

            result.address = encode_segwit_address(0, program);
            break;
        }

        case OutputScriptType::P2TR:
        {
            std::vector<uint8_t> program(script.begin() + 2,
                                         script.begin() + 34);

            result.address = encode_segwit_address(1, program);
            break;
        }

        case OutputScriptType::OP_RETURN:
        {
            result.address = std::nullopt;
            result.op_return = parse_op_return(script);
            break;
        }

        default:
        {
            result.address = std::nullopt;
            break;
        }
    }

    return result;
}

InputScriptType
classify_input(const std::vector<uint8_t>& prevout_script,
               const std::vector<uint8_t>& scriptSig,
               const std::vector<std::vector<uint8_t>>& witness)
{
    OutputScriptType prev_type =
        classify_output_script(prevout_script);

    switch (prev_type)
    {
        case OutputScriptType::P2PKH:
            return InputScriptType::P2PKH;

        case OutputScriptType::P2WPKH:
            return InputScriptType::P2WPKH;

        case OutputScriptType::P2WSH:
            return InputScriptType::P2WSH;

        case OutputScriptType::P2TR:
        {
            if (witness.size() == 1)
                return InputScriptType::P2TR_KEYPATH;

            if (witness.size() >= 2)
                return InputScriptType::P2TR_SCRIPTPATH;

            break;
        }

        case OutputScriptType::P2SH:
        {
            auto redeem_opt = extract_last_push(scriptSig);
            if (!redeem_opt)
                break;

            const auto& redeem = *redeem_opt;

            if (redeem.size() == 22 &&
                redeem[0] == OP_0 &&
                redeem[1] == 0x14)
            {
                return InputScriptType::P2SH_P2WPKH;
            }

            if (redeem.size() == 34 &&
                redeem[0] == OP_0 &&
                redeem[1] == 0x20)
            {
                return InputScriptType::P2SH_P2WSH;
            }

            break;
        }

        default:
            break;
    }

    return InputScriptType::UNKNOWN;
}

std::string output_script_type_str(OutputScriptType t)
{
    switch (t)
    {
    case OutputScriptType::P2PKH:
        return "p2pkh";
    case OutputScriptType::P2SH:
        return "p2sh";
    case OutputScriptType::P2WPKH:
        return "p2wpkh";
    case OutputScriptType::P2WSH:
        return "p2wsh";
    case OutputScriptType::P2TR:
        return "p2tr";
    case OutputScriptType::OP_RETURN:
        return "op_return";
    default:
        return "unknown";
    }
}

std::string input_script_type_str(InputScriptType t)
{
    switch (t)
    {
    case InputScriptType::P2PKH:            return "p2pkh";
    case InputScriptType::P2SH_P2WPKH:     return "p2sh-p2wpkh";
    case InputScriptType::P2SH_P2WSH:      return "p2sh-p2wsh";
    case InputScriptType::P2WPKH:          return "p2wpkh";
    case InputScriptType::P2WSH:           return "p2wsh";
    case InputScriptType::P2TR_KEYPATH:    return "p2tr_keypath";
    case InputScriptType::P2TR_SCRIPTPATH: return "p2tr_scriptpath";
    default:                               return "unknown";
    }
}

std::string op_return_protocol_str(OPReturnProtocol p)
{
    switch (p)
    {
    case OPReturnProtocol::OMNI:            return "omni";
    case OPReturnProtocol::OPENTIMESTAMPS:  return "opentimestamps";
    default:                               return "unknown";
    }
}
