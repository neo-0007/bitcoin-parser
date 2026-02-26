#include "transaction.h"

// TxIn

// Returns true if relative locktime is enabled
// BIP 68 (https://github.com/bitcoin/bips/blob/master/bip-0068.mediawiki)
// Disabled if bit 31 is set
bool TxIn::RLT_enabled() const
{
    return !(sequence & (1u << 31));
}

// Returns an object with all relative locktime data calculated
RelativeLocktimeInfo TxIn::get_rlt_info() const
{
    RelativeLocktimeInfo info{};
    info.enabled = false;
    info.value = 0;

    if (!RLT_enabled())
        return info;

    info.enabled = true;

    // Bit 22 of sequence determines whether the lock is time-based or block-based
    bool time_flag = sequence & (1u << 22);

    if (time_flag)
        info.type = RelativeLockTimeType::UNIX_TIMESTAMP;
    else
        info.type = RelativeLockTimeType::BLOCK_HEIGHT;

    // Lower 16 bits hold the lock value
    // For time-based RLT the caller must multiply by 512 to get seconds (per struct comment)
    info.value = static_cast<uint16_t>(sequence & 0xFFFF);

    return info;
}

// Transaction Constructor (Parser)

// Default constructor
Transaction::Transaction()
{
    version = 0;
    locktime = 0;
    isSegwit = false;
    TxIdHash.fill(0);
    WTxIdHash.fill(0);
}

// Constructor that builds Transaction using the raw transaction array of bytes
Transaction::Transaction(const std::vector<uint8_t> &raw)
{
    size_t offset = 0;

    if (raw.size() < 4)
        throw std::runtime_error("Transaction: raw data too short");

    // Parse 4-byte version field (little-endian)
    version = read_uint32_le(raw, offset);
    offset += 4;

    // SegWit detection: marker byte 0x00 followed by flag byte 0x01
    // BIP 141 (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki)
    if (offset + 1 < raw.size() && raw[offset] == 0x00 && raw[offset + 1] == 0x01)
    {
        isSegwit = true;
        offset += 2;
    }
    else
    {
        isSegwit = false;
    }

    // Parse inputs
    uint64_t input_count = read_varint(raw, offset);

    for (uint64_t i = 0; i < input_count; i++)
    {
        TxIn in{};

        // Previous transaction ID: 32 bytes
        if (offset + 32 > raw.size())
            throw std::runtime_error("Transaction: truncated prevTxId");

        for (int j = 0; j < 32; j++)
            in.prevTxId[j] = raw[offset++];

        // Output index of the previous transaction being spent
        if (offset + 4 > raw.size())
            throw std::runtime_error("Transaction: truncated vout");

        in.vout = read_uint32_le(raw, offset);
        offset += 4;

        // scriptSig: unlocking script for this input
        uint64_t script_len = read_varint(raw, offset);

        if (offset + script_len > raw.size())
            throw std::runtime_error("Transaction: truncated scriptSig");

        in.scriptSig.insert(in.scriptSig.end(),
                            raw.begin() + offset,
                            raw.begin() + offset + script_len);
        offset += script_len;

        // Sequence number; used for RBF and relative locktime (BIP 68 / BIP 125)
        if (offset + 4 > raw.size())
            throw std::runtime_error("Transaction: truncated sequence");

        in.sequence = read_uint32_le(raw, offset);
        offset += 4;

        inputs.push_back(in);
    }

    // Parse outputs
    uint64_t output_count = read_varint(raw, offset);

    for (uint64_t i = 0; i < output_count; i++)
    {
        TxOut out{};

        // Amount in satoshis (1 satoshi = 0.00000001 BTC)
        if (offset + 8 > raw.size())
            throw std::runtime_error("Transaction: truncated amount");

        out.amount = read_uint64_le(raw, offset);
        offset += 8;

        // scriptPubKey: locking script for this output
        uint64_t script_len = read_varint(raw, offset);

        if (offset + script_len > raw.size())
            throw std::runtime_error("Transaction: truncated scriptPubKey");

        out.scriptPubKey.insert(out.scriptPubKey.end(),
                                raw.begin() + offset,
                                raw.begin() + offset + script_len);
        offset += script_len;

        outputs.push_back(out);
    }

    // Parse witness data (only present in SegWit transactions)
    // Each input has its own witness stack: a list of byte vectors
    if (isSegwit)
    {
        for (auto &in : inputs)
        {
            uint64_t item_count = read_varint(raw, offset);

            for (uint64_t i = 0; i < item_count; i++)
            {
                uint64_t item_len = read_varint(raw, offset);

                if (offset + item_len > raw.size())
                    throw std::runtime_error("Transaction: truncated witness item");

                std::vector<uint8_t> item(raw.begin() + offset,
                                          raw.begin() + offset + item_len);
                offset += item_len;

                in.witness.push_back(item);
            }
        }
    }

    // 4-byte locktime field
    if (offset + 4 > raw.size())
        throw std::runtime_error("Transaction: truncated locktime");

    locktime = read_uint32_le(raw, offset);
    offset += 4;

    // Precompute and cache both hashes once at construction time
    TxIdHash = reverse_32(double_sha256(serialize_legacy()));
    WTxIdHash = isSegwit
                    ? reverse_32(double_sha256(serialize_with_witness()))
                    : TxIdHash;
}

