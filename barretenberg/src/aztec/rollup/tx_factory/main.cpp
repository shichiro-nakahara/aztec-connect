#include "../client_proofs/join_split/join_split.hpp"
#include "../client_proofs/join_split/join_split_tx.hpp"
#include "../rollup_proofs/rollup_tx.hpp"
#include "../rollup_proofs/create_noop_join_split_proof.hpp"
#include "../tx/user_context.hpp"
#include "../client_proofs/join_split/sign_notes.hpp"
#include <stdlib/merkle_tree/leveldb_store.hpp>
#include <stdlib/merkle_tree/leveldb_tree.hpp>
#include <stdlib/types/turbo.hpp>
#include <common/streams.hpp>
#include <iostream>

using namespace rollup::rollup_proofs;
using namespace rollup::client_proofs::join_split;
using namespace plonk::stdlib::types::turbo;
using namespace plonk::stdlib::merkle_tree;

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 2) {
        std::cout << "usage: " << args[0] << " <num_txs>" << std::endl;
        return -1;
    }

    const uint32_t num_txs = static_cast<uint32_t>(std::stoul(args[1]));
    std::cerr << "Generating rollup with " << num_txs << " txs..." << std::endl;

    // if (args.size() < 8) {
    //     std::cout << "usage: " << argv[0]
    //               << " join-split <first note index to join> <second note index to join> <first input note value>"
    //                  " <second input note value> <first output note value> <second output note value>"
    //                  " [public input] [public output]"
    //               << std::endl;
    //     return -1;
    // }

    auto proof = create_noop_join_split_proof();

    rollup_tx rollup;
    rollup.rollup_id = 0;
    rollup.num_txs = num_txs;
    rollup.proof_lengths = static_cast<uint32_t>(proof.proof_data.size());
    rollup.txs = std::vector(num_txs, proof.proof_data);
    write(std::cout, rollup);

    /*
    if (args[1] == "server-join-split") {
        if (args.size() < 8) {
            std::cout
                << "usage: " << argv[0]
                << " server-join-split <first note index to join> <second note index to join> <first input note value>"
                   " <second input note value> <first output note value> <second output note value>"
                   " [public input] [public output] [json | binary]"
                << std::endl;
            return -1;
        }

        auto tx = create_join_split_tx({ args.begin() + 2, args.end() }, user);
        if (args.size() < 11 || args[10] == "binary") {
            write(std::cout, hton(tx));
        } else {
            write_json(std::cout, tx);
            std::cout << std::endl;
        }
    } else if (args[1] == "join-split-single") {
        if (args.size() < 8) {
            std::cout << "usage: " << argv[0]
                      << " join-split <first note index to join> <second note index to join> <first input note value>"
                         " <second input note value> <first output note value> <second output note value>"
                         " [public input] [public output]"
                      << std::endl;
            return -1;
        }

        batch_tx batch;
        batch.batch_num = 0;
        batch.txs.push_back(create_join_split_tx({ args.begin() + 2, args.end() }, user));
        std::cerr << batch.txs[0] << std::endl;
        write(std::cout, batch);
    } else if (args.size() > 1 && args[1] == "join-split-auto") {
        bool valid_args = args.size() == 3;
        valid_args |= args.size() == 4 && (args[3] == "binary" || args[3] == "json");
        if (!valid_args) {
            std::cout << "usage: " << argv[0] << " join-split-auto <num transactions> [json | binary]" << std::endl;
            return -1;
        }

        size_t num_txs = (size_t)atoi(args[2].c_str());
        batch_tx batch;
        batch.batch_num = 0;
        batch.txs.reserve(num_txs);
        batch.txs.push_back(create_join_split_tx({ "0", "0", "-", "-", "50", "50", "100", "0" }, user));
        for (size_t i = 0; i < num_txs - 1; ++i) {
            auto index1 = std::to_string(i * 2);
            auto index2 = std::to_string(i * 2 + 1);
            batch.txs.push_back(create_join_split_tx({ index1, index2, "50", "50", "50", "50", "0", "0" }, user));
        }

        auto format = args.size() == 4 ? args[3] : "binary";

        if (format == "binary") {
            write(std::cout, hton(batch));
        } else {
            write_json(std::cout, batch);
        }
    } else {
        std::cout << "usage: " << args[0] << " [join-split] [join-split-auto ...>]" << std::endl;
        return -1;
    }
    */

    return 0;
}
