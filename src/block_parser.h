#ifndef BLOCK_PARSER_H
#define BLOCK_PARSER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

class DatFileReader
{
public:
    DatFileReader(const std::string &path,
                  const std::vector<uint8_t> &xor_key);

    bool read_bytes(std::vector<uint8_t> &buf, size_t n);

private:
    std::ifstream stream_;
    std::vector<uint8_t> xor_key_;
    uint64_t file_offset_ = 0;
};

class BlockParser
{
public:
    BlockParser(const std::string &blk_path,
                const std::string &rev_path,
                const std::string &xor_path,
                const std::string &out_dir = "out");

    size_t run();

private:
    std::string blk_path_;
    std::string rev_path_;
    std::string out_dir_;
    std::vector<uint8_t> xor_key_;
};

#endif