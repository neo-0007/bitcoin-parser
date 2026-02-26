#include "accounting.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::ordered_json;

// Converts the Analyzed Txn that has all the fields we need to its json equivalent
nlohmann::ordered_json analyzed_txn_to_json(const TxnAnalyzer& txn);

// Takes in the json input and returns us the Input Txn with prevout
InputTxnWithPrevout json_to_input_txn_with_prevout(const nlohmann::json &j);

// Make json object from filepath
nlohmann::json get_json(std::string filepath);

nlohmann::ordered_json block_to_json(const BlockAnalyzer &ba);
