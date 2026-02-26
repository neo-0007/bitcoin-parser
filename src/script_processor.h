#ifndef SCRIPT_PROCESSOR_H
#define SCRIPT_PROCESSOR_H

#include "script.h"
#include <optional>
#include "utilities.h"

enum class InputScriptType {
    P2PKH,
    P2SH_P2WPKH,
    P2SH_P2WSH,
    P2WPKH,
    P2WSH,
    P2TR_KEYPATH,
    P2TR_SCRIPTPATH,
    UNKNOWN
};

enum class OPReturnProtocol {
    OMNI,
    OPENTIMESTAMPS,
    UNKNOWN
};

struct OPReturnPayload {
    std::vector<uint8_t> data;
    std::optional<std::string> utf8;
    OPReturnProtocol protocol;
};

struct ProcessedScriptPubKey {
    OutputScriptType type;
    std::optional<std::string> address;
    std::optional<OPReturnPayload> op_return;
};

ProcessedScriptPubKey process_output_script(const std::vector<uint8_t>& script);

InputScriptType classify_input(
    const std::vector<uint8_t>& prevout_script,
    const std::vector<uint8_t>& scriptSig,
    const std::vector<std::vector<uint8_t>>& witness);

// declarations only — defined in script_processor.cpp
std::string input_script_type_str(InputScriptType t);
std::string op_return_protocol_str(OPReturnProtocol p);
std::string output_script_type_str(OutputScriptType t);

#endif