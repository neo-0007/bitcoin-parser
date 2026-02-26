#ifndef UTILITIES_H
#define UTILITIES_H

#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <cstring>
#include <secp256k1.h>
#include "./external/bech32.h"
extern "C" {
    #include "./external/libbase58.h"
}

// Takes a hex string as input and returns a byte vector
std::vector<uint8_t> hex_to_bytes(const std::string& hex);

// Takes a byte vector as input and returns the equivalent hex string
std::string bytes_to_hex(const std::vector<uint8_t>& bytes);

// Takes in a 32 bytes array and converts it to it's equivalent string rep.
std::string bytes_to_hex(const std::array<uint8_t, 32>& bytes);


// Reads 32 bit int from a data starting from a given offset value
// expects the data is in LE format
uint32_t read_uint32_le(const std::vector<uint8_t>& data, size_t offset);

// Reads 64 bit int from a data starting from a given offset value
// expects the data is in LE format
uint64_t read_uint64_le(const std::vector<uint8_t>& data, size_t offset);

// Writes a 32bit value in LE format in the given buffer
void write_uint32_le(std::vector<uint8_t>& buffer, uint32_t value);

// Writes a 64bit value in LE format in the given buffer
void write_uint64_le(std::vector<uint8_t>& buffer, uint64_t value);

// HELPER , like the above fxns read a 16 bit int after a offset in given data
uint16_t read_uint16_le(const std::vector<uint8_t>& data, size_t offset);

// VarInt (CompactSize) helpers
// reference: https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
// Data Format
//   < 0xFD        -> 1 byte  (value as-is)
//   0xFD + 2 LE   -> up to 0xFFFF
//   0xFE + 4 LE   -> up to 0xFFFFFFFF
//   0xFF + 8 LE   -> up to 0xFFFFFFFFFFFFFFFF

// Reads a variable integer / CompactSize in data after a given offset
uint64_t read_varint(const std::vector<uint8_t>& data, size_t& offset);

// Writes a variable integer / CompactSize value in a given buffer
void write_varint(std::vector<uint8_t>& buffer, uint64_t value);


// HASHING
// Uses the OpenSSL libarary implementations

// returns the sha256 digest
std::array<uint8_t, 32> sha256(const std::vector<uint8_t>& data);

// double sha256 , also called HASH256
std::array<uint8_t, 32> double_sha256(const std::vector<uint8_t>& data);


// HANDY helper to reverse a 32 bye array
std::array<uint8_t, 32> reverse_32(const std::array<uint8_t, 32>& data);

std::string byte_to_hex(uint8_t b);

std::array<uint8_t, 20> ripemd160(const std::vector<uint8_t>& data);
std::array<uint8_t, 20> hash160(const std::vector<uint8_t>& data);

std::string encode_p2pkh_address(const std::vector<uint8_t>& hash20);
std::string encode_p2sh_address(const std::vector<uint8_t>& hash20);
std::string encode_segwit_address(uint8_t version,
                                  const std::vector<uint8_t>& program);

bool is_valid_utf8(const std::vector<uint8_t>& data);

// file reader , reads byte_count number of bytes from a .dat file
// returns a byte array 
std::vector<uint8_t> read_file(const std::string& path, size_t byte_count);

// Reads byte_count bytes from path starting at file_offset
// (existing signature you already have)
std::vector<uint8_t> read_file(const std::string& path, size_t byte_count);

// Reads entire file into a byte vector
std::vector<uint8_t> read_file(const std::string& path);

// Reads XOR key from xor.dat (returns empty if all-zero)
std::vector<uint8_t> read_xor_key(const std::string& xor_dat_path);

// XOR-decodes buffer in-place using rolling key. No-op if key empty.
void xor_decode(std::vector<uint8_t>& data, const std::vector<uint8_t>& key);

class Secp256k1Context {
public:
    // Get global shared context
    static secp256k1_context* instance() {
        static Secp256k1Context ctx;  // Thread-safe since C++11
        return ctx.m_ctx;
    }

    // Delete copy/move
    Secp256k1Context(const Secp256k1Context&) = delete;
    Secp256k1Context& operator=(const Secp256k1Context&) = delete;
    Secp256k1Context(Secp256k1Context&&) = delete;
    Secp256k1Context& operator=(Secp256k1Context&&) = delete;

private:
    secp256k1_context* m_ctx;

    Secp256k1Context() {
        m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
        if (!m_ctx) {
            throw std::runtime_error("Failed to create secp256k1 context");
        }
    }

    ~Secp256k1Context() {
        if (m_ctx) {
            secp256k1_context_destroy(m_ctx);
        }
    }
};

#endif // UTILITIES_H
