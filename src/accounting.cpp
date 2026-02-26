#include "accounting.h"
#include "json_helper.h"
#include <unordered_map>
#include <iostream>

// InputTxnWithPrevout

InputTxnWithPrevout::InputTxnWithPrevout(const std::string &filepath)
{
    *this = json_to_input_txn_with_prevout(get_json(filepath));
}

// TxnAnalyzer

TxnAnalyzer::TxnAnalyzer(
    const Transaction &tx,
    const std::vector<Prevout> &prevouts,
    const std::string &network)
    : tx_(tx), network_(network)
{
    // Build prevout lookup map: "txid_hex:vout" → Prevout*
    std::unordered_map<std::string, const Prevout *> prevout_map;
    for (const Prevout &p : prevouts)
    {
        std::string key = bytes_to_hex(p.txid) + ":" + std::to_string(p.vout);
        if (prevout_map.count(key))
            throw std::runtime_error("Duplicate prevout in fixture: " + key);
        prevout_map[key] = &p;
    }

    // inputs first (fee needs input sats), then fee, then rest
    build_inputs(prevout_map);
    build_outputs();
    build_fee_info();
    build_segwit_savings();
    build_warnings();
}

void TxnAnalyzer::build_inputs(
    const std::unordered_map<std::string, const Prevout *> &prevout_map)
{
    for (const TxIn &in : tx_.inputs)
    {
        AccountedInput ai;

        std::array<uint8_t, 32> rev = reverse_32(in.prevTxId);
        ai.txid     = bytes_to_hex(rev);
        ai.vout     = in.vout;
        ai.sequence = in.sequence;
        ai.rlt      = in.get_rlt_info();

        // Coinbase input: prevTxId all zeros, vout 0xFFFFFFFF
        bool is_coinbase = (in.vout == 0xFFFFFFFF);
        for (int b = 0; b < 32 && is_coinbase; ++b)
            if (in.prevTxId[b] != 0) { is_coinbase = false; }

        if (is_coinbase)
        {
            ai.script_sig_hex = bytes_to_hex(in.scriptSig);
            ai.script_asm     = disassemble_script(in.scriptSig);
            ai.script_type    = "coinbase";
            ai.prevout_value_sats        = 0;
            ai.prevout_script_pubkey_hex = "";
            inputs_.push_back(std::move(ai));
            continue;
        }

        // Normal input — lookup prevout
        std::string key = ai.txid + ":" + std::to_string(ai.vout);
        auto it = prevout_map.find(key);
        if (it == prevout_map.end())
            throw std::runtime_error("Missing prevout for input: " + key);

        const Prevout *prev = it->second;

        ai.script_sig_hex = bytes_to_hex(in.scriptSig);
        ai.script_asm     = disassemble_script(in.scriptSig);

        for (const auto &item : in.witness)
            ai.witness.push_back(bytes_to_hex(item));

        InputScriptType ist = classify_input(
            prev->script_pubkey_hex, in.scriptSig, in.witness);
        ai.script_type = input_script_type_str(ist);

        ProcessedScriptPubKey pspk =
            process_output_script(prev->script_pubkey_hex);
        if (pspk.address)
            ai.address = pspk.address;

        ai.prevout_value_sats        = prev->value_sats;
        ai.prevout_script_pubkey_hex = bytes_to_hex(prev->script_pubkey_hex);

        inputs_.push_back(std::move(ai));
    }
}

void TxnAnalyzer::build_outputs()
{
    for (size_t i = 0; i < tx_.outputs.size(); ++i)
    {
        const TxOut &out = tx_.outputs[i];

        AccountedOutput ao;
        ao.n = static_cast<uint32_t>(i);
        ao.value_sats = out.amount;
        ao.script_pubkey_hex = bytes_to_hex(out.scriptPubKey);
        ao.script_asm = disassemble_script(out.scriptPubKey);

        ProcessedScriptPubKey pspk = process_output_script(out.scriptPubKey);
        ao.script_type = output_script_type_str(pspk.type);

        if (pspk.address)
            ao.address = pspk.address;

        if (pspk.type == OutputScriptType::OP_RETURN && pspk.op_return)
        {
            const OPReturnPayload &payload = *pspk.op_return;
            ao.op_return_data_hex = bytes_to_hex(payload.data);
            ao.op_return_data_utf8 = payload.utf8;
            ao.op_return_protocol = op_return_protocol_str(payload.protocol);
        }

        outputs_.push_back(std::move(ao));
    }
}

