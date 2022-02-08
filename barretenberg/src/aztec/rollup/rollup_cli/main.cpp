#include <sstream>
#include <iostream>

#include "../proofs/account/compute_circuit_data.hpp"
#include "../proofs/join_split/compute_circuit_data.hpp"
#include "../proofs/claim/get_circuit_data.hpp"
#include "../proofs/claim/verify.hpp"
#include "../proofs/rollup/index.hpp"
#include "../proofs/root_rollup/index.hpp"
#include "../proofs/root_verifier/index.hpp"
#include <common/timer.hpp>
#include <common/container.hpp>
#include <common/map.hpp>
#include <plonk/composer/turbo/compute_verification_key.hpp>
#include <plonk/proof_system/proving_key/proving_key.hpp>
#include <plonk/proof_system/verification_key/verification_key.hpp>

using namespace ::rollup::proofs;
using namespace plonk::stdlib::merkle_tree;
using namespace serialize;
namespace tx_rollup = ::rollup::proofs::rollup;

namespace {
std::string data_path;
// True if rollup circuit data (proving and verification keys) are to be persisted to disk.
// We likely don't have enough memory to hold all keys in memory, and loading keys from disk is faster.
bool persist;
// In mock mode, mock proofs (expected public inputs, but no constraints) are generated.
bool mock_proofs;
std::shared_ptr<waffle::DynamicFileReferenceStringFactory> crs;
join_split::circuit_data js_cd;
account::circuit_data account_cd;
tx_rollup::circuit_data tx_rollup_cd;
root_rollup::circuit_data root_rollup_cd;
claim::circuit_data claim_cd;
std::vector<uint32_t> valid_outer_sizes;
root_verifier::circuit_data root_verifier_cd;
} // namespace

bool create_tx_rollup()
{
    uint32_t num_txs;
    read(std::cin, num_txs);

    if (!tx_rollup_cd.proving_key || tx_rollup_cd.num_txs != num_txs) {
        tx_rollup_cd.proving_key.reset();
        tx_rollup_cd = tx_rollup::get_circuit_data(
            num_txs, js_cd, account_cd, claim_cd, crs, data_path, true, persist, persist, true, true, mock_proofs);
    }

    tx_rollup::rollup_tx rollup;
    std::cerr << "Reading tx rollup..." << std::endl;
    read(std::cin, rollup);
    std::cerr << "Received tx rollup with " << rollup.num_txs << " txs." << std::endl;

    auto result = verify(rollup, tx_rollup_cd);

    write(std::cout, result.proof_data);
    write(std::cout, result.verified);
    std::cout << std::flush;

    return result.verified;
}

bool create_root_rollup()
{
    uint32_t num_txs;
    uint32_t num_proofs;
    read(std::cin, num_txs);
    read(std::cin, num_proofs);

    if (!tx_rollup_cd.proving_key || tx_rollup_cd.num_txs != num_txs) {
        tx_rollup_cd.proving_key.reset();
        tx_rollup_cd = tx_rollup::get_circuit_data(
            num_txs, js_cd, account_cd, claim_cd, crs, data_path, true, persist, persist, true, true, mock_proofs);
    }

    if (!root_rollup_cd.proving_key || root_rollup_cd.num_inner_rollups != num_proofs) {
        root_rollup_cd.proving_key.reset();
        root_rollup_cd = root_rollup::get_circuit_data(
            num_proofs, tx_rollup_cd, crs, data_path, true, persist, persist, true, true, mock_proofs);
    }

    root_rollup::root_rollup_tx root_rollup;
    std::cerr << "Reading root rollup..." << std::endl;
    read(std::cin, root_rollup);
    std::cerr << "Received root rollup with " << root_rollup.rollups.size() << " rollups." << std::endl;

    auto result = verify(root_rollup, root_rollup_cd);

    root_rollup::root_rollup_broadcast_data broadcast_data(result.broadcast_data);
    auto buf = join({ to_buffer(broadcast_data), result.proof_data });

    write(std::cout, buf);
    write(std::cout, result.verified);
    std::cout << std::flush;

    return result.verified;
}

bool create_claim()
{
    claim::claim_tx claim_tx;
    std::cerr << "Reading claim tx..." << std::endl;
    read(std::cin, claim_tx);

    auto result = verify(claim_tx, claim_cd);

    write(std::cout, result.proof_data);
    write(std::cout, result.verified);
    std::cout << std::flush;

    return result.verified;
}

