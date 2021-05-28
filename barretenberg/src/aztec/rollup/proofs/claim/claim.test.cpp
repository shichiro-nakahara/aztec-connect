#include "../../constants.hpp"
#include "../../fixtures/user_context.hpp"
#include "index.hpp"
#include "../inner_proof_data.hpp"
#include "../notes/native/index.hpp"
#include <common/test.hpp>
#include <stdlib/merkle_tree/index.hpp>
#include <numeric/random/engine.hpp>

using namespace barretenberg;
using namespace plonk::stdlib::types::turbo;
using namespace plonk::stdlib::merkle_tree;
using namespace rollup::proofs;
using namespace rollup::proofs::notes::native;
using namespace rollup::proofs::notes::native::claim;
using namespace rollup::proofs::notes::native::value;
using namespace rollup::proofs::notes::native::defi_interaction;
using namespace rollup::proofs::claim;

namespace {
std::shared_ptr<waffle::FileReferenceStringFactory> srs;
circuit_data cd;
auto& engine = numeric::random::get_debug_engine();
} // namespace

namespace rollup {

class claim_tests : public ::testing::Test {
  protected:
    static void SetUpTestCase()
    {
        srs = std::make_shared<waffle::FileReferenceStringFactory>("../srs_db");
        cd = get_circuit_data(srs, "", true, false, false);
    }

    virtual void SetUp()
    {
        store = std::make_unique<MemoryStore>();
        data_tree = std::make_unique<MerkleTree<MemoryStore>>(*store, DATA_TREE_DEPTH, 0);
        defi_tree = std::make_unique<MerkleTree<MemoryStore>>(*store, DEFI_TREE_DEPTH, 1);
        user = rollup::fixtures::create_user_context();
    }

    template <typename T, typename Tree> void append_note(T const& note, Tree& tree)
    {
        auto enc_note = encrypt(note);
        std::vector<uint8_t> buf;
        write(buf, enc_note.x);
        write(buf, enc_note.y);
        tree->update_element(tree->size(), buf);
    }

    claim_tx create_claim_tx(claim_note const& claim_note,
                             uint32_t claim_note_index,
                             defi_interaction_note const& interaction_note)
    {
        claim_tx tx;
        tx.data_root = data_tree->root();
        tx.claim_note = claim_note;
        tx.claim_note_index = claim_note_index;
        tx.claim_note_path = data_tree->get_hash_path(claim_note_index);

        tx.defi_root = defi_tree->root();
        tx.defi_interaction_note = interaction_note;
        tx.defi_interaction_note_path = defi_tree->get_hash_path(interaction_note.interaction_nonce);

        tx.output_value_a = ((uint512_t(claim_note.deposit_value) * uint512_t(interaction_note.total_output_a_value)) /
                             uint512_t(interaction_note.total_input_value))
                                .lo;
        tx.output_value_b = ((uint512_t(claim_note.deposit_value) * uint512_t(interaction_note.total_output_b_value)) /
                             uint512_t(interaction_note.total_input_value))
                                .lo;
        return tx;
    }

    rollup::fixtures::user_context user;
    std::unique_ptr<MemoryStore> store;
    std::unique_ptr<MerkleTree<MemoryStore>> data_tree;
    std::unique_ptr<MerkleTree<MemoryStore>> defi_tree;
    const uint32_t asset_id = 1;
};

TEST_F(claim_tests, test_claim)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);

    EXPECT_TRUE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_unmatching_ratio_a_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);
    tx.output_value_a = 10;

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_unmatching_ratio_b_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);
    tx.output_value_b = 10;

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_unmatching_bridge_ids_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 1, 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_unmatching_interaction_nonces_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 1, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_missing_claim_note_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 0, 100, 200, 300, 1 };
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_missing_interaction_note_fails)
{
    const claim_note note1 = { 10, 0, 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0) };
    const defi_interaction_note note2 = { 0, 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);

    EXPECT_FALSE(verify_logic(tx, cd));
}

