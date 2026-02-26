#include "block.h"

// BlockHeader

BlockHeader::BlockHeader()
    : version(0), timestamp(0), bits(0), nonce(0)
{
    prevBlock.fill(0);
    merkleRoot.fill(0);
    blockHash.fill(0);
}

BlockHeader::BlockHeader(std::array<uint8_t, 80> &blk_header_hex_bytes)
{
    // Convert to vector once for the read helpers
    std::vector<uint8_t> blk_header_in_v(blk_header_hex_bytes.begin(), blk_header_hex_bytes.end());
    size_t off = 0;

    version = read_uint32_le(blk_header_in_v, off);
    off += 4;

    std::copy(blk_header_hex_bytes.begin() + off, blk_header_hex_bytes.begin() + off + 32, prevBlock.begin());
    off += 32;

    std::copy(blk_header_hex_bytes.begin() + off, blk_header_hex_bytes.begin() + off + 32, merkleRoot.begin());
    off += 32;

    timestamp = read_uint32_le(blk_header_in_v, off);
    off += 4;
    bits = read_uint32_le(blk_header_in_v, off);
    off += 4;
    nonce = read_uint32_le(blk_header_in_v, off);

    calcBlockHash();
}

void BlockHeader::calcBlockHash()
{
    std::vector<uint8_t> buf;
    buf.reserve(80);

    auto w32 = [&](uint32_t val)
    {
        for (int i = 0; i < 4; ++i)
            buf.push_back(static_cast<uint8_t>(val >> (8 * i)));
    };

    w32(version);
    buf.insert(buf.end(), prevBlock.begin(), prevBlock.end());
    buf.insert(buf.end(), merkleRoot.begin(), merkleRoot.end());
    w32(timestamp);
    w32(bits);
    w32(nonce);

    blockHash = reverse_32(double_sha256(buf));
}

int32_t BlockHeader::getVersion() const { return static_cast<int32_t>(version); }
int32_t BlockHeader::getTimestamp() const { return static_cast<int32_t>(timestamp); }
int32_t BlockHeader::getBits() const { return static_cast<int32_t>(bits); }
int32_t BlockHeader::getNonce() const { return static_cast<int32_t>(nonce); }

std::array<uint8_t, 32> BlockHeader::getPreviousBlock() const { return prevBlock; }
std::array<uint8_t, 32> BlockHeader::getMerkleRoot() const { return merkleRoot; }
std::array<uint8_t, 32> BlockHeader::getBlockHash() const { return blockHash; }
std::string BlockHeader::getHashStr() const { return bytes_to_hex(blockHash); }

// Block

Block::Block(std::vector<uint8_t> &blk_hex_bytes)
{
    if (blk_hex_bytes.size() < 8)
        throw std::runtime_error("Block: buffer too short");

    size_t off = 0;

    magic = read_uint32_le(blk_hex_bytes, off);
    off += 4;
    blockSize = read_uint32_le(blk_hex_bytes, off);
    off += 4;

    if (blk_hex_bytes.size() < off + 80)
        throw std::runtime_error("Block: not enough bytes for block header");

    std::array<uint8_t, 80> hdr_bytes;
    std::copy(blk_hex_bytes.begin() + off, blk_hex_bytes.begin() + off + 80, hdr_bytes.begin());
    blockHeader = BlockHeader(hdr_bytes);
    off += 80;

    txnCounter = read_varint(blk_hex_bytes, off);
    txs.reserve(txnCounter);

    for (uint64_t i = 0; i < txnCounter; ++i)
    {
        Transaction tx(blk_hex_bytes, off);
        txs.push_back(std::move(tx));
    }
}

uint32_t Block::getMagicNumber() const { return magic; }
uint32_t Block::getSize() const { return blockSize; }
BlockHeader Block::getHeader() const { return blockHeader; }
uint32_t Block::getTransactionCount() const { return static_cast<uint32_t>(txnCounter); }
const std::vector<Transaction> &Block::getTransactions() const
{
    return txs;
}

uint64_t Block::getOutputsValue() const
{
    uint64_t total = 0;
    for (const auto &tx : txs)
        for (const auto &out : tx.outputs)
            total += out.amount;
    return total;
}

std::vector<uint8_t> Block::calcMerkleRoot() const
{
    if (txs.empty())
        return std::vector<uint8_t>(32, 0);

    // Leaf layer: raw (un-reversed) txid hashes
    std::vector<std::array<uint8_t, 32>> layer;
    layer.reserve(txs.size());
    for (auto &tx : txs)
        layer.push_back(reverse_32(tx.get_txid())); // un-reverse display txid

    while (layer.size() > 1)
    {
        if (layer.size() % 2 != 0)
            layer.push_back(layer.back()); // duplicate last if odd

        std::vector<std::array<uint8_t, 32>> next;
        next.reserve(layer.size() / 2);

        for (size_t i = 0; i < layer.size(); i += 2)
        {
            std::vector<uint8_t> combined;
            combined.insert(combined.end(), layer[i].begin(), layer[i].end());
            combined.insert(combined.end(), layer[i + 1].begin(), layer[i + 1].end());
            next.push_back(double_sha256(combined));
        }
        layer = std::move(next);
    }

    return std::vector<uint8_t>(layer[0].begin(), layer[0].end());
}

// UndoTx

