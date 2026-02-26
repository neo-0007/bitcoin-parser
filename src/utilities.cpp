#include "utilities.h"

// Helper fxn to convert a hex char to its decimal value
static uint8_t hex_char_to_int_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument("hex_char_to_int_val: invalid hex character");
}

// Takes a hex string as input and returns a byte vector
std::vector<uint8_t> hex_to_bytes(const std::string& hex)
{
    if (hex.size() % 2 != 0)
        throw std::invalid_argument("hex string must have even length");

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2)
    {
        uint8_t high_part = hex_char_to_int_val(hex[i]);
        uint8_t low_part  = hex_char_to_int_val(hex[i + 1]);

        uint8_t byte = (high_part << 4) | low_part;
        bytes.push_back(byte);
    }

    return bytes;
}

// Takes a byte vector as input and returns the equivalent hex string
std::string bytes_to_hex(const std::vector<uint8_t>& bytes)
{
    const char* hex_chars = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);

    for (uint8_t b : bytes)
    {
        result.push_back(hex_chars[b >> 4]);
        result.push_back(hex_chars[b & 0x0F]);
    }

    return result;
}


// Takes in a 32 bytes array and converts it to it's equivalent string rep.
std::string bytes_to_hex(const std::array<uint8_t, 32>& bytes)
{
    const char* hex_chars = "0123456789abcdef";
    std::string result(bytes.size() * 2, '\0');

    for (size_t i = 0; i < bytes.size(); ++i)
    {
        result[2 * i]     = hex_chars[bytes[i] >> 4];
        result[2 * i + 1] = hex_chars[bytes[i] & 0x0F];
    }

    return result;
}


// Reads 32 bit int from a data starting from a given offset value
// expects the data is in LE format
uint32_t read_uint32_le(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + 4 > data.size())
        throw std::out_of_range("read_uint32_le: not enough bytes");

    uint32_t value = 0;
    for (size_t i = 0; i < 4; i++)
        value |= static_cast<uint32_t>(data[offset + i]) << (i * 8);

    return value;
}

// Reads 64 bit int from a data starting from a given offset value
// expects the data is in LE format
uint64_t read_uint64_le(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + 8 > data.size())
        throw std::out_of_range("read_uint64_le: not enough bytes");

    uint64_t value = 0;
    for (size_t i = 0; i < 8; i++)
        value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);

    return value;
}

// Writes a 32bit value in LE format in the given buffer
void write_uint32_le(std::vector<uint8_t>& buffer, uint32_t value)
{
    for (size_t i = 0; i < 4; i++)
        buffer.push_back(static_cast<uint8_t>(value >> (i * 8)));
}

// Writes a 64bit value in LE format in the given buffer
void write_uint64_le(std::vector<uint8_t>& buffer, uint64_t value)
{
    for (size_t i = 0; i < 8; i++)
        buffer.push_back(static_cast<uint8_t>(value >> (i * 8)));
}

// VarInt (CompactSize) helpers
// reference: https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
// Data Format
//   < 0xFD        -> 1 byte  (value as-is)
//   0xFD + 2 LE   -> up to 0xFFFF
//   0xFE + 4 LE   -> up to 0xFFFFFFFF
//   0xFF + 8 LE   -> up to 0xFFFFFFFFFFFFFFFF


// HELPER , like the above fxns read a 16 bit int after a offset in given data
uint16_t read_uint16_le(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + 2 > data.size())
        throw std::out_of_range("read_uint16_le: not enough bytes");

    return  static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
}


// Reads a variable integer / CompactSize in data after a given offset
uint64_t read_varint(const std::vector<uint8_t>& data, size_t& offset)
{
    if (offset >= data.size())
        throw std::out_of_range("read_varint: no data");

    uint8_t prefix = data[offset++];

    if (prefix < 0xFD)
    {
        return prefix;
    }
    else if (prefix == 0xFD)
    {
        uint64_t value = read_uint16_le(data, offset);
        offset += 2;
        return value;
    }
    else if (prefix == 0xFE)
    {
        uint64_t value = read_uint32_le(data, offset);
        offset += 4;
        return value;
    }
    else // 0xFF
    {
        uint64_t value = read_uint64_le(data, offset);
        offset += 8;
        return value;
    }
}