Transaction::Transaction(const std::vector<uint8_t> &raw, size_t &off)
{
    version = read_uint32_le(raw, off);
    off += 4;

    if (off + 1 < raw.size() && raw[off] == 0x00 && raw[off + 1] == 0x01)
    {
        isSegwit = true;
        off += 2;
    }
    else
    {
        isSegwit = false;
    }

    uint64_t input_count = read_varint(raw, off);
    for (uint64_t i = 0; i < input_count; i++)
    {
        TxIn in{};
        if (off + 32 > raw.size())
            throw std::runtime_error("Transaction: truncated prevTxId");
        for (int j = 0; j < 32; j++)
            in.prevTxId[j] = raw[off++];
        in.vout = read_uint32_le(raw, off);
        off += 4;
        uint64_t script_len = read_varint(raw, off);
        in.scriptSig.insert(in.scriptSig.end(),
                            raw.begin() + off, raw.begin() + off + script_len);
        off += script_len;
        in.sequence = read_uint32_le(raw, off);
        off += 4;
        inputs.push_back(in);
    }

    uint64_t output_count = read_varint(raw, off);
    for (uint64_t i = 0; i < output_count; i++)
    {
        TxOut out{};
        out.amount = read_uint64_le(raw, off);
        off += 8;
        uint64_t script_len = read_varint(raw, off);
        out.scriptPubKey.insert(out.scriptPubKey.end(),
                                raw.begin() + off, raw.begin() + off + script_len);
        off += script_len;
        outputs.push_back(out);
    }

    if (isSegwit)
    {
        for (auto &in : inputs)
        {
            uint64_t item_count = read_varint(raw, off);
            for (uint64_t i = 0; i < item_count; i++)
            {
                uint64_t item_len = read_varint(raw, off);
                if (off + item_len > raw.size())
                    throw std::runtime_error("Transaction: truncated witness item");
                std::vector<uint8_t> item(raw.begin() + off,
                                          raw.begin() + off + item_len);
                off += item_len;
                in.witness.push_back(item);
            }
        }
    }

    locktime = read_uint32_le(raw, off);
    off += 4;

    TxIdHash = reverse_32(double_sha256(serialize_legacy()));
    WTxIdHash = isSegwit
                    ? reverse_32(double_sha256(serialize_with_witness()))
                    : TxIdHash;
}

// SegWit Getter

// Getter for the private isSegwit variable
bool Transaction::is_segwit() const
{
    return isSegwit;
}

// Size / Weight

// Calculate and return size of transaction in bytes
size_t Transaction::get_size_bytes() const
{
    return serialize_with_witness().size();
}

// Calculate and return weight of transaction
// Follows BIP 141: weight = base_size * 4 + witness_size
// (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#transaction-size-calculations)
size_t Transaction::get_weight() const
{
    size_t base = serialize_legacy().size();
    size_t total = serialize_with_witness().size();
    size_t witness = total - base;

    return base * 4 + witness;
}

// Calculate and return virtual bytes (vbytes) of transaction
// Follows BIP 141: vbytes = ceil(weight / 4)
// (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#transaction-size-calculations)
size_t Transaction::get_vbytes() const
{
    return (get_weight() + 3) / 4;
}

// Locktime

// Returns the type of locktime:
//   NONE           - locktime is 0, no lock applied
//   BLOCK_HEIGHT   - locktime < 500,000,000 (treated as a block number)
//   UNIX_TIMESTAMP - locktime >= 500,000,000 (treated as a Unix timestamp)
LockTimeType Transaction::get_locktime_type() const
{
    if (locktime == 0)
        return LockTimeType::NONE;

    if (locktime < 500000000)
        return LockTimeType::BLOCK_HEIGHT;

    return LockTimeType::UNIX_TIMESTAMP;
}