TEST_F(claim_tests, test_claim_2_outputs_full_proof)
{
    const bridge_id bridge_id = { 0, 2, 0, 111, 222 };

    // Create some values for our circuit that are large enough to properly test the ratio checks.
    auto random_value = []() {
        uint256_t a = engine.get_random_uint256();
        a.data[3] = a.data[3] & 0x0fffffffffffffffULL;
        return a;
    };
    uint256_t input_value = random_value();
    uint256_t total_input = random_value();
    uint256_t total_output_a = random_value();
    uint256_t total_output_b = random_value();
    // Check total_in >= user_in. Does not work otherwise because we get integer overflow.
    if (input_value > total_input) {
        std::swap(input_value, total_input);
    }

    // Create and add a claim note, and a defi interaction note, to the data tree.
    const claim_note note1 = {
        input_value, bridge_id.to_uint256_t(), 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0)
    };
    const defi_interaction_note note2 = { bridge_id.to_uint256_t(), 0, total_input, total_output_a, total_output_b, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);

    // Construct transaction data.
    claim_tx tx = create_claim_tx(note1, 0, note2);

    // Verify proof.
    auto result = verify(tx, cd);
    ASSERT_TRUE(result.verified);

    // Compute expected public inputs.
    auto proof_data = inner_proof_data(result.proof_data);
    const value_note expected_output_note1 = {
        tx.output_value_a, bridge_id.output_asset_id_a, 0, user.owner.public_key, user.note_secret
    };
    const value_note expected_output_note2 = {
        tx.output_value_b, bridge_id.output_asset_id_b, 0, user.owner.public_key, user.note_secret
    };
    auto enc_output_note1 = encrypt(expected_output_note1);
    auto enc_output_note2 = encrypt(expected_output_note2);
    uint256_t nullifier1 = compute_nullifier(encrypt(note1), tx.claim_note_index);

    // Validate public inputs.
    EXPECT_EQ(proof_data.proof_id, 3UL);
    EXPECT_EQ(proof_data.asset_id, tx.claim_note.bridge_id);
    EXPECT_EQ(proof_data.merkle_root, data_tree->root());
    EXPECT_EQ(proof_data.new_note1, enc_output_note1);
    EXPECT_EQ(proof_data.new_note2, enc_output_note2);
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, uint256_t(0));
    EXPECT_EQ(proof_data.input_owner, defi_tree->root());
    EXPECT_EQ(proof_data.output_owner, fr(0));
    EXPECT_EQ(proof_data.public_input, uint256_t(0));
    EXPECT_EQ(proof_data.public_output, uint256_t(0));
    EXPECT_EQ(proof_data.tx_fee, 0UL);
}

TEST_F(claim_tests, test_claim_1_output_full_proof)
{
    const bridge_id bridge_id = { 0, 1, 0, 111, 222 };
    const claim_note note1 = {
        10, bridge_id.to_uint256_t(), 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0)
    };
    const defi_interaction_note note2 = { bridge_id.to_uint256_t(), 0, 100, 200, 300, 1 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);
    auto result = verify(tx, cd);

    auto proof_data = inner_proof_data(result.proof_data);

    const value_note expected_output_note1 = {
        20, bridge_id.output_asset_id_a, 0, user.owner.public_key, user.note_secret
    };
    auto enc_output_note1 = encrypt(expected_output_note1);

    uint256_t nullifier1 = compute_nullifier(encrypt(note1), tx.claim_note_index);

    EXPECT_EQ(proof_data.proof_id, 3UL);
    EXPECT_EQ(proof_data.asset_id, tx.claim_note.bridge_id);
    EXPECT_EQ(proof_data.merkle_root, data_tree->root());
    EXPECT_EQ(proof_data.new_note1, enc_output_note1);
    EXPECT_EQ(proof_data.new_note2, grumpkin::g1::affine_element(0, 0));
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, uint256_t(0));
    EXPECT_EQ(proof_data.input_owner, defi_tree->root());
    EXPECT_EQ(proof_data.output_owner, fr(0));
    EXPECT_EQ(proof_data.public_input, uint256_t(0));
    EXPECT_EQ(proof_data.public_output, uint256_t(0));
    EXPECT_EQ(proof_data.tx_fee, 0UL);

    EXPECT_TRUE(result.verified);
}

TEST_F(claim_tests, test_claim_refund_full_proof)
{
    const bridge_id bridge_id = { 0, 1, 0, 111, 222 };
    const claim_note note1 = {
        10, bridge_id.to_uint256_t(), 0, create_partial_value_note(user.note_secret, user.owner.public_key, 0)
    };
    const defi_interaction_note note2 = { bridge_id.to_uint256_t(), 0, 100, 200, 300, 0 };
    append_note(note1, data_tree);
    append_note(note2, defi_tree);
    claim_tx tx = create_claim_tx(note1, 0, note2);
    auto result = verify(tx, cd);

    auto proof_data = inner_proof_data(result.proof_data);

    const value_note expected_output_note1 = {
        10, bridge_id.input_asset_id, 0, user.owner.public_key, user.note_secret
    };
    auto enc_output_note1 = encrypt(expected_output_note1);

    uint256_t nullifier1 = compute_nullifier(encrypt(note1), tx.claim_note_index);

    EXPECT_EQ(proof_data.proof_id, 3UL);
    EXPECT_EQ(proof_data.asset_id, tx.claim_note.bridge_id);
    EXPECT_EQ(proof_data.merkle_root, data_tree->root());
    EXPECT_EQ(proof_data.new_note1, enc_output_note1);
    EXPECT_EQ(proof_data.new_note2, grumpkin::g1::affine_element(0, 0));
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, uint256_t(0));
    EXPECT_EQ(proof_data.input_owner, defi_tree->root());
    EXPECT_EQ(proof_data.output_owner, fr(0));
    EXPECT_EQ(proof_data.public_input, uint256_t(0));
    EXPECT_EQ(proof_data.public_output, uint256_t(0));
    EXPECT_EQ(proof_data.tx_fee, 0UL);

    EXPECT_TRUE(result.verified);
}

} // namespace rollup