#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>
#include "utilities.h"

enum class LockTimeType {
	UNIX_TIMESTAMP,
	BLOCK_HEIGHT,
	NONE
};

enum class RelativeLockTimeType {
	UNIX_TIMESTAMP,
	BLOCK_HEIGHT,
};

struct RelativeLocktimeInfo {
	bool enabled;
	// Type of lock
	RelativeLockTimeType type;
	// The value is 16 bits
	// in case the RLT is timebased we need to multiply it by 512 to get the value in sec
	// Than we cannot store it in uint16_t we need a 64 bit int
	uint16_t value;
};

class TxIn {

	public:
		std::array<uint8_t, 32> prevTxId;
		uint32_t vout;
		std::vector<uint8_t> scriptSig;
		uint32_t sequence;

		// only present if isSegwit is true
		std::vector<std::vector<uint8_t>> witness;

		// FXNS FOR sequence field
		// these are specific to input

		// Returns true if relative locktime is enabled
		// BIP 68 (https://github.com/bitcoin/bips/blob/master/bip-0068.mediawiki)
		bool RLT_enabled() const;

		// Returns a object with all relative locktime data calculated
		RelativeLocktimeInfo get_rlt_info() const;
};

class TxOut {
	public:
		// Amount is in Satoshis, 1 Satoshis = 0.00000001 BTC
		uint64_t amount;
		std::vector<uint8_t> scriptPubKey;
};

class Transaction {
	
	public:
		uint32_t version;

		std::vector<TxOut> outputs;
		std::vector<TxIn> inputs;

		uint32_t locktime;

		// default constructor
		Transaction();

		// constructor that build Txn using the raw transaction array of bytes 
		Transaction(const std::vector<uint8_t>& raw_txn_hex_bytes);

		// Constructor used by block
		Transaction(const std::vector<uint8_t> &raw, size_t &off);

		// Getter for the private variable
		bool is_segwit() const;

		// calculate and return size of txn in bytes
		size_t get_size_bytes() const;

		// calculate and return weight of txn
		// follows BIP 141 (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#transaction-size-calculations)
		size_t get_weight() const;

		// calculate and return vbytes of txn
		// follows BIP 141 (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#transaction-size-calculations)
		size_t get_vbytes() const;


		// LOCKTIME PROCESSOR FXNS

		// returns the type of lock 
		// lock_time_type is a enum , look above
		LockTimeType get_locktime_type() const;

		// True if txn is final, false otherwise
		// We can use the locktime_enabled() but it's good to have this
		bool is_final();

		// Returns true if locktime is enabled , false otherwise
		bool locktime_enabled();

		// Returns true if replace by fee is enabled
		// introduced in BIP 125 (https://github.com/bitcoin/bips/blob/master/bip-0125.mediawiki)
		bool RBF_enabled() const;

        // Returns the precomputed TxID (double-SHA256 of legacy serialization, reversed)
        std::array<uint8_t, 32> get_txid() const;

        // Returns the precomputed wTxID (double-SHA256 of witness serialization, reversed)
        // For non-segwit transactions wTxID == TxID per BIP 141
        std::array<uint8_t, 32> get_wtxid() const;

	private:
		bool isSegwit;

		// Precomputed hash caches, calculated once in the constructor
        std::array<uint8_t, 32> TxIdHash;
        std::array<uint8_t, 32> WTxIdHash;

        // Will serialize the Txn to bytes, no witness data
        // used internally to compute TxIdHash
        std::vector<uint8_t> serialize_legacy() const;

        // Will serialize the Txn to bytes, with witness data
        // used internally to compute WTxIdHash
        std::vector<uint8_t> serialize_with_witness() const;

};

// Helpers for enum to string conversion
std::string locktime_type_str(LockTimeType t);

#endif