// Returns true if the transaction is final, false otherwise
// A transaction is final if locktime is 0, or if ALL inputs have sequence == 0xFFFFFFFF
bool Transaction::is_final()
{
    if (locktime == 0)
        return true;

    for (const auto &in : inputs)
        if (in.sequence != 0xFFFFFFFF)
            return false;

    return true;
}

// Returns true if locktime is enabled, false otherwise
// Locktime is only enforced when at least one input has sequence < 0xFFFFFFFF
bool Transaction::locktime_enabled()
{
    for (const auto &in : inputs)
        if (in.sequence < 0xFFFFFFFF)
            return true;

    return false;
}

// Returns true if Replace-By-Fee (RBF) is enabled
// Introduced in BIP 125 (https://github.com/bitcoin/bips/blob/master/bip-0125.mediawiki)
// RBF is signalled when at least one input has sequence < 0xFFFFFFFE
bool Transaction::RBF_enabled() const
{
    for (const auto &in : inputs)
        if (in.sequence < 0xFFFFFFFE)
            return true;

    return false;
}

// Serialization (private)

// Serialize the transaction to bytes without witness data
// Used internally to compute TxIdHash
std::vector<uint8_t> Transaction::serialize_legacy() const
{
    std::vector<uint8_t> out;

    write_uint32_le(out, version);

    write_varint(out, inputs.size());

    for (const auto &in : inputs)
    {
        out.insert(out.end(), in.prevTxId.begin(), in.prevTxId.end());
        write_uint32_le(out, in.vout);

        write_varint(out, in.scriptSig.size());
        out.insert(out.end(), in.scriptSig.begin(), in.scriptSig.end());

        write_uint32_le(out, in.sequence);
    }

    write_varint(out, outputs.size());

    for (const auto &o : outputs)
    {
        write_uint64_le(out, o.amount);

        write_varint(out, o.scriptPubKey.size());
        out.insert(out.end(), o.scriptPubKey.begin(), o.scriptPubKey.end());
    }

    write_uint32_le(out, locktime);

    return out;
}

// Serialize the transaction to bytes with witness data
// Used internally to compute WTxIdHash
// For non-segwit transactions falls back to serialize_legacy()
std::vector<uint8_t> Transaction::serialize_with_witness() const
{
    if (!isSegwit)
        return serialize_legacy();

    std::vector<uint8_t> out;

    write_uint32_le(out, version);

    // SegWit marker (0x00) and flag (0x01)
    out.push_back(0x00);
    out.push_back(0x01);

    write_varint(out, inputs.size());

    for (const auto &in : inputs)
    {
        out.insert(out.end(), in.prevTxId.begin(), in.prevTxId.end());
        write_uint32_le(out, in.vout);

        write_varint(out, in.scriptSig.size());
        out.insert(out.end(), in.scriptSig.begin(), in.scriptSig.end());

        write_uint32_le(out, in.sequence);
    }

    write_varint(out, outputs.size());

    for (const auto &o : outputs)
    {
        write_uint64_le(out, o.amount);

        write_varint(out, o.scriptPubKey.size());
        out.insert(out.end(), o.scriptPubKey.begin(), o.scriptPubKey.end());
    }

    // Witness data: one witness stack per input
    // Each stack: item_count (varint), then per item: length (varint) + bytes
    for (const auto &in : inputs)
    {
        write_varint(out, in.witness.size());

        for (const auto &item : in.witness)
        {
            write_varint(out, item.size());
            out.insert(out.end(), item.begin(), item.end());
        }
    }

    write_uint32_le(out, locktime);

    return out;
}

// TXID & WTXID

// Returns the precomputed TxID
// TxID = double-SHA256 of legacy serialization, reversed (big-endian display format)
std::array<uint8_t, 32> Transaction::get_txid() const
{
    return TxIdHash;
}

// Returns the precomputed wTxID
// wTxID = double-SHA256 of witness serialization, reversed
// For non-segwit transactions wTxID == TxID per BIP 141
std::array<uint8_t, 32> Transaction::get_wtxid() const
{
    return WTxIdHash;
}

std::string locktime_type_str(LockTimeType t)
{
    switch (t)
    {
    case LockTimeType::UNIX_TIMESTAMP:
        return "unix_timestamp";
    case LockTimeType::BLOCK_HEIGHT:
        return "block_height";
    default:
        return "none";
    }
}