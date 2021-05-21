#include <crypto/sha256/sha256.hpp>
#include "../../fixtures/user_context.hpp"
#include "../../constants.hpp"
#include "../rollup/verify.hpp"
#include "../join_split/create_noop_join_split_proof.hpp"
#include "compute_or_load_fixture.hpp"
#include "compute_circuit_data.hpp"
#include "root_rollup_circuit.hpp"
#include "create_root_rollup_tx.hpp"
#include "verify.hpp"
#include <common/test.hpp>

namespace rollup {
namespace proofs {
namespace root_rollup {

using namespace barretenberg;
using namespace notes::native;
using namespace plonk::stdlib::merkle_tree;

namespace {
std::shared_ptr<waffle::DynamicFileReferenceStringFactory> srs;
numeric::random::Engine* rand_engine = &numeric::random::get_debug_engine(true);
fixtures::user_context user = fixtures::create_user_context(rand_engine);
join_split::circuit_data join_split_cd;
account::circuit_data account_cd;
rollup::circuit_data tx_rollup_cd;
circuit_data root_rollup_cd;
std::vector<std::vector<uint8_t>> js_proofs;
std::vector<std::vector<uint8_t>> defi_js_proofs;
} // namespace

class root_rollup_tests : public ::testing::Test {
  protected:
    static constexpr auto CRS_PATH = "../srs_db/ignition";
    static constexpr auto FIXTURE_PATH = "../src/aztec/rollup/proofs/root_rollup/fixtures";
    static constexpr auto TEST_PROOFS_PATH = "../src/aztec/rollup/proofs/root_rollup/fixtures/test_proofs";
    static constexpr auto INNER_ROLLUP_TXS = 2U;
    static constexpr auto ROLLUPS_PER_ROLLUP = 3U;

    typedef std::vector<std::vector<std::vector<uint8_t>>> RollupStructure;

    root_rollup_tests()
        : data_tree(store, DATA_TREE_DEPTH, 0)
        , null_tree(store, NULL_TREE_DEPTH, 1)
        , root_tree(store, ROOT_TREE_DEPTH, 2)
        , defi_tree(store, DEFI_TREE_DEPTH, 3)
    {
        update_root_tree_with_data_root(0);
        rand_engine = &numeric::random::get_debug_engine(true);
        user = fixtures::create_user_context(rand_engine);
    }

    static void SetUpTestCase()
    {
        auto recreate = !exists(FIXTURE_PATH);
        srs = std::make_shared<waffle::DynamicFileReferenceStringFactory>(CRS_PATH);

        if (recreate) {
            // If no fixtures dir, recreate all proving keys, verification keys, padding proofs etc.
            account_cd = account::compute_or_load_circuit_data(srs, FIXTURE_PATH);
            join_split_cd = join_split::compute_or_load_circuit_data(srs, FIXTURE_PATH);
            tx_rollup_cd = rollup::get_circuit_data(
                INNER_ROLLUP_TXS, join_split_cd, account_cd, srs, FIXTURE_PATH, true, true, true);
            root_rollup_cd = get_circuit_data(ROLLUPS_PER_ROLLUP, tx_rollup_cd, srs, FIXTURE_PATH, true, true, true);
        } else {
            // Otherwise we should only need the inner proofs verification key for logic tests.
            tx_rollup_cd = rollup::get_circuit_data(
                INNER_ROLLUP_TXS, join_split_cd, account_cd, srs, FIXTURE_PATH, false, false, true, false, true);
            root_rollup_cd = get_circuit_data(
                ROLLUPS_PER_ROLLUP, tx_rollup_cd, srs, FIXTURE_PATH, false, false, false, false, false);
        }

        MemoryStore store;
        MerkleTree<MemoryStore> data_tree(store, DATA_TREE_DEPTH, 0);
        // Create 5 js proofs to play with.
        for (size_t i = 0; i < 5; ++i) {
            auto js_proof = compute_or_load_fixture(TEST_PROOFS_PATH, format("js", i), [&] {
                return create_noop_join_split_proof(join_split_cd, data_tree.root());
            });
            js_proofs.push_back(js_proof);
        }

        // // Create 3 defi deposit proofs to play with.
        // std::vector<uint32_t> deposit_amounts = { 100, 20, 60 };
        // std::vector<uint32_t> change_amounts = { 10, 70, 80 };
        // for (size_t i = 0; i < 3; i++) {
        //     auto defi_js_proof = compute_or_load_fixture(TEST_PROOFS_PATH, format("defi_js", i), [&] {
        //         return create_noop_join_split_proof(
        //             join_split_cd, data_tree.root(), true, deposit_amounts[i], change_amounts[i]);
        //     });
        //     defi_js_proofs.push_back(defi_js_proof);
        // }
    }

    root_rollup_tx create_root_rollup_tx(std::string const& test_name,
                                         uint32_t rollup_id,
                                         RollupStructure const& rollup_structure)
    {
        std::vector<rollup::rollup_tx> rollups;
        std::vector<std::vector<uint8_t>> rollups_data;

        for (auto tx_proofs : rollup_structure) {
            auto name = format(test_name, "_rollup", rollups.size() + 1);
            auto rollup = create_rollup_tx(tx_proofs);
            auto rollup_data = compute_or_load_rollup(name, rollup);
            ASSERT(!rollup_data.empty());
            rollups.push_back(rollup);
            rollups_data.push_back(rollup_data);
        }

        return root_rollup::create_root_rollup_tx(rollup_id, rollups_data, data_tree, root_tree, defi_tree);
    }