bool create_root_verifier()
{
    // TODO: Not needed. We can assume prior call to create_rollup_tx will have inited the tx_rollup_cd.
    uint32_t num_txs;
    read(std::cin, num_txs);

    // We do however, currently need to know the num_proofs, in order to correctly be able to slice off broadcast data.
    uint32_t num_proofs;
    read(std::cin, num_proofs);

    // On first run of create_root_verifier, build list of valid verification keys.
    if (!root_verifier_cd.proving_key) {
        for (size_t size : valid_outer_sizes) {
            if (root_rollup_cd.proving_key && root_rollup_cd.num_inner_rollups == size) {
                root_verifier_cd.valid_vks.emplace_back(root_rollup_cd.verification_key);
            } else {
                root_verifier_cd.valid_vks.emplace_back(
                    root_rollup::get_circuit_data(
                        size, tx_rollup_cd, crs, data_path, true, persist, persist, true, true, mock_proofs)
                        .verification_key);
            }
        }
        root_verifier_cd = root_verifier::get_circuit_data(root_rollup_cd,
                                                           crs,
                                                           root_verifier_cd.valid_vks,
                                                           data_path,
                                                           true,
                                                           persist,
                                                           persist,
                                                           true,
                                                           true,
                                                           mock_proofs);
    }

    std::vector<uint8_t> root_rollup_proof_buf;
    std::cerr << "Reading root verifier tx..." << std::endl;
    read(std::cin, root_rollup_proof_buf);

    auto rollup_size = num_proofs * tx_rollup_cd.rollup_size;
    auto tx = root_verifier::create_root_verifier_tx(root_rollup_proof_buf, rollup_size);

    std::cerr << "Received root verifier tx... (circuit valid sizes: "
              << map(valid_outer_sizes, [](size_t s) { return s * tx_rollup_cd.rollup_size; })
              << ", proof size: " << rollup_size << ")" << std::endl;

    auto result = verify(tx, root_verifier_cd, root_rollup_cd);

    result.proof_data = join({ tx.broadcast_data, result.proof_data });
    write(std::cout, result.proof_data);
    write(std::cout, (uint8_t)result.verified);
    std::cout << std::flush;

    return result.verified;
}

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);
    const std::string srs_path = (args.size() > 1) ? args[1] : "../srs_db/ignition";
    data_path = (args.size() > 2) ? args[2] : "./data";
    std::string outers = args.size() > 3 ? args[3] : "1";
    persist = args.size() > 4 ? args[4] == "true" : true;
    mock_proofs = args.size() > 5 ? args[5] == "true" : false;

    std::istringstream outer_stream(outers);
    std::string outer_size;
    while (std::getline(outer_stream, outer_size, ',')) {
        valid_outer_sizes.emplace_back(std::stoul(outer_size));
    };

    if (mock_proofs) {
        info("Running in mock proof mode. Mock proofs will be generated!");
    }

    info("Loading crs...");
    crs = std::make_shared<waffle::DynamicFileReferenceStringFactory>(srs_path);

    account_cd = account::get_circuit_data(crs, mock_proofs);
    js_cd = join_split::get_circuit_data(crs, mock_proofs);
    claim_cd = claim::get_circuit_data(crs, mock_proofs);

    info("Reading rollups from standard input...");
    while (true) {
        if (!std::cin.good() || std::cin.peek() == std::char_traits<char>::eof()) {
            break;
        }

        uint32_t proof_id;
        read(std::cin, proof_id);

        switch (proof_id) {
        case 0: {
            create_tx_rollup();
            break;
        }
        case 1: {
            create_root_rollup();
            break;
        }
        case 2: {
            create_claim();
            break;
        }
        case 3: {
            create_root_verifier();
            break;
        }
        case 100: {
            // Convert to buffer first, so when we call write we prefix the buffer length.
            std::cerr << "Serving join split vk..." << std::endl;
            write(std::cout, to_buffer(*js_cd.verification_key));
            break;
        }
        case 101: {
            std::cerr << "Serving account vk..." << std::endl;
            write(std::cout, to_buffer(*account_cd.verification_key));
            break;
        }
        case 666: {
            // Ping... Pong... Used for learning when rollup_cli is responsive.
            std::cerr << "Ping... Pong..." << std::endl;
            serialize::write(std::cout, true);
            break;
        }
        default: {
            std::cerr << "Unknown command: " << proof_id << std::endl;
            break;
        }
        }
    }

    return 0;
}