#include <iostream>
#include <fstream>
#include <filesystem>
#include "accounting.h"
#include "json_helper.h"
#include "block_parser.h"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static int run_tx_mode(const std::string &input_path)
{
    InputTxnWithPrevout in(input_path);
    Transaction tx(in.raw_tx_bytes);

    if (in.prevouts.size() != tx.inputs.size())
        throw std::runtime_error(
            "prevouts count (" + std::to_string(in.prevouts.size()) +
            ") != tx input count (" + std::to_string(tx.inputs.size()) + ")");

    TxnAnalyzer ta(tx, in.prevouts, in.network);

    nlohmann::ordered_json j = analyzed_txn_to_json(ta);

    // Ensure out/ exists
    fs::create_directories("out");

    std::string txid = ta.txid();
    std::string out_path = "out/" + txid + ".json";

    std::ofstream out(out_path);
    if (!out)
        throw std::runtime_error("Failed to write output file");

    out << j.dump(4);
    out.close();

    // Print to stdout (required for single-tx mode)
    std::cout << j.dump(4) << "\n";

    return 0;
}

static int run_block_mode(const std::string &blk_path,
                          const std::string &rev_path,
                          const std::string &xor_path)
{
    BlockParser parser(blk_path, rev_path, xor_path, "out");
    parser.run();
    return 0;
}

int main(int argc, char *argv[])
{
    try
    {
        if (argc == 5 && std::string(argv[1]) == "--block")
            return run_block_mode(argv[2], argv[3], argv[4]);

        if (argc == 2)
            return run_tx_mode(argv[1]);

        nlohmann::ordered_json err = {
            {"ok", false},
            {"error", {{"code", "INVALID_USAGE"}, {"message", "Usage: tx_tool <input.json> | tx_tool --block <blk.dat> <rev.dat> <xor.dat>"}}}};

        std::cout << err.dump(4) << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        nlohmann::ordered_json err = {
            {"ok", false},
            {"error", {{"code", "CLI_ERROR"}, {"message", e.what()}}}};

        std::cout << err.dump(4) << "\n";
        return 1;
    }
}