    void update_root_tree_with_data_root(size_t index)
    {
        auto data_root = to_buffer(data_tree.root());
        root_tree.update_element(index, data_root);
    }

    std::vector<uint8_t> compute_or_load_rollup(std::string const& name, rollup::rollup_tx& rollup)
    {
        return compute_or_load_fixture(TEST_PROOFS_PATH, name, [&] {
            // We need to ensure we have a proving key to build the inner proof fixtures.
            if (!tx_rollup_cd.proving_key) {
                account_cd = account::compute_or_load_circuit_data(srs, FIXTURE_PATH);
                join_split_cd = join_split::compute_or_load_circuit_data(srs, FIXTURE_PATH);
                tx_rollup_cd = rollup::get_circuit_data(
                    INNER_ROLLUP_TXS, join_split_cd, account_cd, srs, FIXTURE_PATH, true, true, true);
                root_rollup_cd.inner_rollup_circuit_data = tx_rollup_cd;
            }
            return rollup::verify_rollup(rollup, tx_rollup_cd).proof_data;
        });
    }

    rollup::rollup_tx create_rollup_tx(std::vector<std::vector<uint8_t>> const& txs)
    {
        return rollup::create_rollup(txs, data_tree, null_tree, root_tree, INNER_ROLLUP_TXS);
    }

    MemoryStore store;
    MerkleTree<MemoryStore> data_tree;
    MerkleTree<MemoryStore> null_tree;
    MerkleTree<MemoryStore> root_tree;
    MerkleTree<MemoryStore> defi_tree;
};

/*
 * The fixtures names are named so as to reduce unnecessary (re)computation.
 * i.e. If a rollup has a structure shorter than its name suggests, it's because it can reuse the fixtures from
 * the longer rollup structure due to them having the same leading structure.
 */
TEST_F(root_rollup_tests, test_root_rollup_1_real_2_padding)
{
    auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_TRUE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_root_rollup_2_real_1_padding)
{
    auto tx_data = create_root_rollup_tx("root_211", 0, { { js_proofs[0], js_proofs[1] }, { js_proofs[2] } });
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_TRUE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_root_rollup_3_real_0_padding)
{
    auto tx_data = create_root_rollup_tx(
        "root_221", 0, { { js_proofs[0], js_proofs[1] }, { js_proofs[2], js_proofs[3] }, { js_proofs[4] } });
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_TRUE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_incorrect_new_data_root_fails)
{
    auto tx_data = create_root_rollup_tx("bad_new_data_root_fail", 0, { { js_proofs[0] } });
    tx_data.new_data_roots_root = fr::random_element();
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_inner_rollups_out_of_order_fail)
{
    auto tx_data =
        create_root_rollup_tx("root_221", 0, { { js_proofs[0], js_proofs[1] }, { js_proofs[2], js_proofs[3] } });
    std::swap(tx_data.rollups[0], tx_data.rollups[1]);

    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_invalid_padding_proof_fail)
{
    auto tx_data = create_root_rollup_tx(
        "root_221", 0, { { js_proofs[0], js_proofs[1] }, { js_proofs[2], js_proofs[3] }, { js_proofs[4] } });
    tx_data.num_inner_proofs = 2;
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_invalid_last_proof_fail)
{
    auto tx_data = create_root_rollup_tx("root_221", 0, { { js_proofs[0], js_proofs[1] } });
    tx_data.num_inner_proofs = 2;
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

TEST_F(root_rollup_tests, valid_previous_defi_hash_for_0_interactions)
{
    auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_TRUE(result.logic_verified);

    auto previous_defi_interaction_hash = result.public_inputs.back();

    std::vector<uint8_t> sha256_input;
    for (size_t i = 0; i < NUM_BRIDGE_CALLS_PER_BLOCK; i++) {
        notes::native::defi_interaction::defi_interaction_note note = { 0, 0, 0, 0, 0, false };
        auto buf = note.to_byte_array();
        sha256_input.insert(sha256_input.end(), buf.begin(), buf.end());
    }
    auto expected = sha256::sha256(sha256_input);

    ASSERT_EQ(from_buffer<fr>(expected), previous_defi_interaction_hash);
}

TEST_F(root_rollup_tests, test_bridge_ids_with_0_interactions_always_succeeds)
{
    auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
    tx_data.num_defi_interactions = 0;
    tx_data.bridge_ids = { 0, 123, 56, 178 };
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_TRUE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_zero_bridge_id_fails)
{
    auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
    tx_data.num_defi_interactions = 4;
    tx_data.bridge_ids = { 0, 123, 56, 178 };
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

TEST_F(root_rollup_tests, test_interactions_greater_than_num_bridge_ids_fails)
{
    auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
    tx_data.bridge_ids = { 123, 56, 0, 0 };
    tx_data.num_defi_interactions = 3;
    auto result = verify_logic(tx_data, root_rollup_cd);
    ASSERT_FALSE(result.logic_verified);
}

// TEST_F(root_rollup_tests, test_repeating_bridge_ids_fails)
// {
//     auto tx_data = create_root_rollup_tx("root_1", 0, { { js_proofs[0] } });
//     tx_data.bridge_ids = { 123, 123, 45, 12 };
//     tx_data.num_defi_interactions = 4;
//     auto result = verify_logic(tx_data, root_rollup_cd);
//     ASSERT_FALSE(result.logic_verified);
// }

} // namespace root_rollup
} // namespace proofs
} // namespace rollup