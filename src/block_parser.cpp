#include "block_parser.h"
#include "block.h"
#include "accounting.h"
#include "json_helper.h"
#include "utilities.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// ---------------- DatFileReader ----------------

DatFileReader::DatFileReader(const std::string &path,
                             const std::vector<uint8_t> &xor_key)
    : stream_(path, std::ios::binary),
      xor_key_(xor_key),
      file_offset_(0)
{
    if (!stream_.is_open())
        throw std::runtime_error("Cannot open file: " + path);
}

bool DatFileReader::read_bytes(std::vector<uint8_t> &buf, size_t n)
{
    buf.resize(n);

    if (!stream_.read(reinterpret_cast<char *>(buf.data()), n))
        return false;

    if (!xor_key_.empty())
    {
        for (size_t i = 0; i < n; ++i)
            buf[i] ^= xor_key_[(file_offset_ + i) % xor_key_.size()];
    }

    file_offset_ += n;
    return true;
}

// ---------------- BlockParser ----------------

BlockParser::BlockParser(const std::string &blk_path,
                         const std::string &rev_path,
                         const std::string &xor_path,
                         const std::string &out_dir)
    : blk_path_(blk_path),
      rev_path_(rev_path),
      out_dir_(out_dir)
{
    xor_key_ = read_xor_key(xor_path);
    fs::create_directories(out_dir_);
}

size_t BlockParser::run()
{
    std::vector<uint8_t> blk_raw = read_file(blk_path_);
    std::vector<uint8_t> rev_raw = read_file(rev_path_);

    if (!xor_key_.empty()) {
        xor_decode(blk_raw, xor_key_);
        xor_decode(rev_raw, xor_key_);
    }

    size_t blk_off = 0;
    size_t rev_off = 0;

    while (blk_off < blk_raw.size() && rev_off < rev_raw.size())
    {
        // ---------------- EXTRACT BLOCK RECORD ----------------
        size_t blk_start = blk_off;

        if (blk_off + 8 > blk_raw.size())
            throw std::runtime_error("blk truncated");

        uint32_t blk_magic = read_uint32_le(blk_raw, blk_off);
        blk_off += 4;

        uint32_t blk_size = read_uint32_le(blk_raw, blk_off);
        blk_off += 4;

        size_t blk_total = 8 + blk_size;

        if (blk_start + blk_total > blk_raw.size())
            throw std::runtime_error("blk record overflow");

        std::vector<uint8_t> blk_record(
            blk_raw.begin() + blk_start,
            blk_raw.begin() + blk_start + blk_total
        );

        blk_off = blk_start + blk_total;

        // ---------------- EXTRACT UNDO RECORD ----------------
        size_t rev_start = rev_off;

        if (rev_off + 8 > rev_raw.size())
            throw std::runtime_error("rev truncated");

        uint32_t rev_magic = read_uint32_le(rev_raw, rev_off);
        rev_off += 4;

        uint32_t rev_size = read_uint32_le(rev_raw, rev_off);
        rev_off += 4;

        size_t rev_total = 8 + rev_size + 32; // + checksum

        if (rev_start + rev_total > rev_raw.size())
            throw std::runtime_error("rev record overflow");

        std::vector<uint8_t> rev_record(
            rev_raw.begin() + rev_start,
            rev_raw.begin() + rev_start + rev_total
        );

        rev_off = rev_start + rev_total;

        // ---------------- PARSE ----------------
        Block block(blk_record);
        UndoBlock undo(rev_record);

        std::cerr << "blk tx=" << block.getTransactionCount()
                  << " undo tx=" << undo.getTxCount() << "\n";

        // ---------------- VALID ALIGNMENT ----------------
        if (undo.getTxCount() == block.getTransactionCount() - 1)
        {
            std::cerr << "✔ Found matching block/undo pair\n";

            BlockAnalyzer analyzer(block, undo, "mainnet");

            std::string out_path =
                out_dir_ + "/" +
                analyzer.block_header.block_hash + ".json";

            std::ofstream out(out_path);
            if (!out)
                throw std::runtime_error("Cannot open output");

            out << block_to_json(analyzer).dump(4) << "\n";

            return 1;
        }

        // Otherwise continue to next record
    }

    throw std::runtime_error("No matching block/undo pair found");
}