static uint64_t decompress_amount(uint64_t x)
{
    if (x == 0)
        return 0;
    x--;
    uint64_t e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9)
    {
        uint64_t d = x % 9 + 1;
        x /= 9;
        n = x * 10 + d;
    }
    else
    {
        n = x + 1;
    }
    while (e--)
        n *= 10;
    return n;
}

static std::vector<uint8_t> decompress_script(
    uint64_t type, const std::vector<uint8_t> &data, size_t &off)
{
    auto read_n = [&](size_t n) -> std::vector<uint8_t>
    {
        if (off + n > data.size())
            throw std::runtime_error("decompress_script: underflow");
        std::vector<uint8_t> out(data.begin() + off, data.begin() + off + n);
        off += n;
        return out;
    };

    std::vector<uint8_t> script;
    switch (type)
    {
    case 0:
    { // P2PKH
        auto h = read_n(20);
        script = {0x76, 0xa9, 0x14};
        script.insert(script.end(), h.begin(), h.end());
        script.push_back(0x88);
        script.push_back(0xac);
        break;
    }
    case 1:
    { // P2SH
        auto h = read_n(20);
        script = {0xa9, 0x14};
        script.insert(script.end(), h.begin(), h.end());
        script.push_back(0x87);
        break;
    }
    case 2:
    case 3:
    { // P2PK compressed
        auto x = read_n(32);
        script = {0x21, static_cast<uint8_t>(type)};
        script.insert(script.end(), x.begin(), x.end());
        script.push_back(0xac);
        break;
    }
    case 4:
    case 5:
    {
        // Read 32-byte X coordinate
        auto x = read_n(32);

        // Reconstruct compressed pubkey (33 bytes)
        unsigned char compressed[33];
        compressed[0] = static_cast<unsigned char>(type - 2); // 0x02 or 0x03
        std::memcpy(compressed + 1, x.data(), 32);

        // Parse using secp256k1
        secp256k1_pubkey pubkey;

        if (!secp256k1_ec_pubkey_parse(
                Secp256k1Context::instance(),
                &pubkey,
                compressed,
                33))
        {
            throw std::runtime_error("Invalid compressed pubkey in undo data");
        }

        // Serialize as uncompressed (65 bytes)
        unsigned char full[65];
        size_t full_len = 65;

        secp256k1_ec_pubkey_serialize(
            Secp256k1Context::instance(),
            full,
            &full_len,
            &pubkey,
            SECP256K1_EC_UNCOMPRESSED);

        if (full_len != 65)
            throw std::runtime_error("Unexpected pubkey length");

        // Build script:
        // OP_PUSH65 <65-byte pubkey> OP_CHECKSIG
        script.clear();
        script.push_back(0x41); // push 65 bytes
        script.insert(script.end(), full, full + 65);
        script.push_back(0xac); // OP_CHECKSIG

        break;
    }
    default:
    {
        if (type < 6)
            throw std::runtime_error("Invalid script type");

        script = read_n(static_cast<size_t>(type - 6));
        break;
    }
    }
    return script;
}

// Bitcoin Core CVarInt decoder (serialize.h).
// Used for all Coin fields in undo data -- DIFFERENT from CompactSize (read_varint).
// Each byte stores 7 bits of value; high bit = more bytes follow.
// On continuation, add 1 to de-bias (ensures unique encoding per value).
static uint64_t read_cvarint(const std::vector<uint8_t> &data, size_t &off)
{
    uint64_t n = 0;
    while (true)
    {
        if (off >= data.size())
            throw std::runtime_error("read_cvarint: underflow");
        uint8_t b = data[off++];
        n = (n << 7) | (b & 0x7F);
        if (b & 0x80)
            n += 1;
        else
            break;
    }
    return n;
}

UndoTx::UndoTx(std::vector<uint8_t> &data, size_t &off)
{
    uint64_t input_count = read_varint(data, off);
    spentOutputs.reserve(input_count);

    for (uint64_t i = 0; i < input_count; i++)
    {
        UndoCoin coin;

        uint64_t code = read_cvarint(data, off);
        coin.height = code >> 1;
        coin.isCoinbase = code & 1;

        if (coin.height > 0)
        {
            if (off >= data.size())
                throw std::runtime_error("dummy underflow");
            uint8_t dummy = data[off++];
            // should always be 0
        }
        // CompressedAmount (CVarInt)
        uint64_t compressed = read_cvarint(data, off);
        coin.value = decompress_amount(compressed);

        // CompressedScript (type CVarInt + data bytes)
        uint64_t script_type = read_cvarint(data, off);
        coin.scriptPubKey = decompress_script(script_type, data, off);

        spentOutputs.push_back(std::move(coin));
    }
}

// UndoBlock

UndoBlock::UndoBlock(std::vector<uint8_t> &raw)
{
    size_t off = 0;

    magic = read_uint32_le(raw, off);
    off += 4;

    undoPayloadSize = read_uint32_le(raw, off);
    off += 4;

    if (off + undoPayloadSize > raw.size())
        throw std::runtime_error("UndoBlock truncated");

    size_t payloadEnd = off + undoPayloadSize;

    txCount = read_varint(raw, off);

    transactions.reserve(txCount);

    for (uint64_t i = 0; i < txCount; ++i)
        transactions.emplace_back(raw, off);

    if (off != payloadEnd)
        throw std::runtime_error("UndoBlock payload size mismatch");

    // skip checksum (32 bytes), we dont need that ?
    // but we should not skip this ?
    off += 32;
}
