#ifndef BLOCK_H
#define BLOCK_H

#include "transaction.h"
#include "utilities.h"
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <stdexcept>
#include <algorithm>

// Block header data structure
class BlockHeader
{
private:
    uint32_t version;
    std::array<uint8_t, 32> prevBlock;
    std::array<uint8_t, 32> merkleRoot;
    uint32_t timestamp, bits, nonce;

    // This is block level but since calculated from blockheader i kept it here
    std::array<uint8_t, 32> blockHash;

public:
    // Constructor that takes in the 80 bytes block header in byte array
    BlockHeader(std::array<uint8_t, 80> &blk_header_hex_bytes);
    // Default constructor
    BlockHeader();

    // Getters for private variables
    int32_t getVersion() const;
    std::array<uint8_t, 32> getPreviousBlock() const;
    std::array<uint8_t, 32> getMerkleRoot() const;
    int32_t getTimestamp() const;
    int32_t getBits() const;
    int32_t getNonce() const;

    std::array<uint8_t, 32> getBlockHash() const;
    std::string getHashStr() const;

protected:
    void calcBlockHash();
};

// Block data structure
class Block
{
private:
    uint32_t magic, blockSize;
    BlockHeader blockHeader;
    uint64_t txnCounter;
    std::vector<Transaction> txs;

public:
    // Constructor that takes in a single block hex bytes and build the block data structure
    // blk_hex_bytes : [magic bytes] [payload size] [payload]
    Block(std::vector<uint8_t> &blk_hex_bytes);

    // getters for pvt variables
    uint32_t getMagicNumber() const;
    uint32_t getSize() const;
    BlockHeader getHeader() const;
    uint32_t getTransactionCount() const;

    uint64_t getOutputsValue() const;

    const std::vector<Transaction> &getTransactions() const;

    std::vector<uint8_t> calcMerkleRoot() const;
};

// Data structures for rev files

// Prevout structure
// Data structures for rev files
struct UndoCoin {
    uint32_t height;
    bool isCoinbase;
    uint64_t value;
    std::vector<uint8_t> scriptPubKey;
};

class UndoTx
{
private:
    uint64_t inputCount; // CompactSize
    std::vector<UndoCoin> spentOutputs;

public:
    UndoTx(std::vector<uint8_t> &data, size_t &offset);

    const std::vector<UndoCoin>& getInputs() const {
        return spentOutputs;
    }

    uint64_t getInputCount() const {
        return inputCount;
    }
};

class UndoBlock
{
private:
    uint32_t magic;
    uint32_t undoPayloadSize;
    uint64_t txCount;  // number of non-coinbase txs

    std::vector<UndoTx> transactions;

public:
    UndoBlock(std::vector<uint8_t> &bytes);

    const std::vector<UndoTx>& getTransactions() const {
        return transactions;
    }

    uint64_t getTxCount() const {
        return txCount;
    }
};

#endif // BLOCK_H