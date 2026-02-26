#include "json_helper.h"

nlohmann::ordered_json analyzed_txn_to_json(const TxnAnalyzer &ta)
{

    auto rlt_type_str = [](RelativeLockTimeType t) -> std::string
    {
        return t == RelativeLockTimeType::UNIX_TIMESTAMP ? "seconds" : "blocks";
    };

    //vin
    json vin = json::array();
    for (const auto &in : ta.vin())
    {
        json witness = json::array();
        for (const auto &w : in.witness)
            witness.push_back(w);

        vin.push_back({{"txid", in.txid},
                       {"vout", in.vout},
                       {"sequence", in.sequence},
                       {"script_sig_hex", in.script_sig_hex},
                       {"script_asm", in.script_asm},
                       {"witness", witness},
                       {"script_type", in.script_type},
                       {"address", in.address ? json(*in.address) : json(nullptr)},
                       {"prevout", {{"value_sats", in.prevout_value_sats}, {"script_pubkey_hex", in.prevout_script_pubkey_hex}}},
                       {"relative_timelock", {{"enabled", in.rlt.enabled}, {"type", rlt_type_str(in.rlt.type)}, {"value", in.rlt.value}}}});
    }

    //vout
    json vout = json::array();
    for (const auto &out : ta.vout())
    {
        json output = {
            {"n", out.n},
            {"value_sats", out.value_sats},
            {"script_pubkey_hex", out.script_pubkey_hex},
            {"script_asm", out.script_asm},
            {"script_type", out.script_type},
            {"address", out.address ? json(*out.address) : json(nullptr)}};

        if (out.op_return_data_hex)
        {
            output["op_return_data_hex"] = *out.op_return_data_hex;
            output["op_return_data_utf8"] = out.op_return_data_utf8
                                                ? json(*out.op_return_data_utf8)
                                                : json(nullptr);
            output["op_return_protocol"] = out.op_return_protocol
                                               ? json(*out.op_return_protocol)
                                               : json(nullptr);
        }

        vout.push_back(std::move(output));
    }

    //warnings
    json warnings = json::array();
    for (const auto &w : ta.warnings())
        warnings.push_back({{"code", w.code_str()}});

    //segwit_savings (null for non-segwit)
    json segwit_savings;
    if (ta.segwit())
    {
        const auto &ss = ta.segwit_savings();
        segwit_savings = {
            {"witness_bytes", ss.witness_bytes},
            {"non_witness_bytes", ss.non_witness_bytes},
            {"total_bytes", ss.total_bytes},
            {"weight_actual", ss.weight_actual},
            {"weight_if_legacy", ss.weight_if_legacy},
            {"savings_pct", ss.savings_pct}};
    }
    else
    {
        segwit_savings = nullptr;
    }

    //root
    return {
        {"ok", true},
        {"network", ta.network()},
        {"segwit", ta.segwit()},
        {"txid", ta.txid()},
        {"wtxid", ta.segwit() ? json(ta.wtxid()) : json(nullptr)},
        {"version", ta.version()},
        {"locktime", ta.locktime()},
        {"size_bytes", ta.size_bytes()},
        {"weight", ta.weight()},
        {"vbytes", ta.vbytes()},
        {"total_input_sats", ta.total_input_sats()},
        {"total_output_sats", ta.total_output_sats()},
        {"fee_sats", ta.fee_sats()},
        {"fee_rate_sat_vb", ta.fee_rate_sat_vb()},
        {"rbf_signaling", ta.rbf_signaling()},
        {"locktime_type", ta.locktime_type()},
        {"locktime_value", ta.locktime_value()},
        {"segwit_savings", segwit_savings},
        {"vin", vin},
        {"vout", vout},
        {"warnings", warnings}};
}

// Make json object from filepath
nlohmann::json get_json(std::string filepath)
{
    std::ifstream f(filepath);
    if (!f.is_open())
        throw std::runtime_error("get_json: cannot open file: " + filepath);

    nlohmann::json j;
    try
    {
        f >> j;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        throw std::runtime_error("get_json: JSON parse error in '" + filepath + "': " + e.what());
    }

    return j;
}

// Takes in the json input and returns us the Input Txn
InputTxnWithPrevout json_to_input_txn_with_prevout(const nlohmann::json &j)
{
    InputTxnWithPrevout result;

    // network
    result.network = j.at("network").get<std::string>();
    // raw transaction bytes
    result.raw_tx_bytes =
        hex_to_bytes(j.at("raw_tx").get<std::string>());
    // prevouts array
    const nlohmann::json &prevouts_json = j.at("prevouts");

    for (size_t i = 0; i < prevouts_json.size(); ++i)
    {
        const nlohmann::json &p = prevouts_json.at(i);

        Prevout prev;

        std::vector<uint8_t> txid_vec =
            hex_to_bytes(p.at("txid").get<std::string>());

        if (txid_vec.size() != 32)
            throw std::runtime_error("Invalid txid length");

        std::copy(txid_vec.begin(),
                  txid_vec.end(),
                  prev.txid.begin());

        prev.vout = p.at("vout").get<uint32_t>();
        prev.value_sats = p.at("value_sats").get<uint64_t>();
        prev.script_pubkey_hex =
            hex_to_bytes(p.at("script_pubkey_hex").get<std::string>());

        result.prevouts.push_back(prev);
    }

    return result;
}

static const char *SCRIPT_TYPE_ORDER[] = {
    "p2wpkh", "p2tr", "p2sh", "p2pkh", "p2wsh", "op_return", "unknown"};

nlohmann::ordered_json block_to_json(const BlockAnalyzer &ba)
{
    //block_header
    const auto &h = ba.block_header;
    json block_header = {
        {"version", h.version},
        {"prev_block_hash", h.prev_block_hash},
        {"merkle_root", h.merkle_root},
        {"merkle_root_valid", h.merkle_root_valid},
        {"timestamp", h.timestamp},
        {"bits", h.bits},
        {"nonce", h.nonce},
        {"block_hash", h.block_hash}};

    //coinbase
    const auto &cb = ba.coinbase;
    json coinbase = {
        {"bip34_height", cb.bip34_height},
        {"coinbase_script_hex", cb.coinbase_script_hex},
        {"total_output_sats", cb.total_output_sats}};

    //transactions
    json transactions = json::array();
    for (const auto &ta : ba.transactions)
        transactions.push_back(analyzed_txn_to_json(ta));

    //block_stats
    const auto &s = ba.block_stats;

    json script_summary = json::object();
    for (const char *t : SCRIPT_TYPE_ORDER)
    {
        auto it = s.script_type_summary.find(t);
        if (it != s.script_type_summary.end() && it->second > 0)
            script_summary[t] = it->second;
    }
    // Any extra types not in fixed order
    for (const auto &[k, v] : s.script_type_summary)
        if (!script_summary.contains(k) && v > 0)
            script_summary[k] = v;

    json block_stats = {
        {"total_fees_sats", s.total_fees_sats},
        {"total_weight", s.total_weight},
        {"avg_fee_rate_sat_vb", s.avg_fee_rate_sat_vb},
        {"script_type_summary", script_summary}};

    //root
    return {
        {"ok", ba.ok},
        {"mode", ba.mode},
        {"block_header", block_header},
        {"tx_count", ba.tx_count},
        {"coinbase", coinbase},
        {"transactions", transactions},
        {"block_stats", block_stats}};
}