// Writes a variable integer / CompactSize value in a given buffer
void write_varint(std::vector<uint8_t>& buffer, uint64_t value)
{
    if (value < 0xFD)
    {
        buffer.push_back(static_cast<uint8_t>(value));
    }
    else if (value <= 0xFFFF)
    {
        buffer.push_back(0xFD);

        for (int i = 0; i < 2; ++i)
            buffer.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
    else if (value <= 0xFFFFFFFF)
    {
        buffer.push_back(0xFE);

        for (int i = 0; i < 4; ++i)
            buffer.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
    else
    {
        buffer.push_back(0xFF);

        for (int i = 0; i < 8; ++i)
            buffer.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}


// HASHING
// Uses the OpenSSL libarary implementations

// returns the sha256 digest
std::array<uint8_t, 32> sha256(const std::vector<uint8_t>& data)
{
    std::array<uint8_t, 32> hash;
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

// double sha256 , also called HASH256
std::array<uint8_t, 32> double_sha256(const std::vector<uint8_t>& data)
{
    std::array<uint8_t, 32> first_pass = sha256(data);
    std::vector<uint8_t> first_pass_vec(first_pass.begin(), first_pass.end());
    return sha256(first_pass_vec);
}


// HANDY helper to reverse a 32 bye array
std::array<uint8_t, 32> reverse_32(const std::array<uint8_t, 32>& data)
{
    std::array<uint8_t, 32> reversed;
    for (size_t i = 0; i < 32; i++)
        reversed[i] = data[31 - i];
    return reversed;
}

std::string byte_to_hex(uint8_t b)
{
    const char* hex_chars = "0123456789ABCDEF";
    std::string s;
    s += hex_chars[(b >> 4) & 0x0F];
    s += hex_chars[b & 0x0F];
    return s;
}

std::array<uint8_t, 20> ripemd160(const std::vector<uint8_t>& data)
{
    std::array<uint8_t, 20> hash;
    RIPEMD160(data.data(), data.size(), hash.data());
    return hash;
}

std::array<uint8_t, 20> hash160(const std::vector<uint8_t>& data)
{
    auto sha = sha256(data);
    std::vector<uint8_t> tmp(sha.begin(), sha.end());
    return ripemd160(tmp);
}


static bool openssl_sha256_adapter(void *digest,
                                   const void *data,
                                   size_t datasz)
{
    std::vector<uint8_t> input(
        (const uint8_t*)data,
        (const uint8_t*)data + datasz);

    auto hash = sha256(input);
    memcpy(digest, hash.data(), 32);
    return true;
}

static struct Base58Init {
    Base58Init() {
        b58_sha256_impl = openssl_sha256_adapter;
    }
} base58_init;

std::string encode_p2pkh_address(const std::vector<uint8_t>& hash20)
{
    if (hash20.size() != 20)
        throw std::runtime_error("Invalid P2PKH hash size");

    size_t outsz = 64;
    char out[64];

    if (!b58check_enc(out, &outsz, 0x00,
                      hash20.data(),
                      hash20.size()))
        throw std::runtime_error("Base58Check encoding failed");

    return std::string(out);
}

std::string encode_p2sh_address(const std::vector<uint8_t>& hash20)
{
    if (hash20.size() != 20)
        throw std::runtime_error("Invalid P2SH hash size");

    size_t outsz = 64;
    char out[64];

    if (!b58check_enc(out, &outsz, 0x05,
                      hash20.data(),
                      hash20.size()))
        throw std::runtime_error("Base58Check encoding failed");

    return std::string(out);
}

// utilities.cpp  –  replace encode_segwit_address

std::string encode_segwit_address(uint8_t version,
                                  const std::vector<uint8_t>& program)
{
    const char* hrp   = "bc";   // mainnet; swap to "tb" for testnet
    size_t      n_hrp = 2;

    // bech32_address_encode never writes more than 91 chars + null terminator
    char address[92] = {};

    ssize_t ret = bech32_address_encode(
        address,
        sizeof(address),
        program.data(),
        program.size(),
        hrp,
        n_hrp,
        static_cast<unsigned>(version)
    );

    if (ret < 0)
        throw std::runtime_error(
            "bech32_address_encode failed: error code " + std::to_string(ret));

    return std::string(address, static_cast<size_t>(ret));
}

bool is_valid_utf8(const std::vector<uint8_t>& data)
{
    size_t i = 0;

    while (i < data.size())
    {
        uint8_t byte = data[i];

        // 1-byte ASCII
        if (byte <= 0x7F)
        {
            i++;
        }
        // 2-byte sequence
        else if ((byte >> 5) == 0x6)
        {
            if (i + 1 >= data.size()) return false;

            if ((data[i + 1] >> 6) != 0x2) return false;

            i += 2;
        }
        // 3-byte sequence
        else if ((byte >> 4) == 0xE)
        {
            if (i + 2 >= data.size()) return false;

            if ((data[i + 1] >> 6) != 0x2) return false;
            if ((data[i + 2] >> 6) != 0x2) return false;

            i += 3;
        }
        // 4-byte sequence
        else if ((byte >> 3) == 0x1E)
        {
            if (i + 3 >= data.size()) return false;

            if ((data[i + 1] >> 6) != 0x2) return false;
            if ((data[i + 2] >> 6) != 0x2) return false;
            if ((data[i + 3] >> 6) != 0x2) return false;

            i += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}

std::vector<uint8_t> read_file(const std::string& path, size_t byte_count)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("read_file: cannot open: " + path);
    std::vector<uint8_t> buf(byte_count);
    if (!f.read(reinterpret_cast<char*>(buf.data()), byte_count))
        throw std::runtime_error("read_file: read error: " + path);
    return buf;
}

std::vector<uint8_t> read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        throw std::runtime_error("read_file: cannot open: " + path);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size))
        throw std::runtime_error("read_file: read error: " + path);
    return buf;
}

std::vector<uint8_t> read_xor_key(const std::string& xor_dat_path)
{
    std::vector<uint8_t> raw = read_file(xor_dat_path);
    if (raw.empty()) return {};
    bool all_zero = std::all_of(raw.begin(), raw.end(),
                                [](uint8_t b){ return b == 0; });
    return all_zero ? std::vector<uint8_t>{} : raw;
}

void xor_decode(std::vector<uint8_t>& data, const std::vector<uint8_t>& key)
{
    if (key.empty()) return;
    for (size_t i = 0; i < data.size(); ++i)
        data[i] ^= key[i % key.size()];
}
