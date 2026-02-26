#ifndef ACCOUNTING_H
#define ACCOUNTING_H

#include "transaction.h"
#include "block.h"
#include "script.h"
#include "script_processor.h"
#include "utilities.h"
#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include <unordered_map>
#include <map>

// INPUT part SINGLE TXN MODE

// Prevout data strcture we get from input
// will be used by InputTxnWithPrevout class
class Prevout
{
public:
    std::array<uint8_t, 32> txid;
    uint32_t vout;
    uint64_t value_sats;
    std::vector<uint8_t> script_pubkey_hex;
};

// Input (from JSON file)
class InputTxnWithPrevout
{
public:
    std::string network;
    std::vector<uint8_t> raw_tx_bytes;
    std::vector<Prevout> prevouts;

    InputTxnWithPrevout() = default;

    // Constructor that takes in the filepath and creates a InputTxnWithPrevout Object
    // defined in json_helper.cpp
    InputTxnWithPrevout(const std::string &filepath);
};

// OUTPUT part SINGLE TXN MODE

// These are classes used by our main output class
// Conatins all the fields we want in our json

// Segwit savings breakdown
struct SegwitSavings
{
    size_t witness_bytes;
    size_t non_witness_bytes;
    size_t total_bytes;
    size_t weight_actual;
    size_t weight_if_legacy;
    double savings_pct;
};

// Warning codes
enum class WarningCode
{
    RBF_SIGNALING,
    HIGH_FEE,
    DUST_OUTPUT,
    UNKNOWN_OUTPUT_SCRIPT,
};

struct TxWarning
{
    WarningCode code;

    std::string code_str() const
    {
        switch (code)
        {
        case WarningCode::RBF_SIGNALING:
            return "RBF_SIGNALING";
        case WarningCode::HIGH_FEE:
            return "HIGH_FEE";
        case WarningCode::DUST_OUTPUT:
            return "DUST_OUTPUT";
        case WarningCode::UNKNOWN_OUTPUT_SCRIPT:
            return "UNKNOWN_OUTPUT_SCRIPT";
        }
        return "UNKNOWN";
    }
};

// Processed input field in a txn
struct AccountedInput
{
    std::string txid; // hex, reversed (display order)
    uint32_t vout;
    uint32_t sequence;
    std::string script_sig_hex;
    std::string script_asm;
    std::vector<std::string> witness; // each item hex-encoded
    std::string script_type;
    std::optional<std::string> address;

    uint64_t prevout_value_sats;
    std::string prevout_script_pubkey_hex;

    RelativeLocktimeInfo rlt;
};

// Processed output
struct AccountedOutput
{
    uint32_t n;
    uint64_t value_sats;
    std::string script_pubkey_hex;
    std::string script_asm;
    std::string script_type;
    std::optional<std::string> address;

    std::optional<std::string> op_return_data_hex;
    std::optional<std::string> op_return_data_utf8;
    std::optional<std::string> op_return_protocol;
};

// Main accounting class for Txn
// Contains all the info we want for our final output
class TxnAnalyzer
{
public:
    TxnAnalyzer(
        const Transaction &tx,
        const std::vector<Prevout> &prevouts,
        const std::string &network);

    bool ok() const
    {
        return true;
    }
    std::string network() const { return network_; }
    bool segwit() const { return tx_.is_segwit(); }
    std::string txid() const { return bytes_to_hex(tx_.get_txid()); }
    std::string wtxid() const { return bytes_to_hex(tx_.get_wtxid()); }
    uint32_t version() const { return tx_.version; }
    uint32_t locktime() const { return tx_.locktime; }
    size_t size_bytes() const { return tx_.get_size_bytes(); }
    size_t weight() const { return tx_.get_weight(); }
    size_t vbytes() const { return tx_.get_vbytes(); }

    uint64_t total_input_sats() const { return total_input_sats_; }
    uint64_t total_output_sats() const { return total_output_sats_; }
    uint64_t fee_sats() const { return fee_sats_; }
    double fee_rate_sat_vb() const { return fee_rate_sat_vb_; }

    bool rbf_signaling() const { return tx_.RBF_enabled(); }
    std::string locktime_type() const { return locktime_type_str(tx_.get_locktime_type()); }
    uint32_t locktime_value() const { return tx_.locktime; }

    const SegwitSavings &segwit_savings() const { return segwit_savings_; }
    const std::vector<AccountedInput> &vin() const { return inputs_; }
    const std::vector<AccountedOutput> &vout() const { return outputs_; }
    const std::vector<TxWarning> &warnings() const { return warnings_; }

private:
    const Transaction &tx_;
    std::string network_;

    uint64_t total_input_sats_ = 0;
    uint64_t total_output_sats_ = 0;
    uint64_t fee_sats_ = 0;
    double fee_rate_sat_vb_ = 0.0;

    SegwitSavings segwit_savings_{};
    std::vector<AccountedInput> inputs_;
    std::vector<AccountedOutput> outputs_;
    std::vector<TxWarning> warnings_;

    void build_inputs(
        const std::unordered_map<std::string, const Prevout *> &prevout_map);
    void build_outputs();
    void build_fee_info();
    void build_segwit_savings();
    void build_warnings();
};


// Output structure of Block analysis

class BlockStats
{
public:
    uint64_t total_fees_sats = 0;
    uint64_t total_weight = 0;
    double avg_fee_rate_sat_vb = 0.0;

    std::map<std::string, uint64_t> script_type_summary;
};

class CoinBaseInfo
{
public:
    uint32_t bip34_height = 0;
    std::string coinbase_script_hex;
    uint64_t total_output_sats = 0;
};

class AnalyzedBlockHeader
{
public:
    uint32_t version = 0;
    std::string prev_block_hash;
    std::string merkle_root;
    bool merkle_root_valid = false;
    uint32_t timestamp = 0;
    std::string bits; // hex string
    uint32_t nonce = 0;
    std::string block_hash; // hex string
};

class BlockAnalyzer
{
public:
    bool ok = true;
    std::string mode = "block";

    AnalyzedBlockHeader block_header;
    uint64_t tx_count = 0;

    CoinBaseInfo coinbase;

    std::vector<TxnAnalyzer> transactions;

    BlockStats block_stats;

public:
    BlockAnalyzer() = default;

    BlockAnalyzer(const Block &block,
                  const UndoBlock &undo,
                  const std::string &network);

private:
    void analyze(const Block &block,
                 const UndoBlock &undo,
                 const std::string &network);

    void analyze_header(const Block &block);
    void analyze_coinbase(const Transaction &coinbase_tx);
    void analyze_transactions(const Block &block,
                              const UndoBlock &undo,
                              const std::string &network);
    void compute_block_stats();
};

#endif // ACCOUNTING_H