void TxnAnalyzer::build_fee_info()
{
    total_input_sats_  = 0;
    total_output_sats_ = 0;

    for (const auto &ai : inputs_)
        if (ai.script_type != "coinbase")   // skip coinbase
            total_input_sats_ += ai.prevout_value_sats;

    for (const auto &ao : outputs_)
        total_output_sats_ += ao.value_sats;

    // For coinbase tx, fee is 0 (no real inputs)
    fee_sats_ = (total_input_sats_ >= total_output_sats_)
                    ? total_input_sats_ - total_output_sats_
                    : 0;

    size_t vb = tx_.get_vbytes();
    fee_rate_sat_vb_ = (vb > 0)
        ? std::round((static_cast<double>(fee_sats_) / vb) * 100.0) / 100.0
        : 0.0;
}

void TxnAnalyzer::build_segwit_savings()
{
    size_t total = tx_.get_size_bytes();
    size_t w_act = tx_.get_weight();

    size_t non_witness_bytes, witness_bytes, weight_if_legacy;

    if (tx_.is_segwit())
    {
        // BIP 141: weight = 4*base + witness, total = base + 2 + witness
        // Solving: base = (w_act + 2 - total) / 3
        size_t base = (w_act + 2 - total) / 3;
        witness_bytes = total - base - 2;
        non_witness_bytes = base;
        weight_if_legacy = total * 4;
    }
    else
    {
        witness_bytes = 0;
        non_witness_bytes = total;
        weight_if_legacy = total * 4;
    }

    double savings = (weight_if_legacy > 0)
                         ? std::round((1.0 - static_cast<double>(w_act) /
                                                 static_cast<double>(weight_if_legacy)) *
                                      10000.0) /
                               100.0
                         : 0.0;

    segwit_savings_ = {witness_bytes, non_witness_bytes, total,
                       w_act, weight_if_legacy, savings};
}

void TxnAnalyzer::build_warnings()
{
    if (tx_.RBF_enabled())
        warnings_.push_back({WarningCode::RBF_SIGNALING});

    if (fee_sats_ > 1'000'000 || fee_rate_sat_vb_ > 200.0)
        warnings_.push_back({WarningCode::HIGH_FEE});

    for (const auto &out : outputs_)
        if (out.script_type != "op_return" && out.value_sats < 546)
        {
            warnings_.push_back({WarningCode::DUST_OUTPUT});
            break;
        }

    for (const auto &out : outputs_)
        if (out.script_type == "unknown")
        {
            warnings_.push_back({WarningCode::UNKNOWN_OUTPUT_SCRIPT});
            break;
        }
}

// BlockAnalyzer

BlockAnalyzer::BlockAnalyzer(const Block &block,
                             const UndoBlock &undo,
                             const std::string &network)
{
    analyze(block, undo, network);
}

//
// Top-level analysis
//

void BlockAnalyzer::analyze(const Block &block,
                            const UndoBlock &undo,
                            const std::string &network)
{
    analyze_header(block);

    const std::vector<Transaction> &txs = block.getTransactions();

    if (txs.empty())
        throw std::runtime_error("Block has no transactions");

    tx_count = txs.size();

    analyze_coinbase(txs[0]);
    analyze_transactions(block, undo, network);
    compute_block_stats();
}

void BlockAnalyzer::analyze_header(const Block &block)
{
    const BlockHeader &hdr = block.getHeader();

    block_header.version = hdr.getVersion();
    block_header.timestamp = hdr.getTimestamp();
    block_header.nonce = hdr.getNonce();
    block_header.block_hash = hdr.getHashStr();

    block_header.prev_block_hash =
        bytes_to_hex(reverse_32(hdr.getPreviousBlock()));

    block_header.merkle_root =
        bytes_to_hex(reverse_32(hdr.getMerkleRoot()));

    // bits as big-endian hex
    uint32_t bits_val = hdr.getBits();
    std::vector<uint8_t> bits_bytes = {
        uint8_t(bits_val >> 24),
        uint8_t(bits_val >> 16),
        uint8_t(bits_val >> 8),
        uint8_t(bits_val)};
    block_header.bits = bytes_to_hex(bits_bytes);

    // Verify merkle root
    std::vector<uint8_t> computed = block.calcMerkleRoot();
    auto header_root = hdr.getMerkleRoot();

    block_header.merkle_root_valid =
        std::equal(computed.begin(), computed.end(),
                   header_root.begin());
}

void BlockAnalyzer::analyze_coinbase(const Transaction &cb_tx)
{
    if (cb_tx.inputs.empty())
        throw std::runtime_error("Coinbase has no inputs");

    const std::vector<uint8_t> &script = cb_tx.inputs[0].scriptSig;
    coinbase.coinbase_script_hex = bytes_to_hex(script);

    // BIP34: first byte = push length, followed by LE height bytes
    coinbase.bip34_height = 0;
    if (!script.empty())
    {
        uint8_t push_len = script[0];
        if (push_len >= 1 && push_len <= 4 &&
            script.size() >= static_cast<size_t>(1 + push_len))
        {
            uint32_t h = 0;
            for (uint8_t i = 0; i < push_len; ++i)
                h |= static_cast<uint32_t>(script[1 + i]) << (8 * i);
            coinbase.bip34_height = h;
        }
    }

    coinbase.total_output_sats = 0;
    for (const auto &out : cb_tx.outputs)
        coinbase.total_output_sats += out.amount;
}

void BlockAnalyzer::analyze_transactions(const Block &block,
                                         const UndoBlock &undo,
                                         const std::string &network)
{
    const auto &txs       = block.getTransactions();
    const auto &undo_txs  = undo.getTransactions();

    std::cerr << "[debug] block txs=" << txs.size()
              << " undo txs=" << undo_txs.size()
              << "\n";

    if (undo_txs.size() != txs.size() - 1)
        throw std::runtime_error("Undo mismatch: tx count does not match");

    transactions.reserve(txs.size());

    // Coinbase (no undo)
    transactions.emplace_back(txs[0], std::vector<Prevout>{}, network);

    for (size_t i = 1; i < txs.size(); ++i)
    {
        const auto &inputs = txs[i].inputs;
        const auto &undo_inputs = undo_txs[i - 1].getInputs();

        if (inputs.size() != undo_inputs.size())
            throw std::runtime_error("Undo mismatch: input count mismatch");

        std::vector<Prevout> prevouts;
        prevouts.reserve(inputs.size());

        for (size_t j = 0; j < inputs.size(); ++j)
        {
            Prevout p;
            p.txid = reverse_32(inputs[j].prevTxId);
            p.vout = inputs[j].vout;

            p.value_sats        = undo_inputs[j].value;
            p.script_pubkey_hex = undo_inputs[j].scriptPubKey;

            prevouts.push_back(std::move(p));
        }

        transactions.emplace_back(txs[i], prevouts, network);
    }
}

void BlockAnalyzer::compute_block_stats()
{
    block_stats.total_fees_sats = 0;
    block_stats.total_weight = 0;
    block_stats.script_type_summary.clear();

    uint64_t total_vbytes = 0;

    for (size_t i = 0; i < transactions.size(); ++i)
    {
        const TxnAnalyzer &ta = transactions[i];

        block_stats.total_weight += ta.weight();
        total_vbytes += ta.vbytes();

        if (i > 0)
            block_stats.total_fees_sats += ta.fee_sats();

        for (const auto &out : ta.vout())
            block_stats.script_type_summary[out.script_type]++;
    }

    block_stats.avg_fee_rate_sat_vb =
        total_vbytes > 0
            ? static_cast<double>(block_stats.total_fees_sats) /
                  total_vbytes
            : 0.0;
}