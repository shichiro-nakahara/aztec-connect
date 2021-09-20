#include "../../constants.hpp"
#include "../../fixtures/user_context.hpp"
#include "../inner_proof_data.hpp"
#include "index.hpp"
#include "../notes/native/index.hpp"
#include <common/streams.hpp>
#include <common/test.hpp>
#include <stdlib/merkle_tree/index.hpp>

namespace rollup {
namespace proofs {
namespace join_split {

using namespace barretenberg;
using namespace plonk::stdlib::types::turbo;
using namespace plonk::stdlib::merkle_tree;
using namespace rollup::proofs;
using namespace rollup::proofs::notes::native;

auto create_account_leaf_data(fr const& account_alias_id,
                              grumpkin::g1::affine_element const& owner_key,
                              grumpkin::g1::affine_element const& signing_key)
{
    return notes::native::account::account_note{ account_alias_id, owner_key, signing_key }.commit();
}

class join_split_tests : public ::testing::Test {
  protected:
#ifndef DISABLE_HEAVY_TESTS
    static void SetUpTestCase()
    {
        auto null_crs_factory = std::make_unique<waffle::ReferenceStringFactory>();
        init_proving_key(std::move(null_crs_factory));
        auto crs_factory = std::make_unique<waffle::FileReferenceStringFactory>("../srs_db");
        init_verification_key(std::move(crs_factory));
    }
#endif

    virtual void SetUp()
    {
        store = std::make_unique<MemoryStore>();
        tree = std::make_unique<MerkleTree<MemoryStore>>(*store, 32);
        user = rollup::fixtures::create_user_context();

        value_notes[0] = { 100, asset_id, 0, user.owner.public_key, user.note_secret, user.owner.public_key.x };
        value_notes[1] = { 50,
                           asset_id,
                           0,
                           user.owner.public_key,
                           user.note_secret,
                           rollup::fixtures::create_key_pair(nullptr).public_key.x };
        value_notes[2] = { 90, asset_id, 1, user.owner.public_key, user.note_secret, 0 };
        value_notes[3] = { 40, asset_id, 1, user.owner.public_key, user.note_secret, 0 };
        dummy_value_notes[0] = { 0, asset_id, 0, user.owner.public_key, fr::random_element(), 0 };
        dummy_value_notes[1] = { 0, asset_id, 0, user.owner.public_key, fr::random_element(), 0 };
    }

    /**
     * Add two account notes for the user.
     */
    void preload_account_notes()
    {
        auto account_alias_id = rollup::fixtures::generate_account_alias_id(user.alias_hash, 1);
        tree->update_element(
            tree->size(),
            create_account_leaf_data(account_alias_id, user.owner.public_key, user.signing_keys[0].public_key));
        tree->update_element(
            tree->size(),
            create_account_leaf_data(account_alias_id, user.owner.public_key, user.signing_keys[1].public_key));
    }

    /**
     * Add two value notes with nonce 0, and two value notes with nonce 1.
     */
    void preload_value_notes()
    {
        for (auto note : value_notes) {
            tree->update_element(tree->size(), note.commit());
        }
    }

    void append_notes(std::vector<value_note> const& notes)
    {
        for (auto note : notes) {
            tree->update_element(tree->size(), note.commit());
        }
    }

    join_split_tx create_join_split_tx(std::array<uint32_t, 2> const& input_indicies,
                                       std::array<value_note, 2> const& input_notes,
                                       uint32_t tx_asset_id,
                                       uint32_t account_index = 0,
                                       uint32_t nonce = 0)
    {
        value_note output_note1 = {
            input_notes[0].value + input_notes[1].value, tx_asset_id, nonce, user.owner.public_key, user.note_secret, 0
        };
        value_note output_note2 = { 0, tx_asset_id, nonce, user.owner.public_key, user.note_secret, 0 };

        join_split_tx tx;
        tx.public_input = 0;
        tx.public_output = 0;
        tx.num_input_notes = 2;
        tx.input_index = input_indicies;
        tx.old_data_root = tree->root();
        tx.input_path = { tree->get_hash_path(input_indicies[0]), tree->get_hash_path(input_indicies[1]) };
        tx.input_note = input_notes;
        tx.output_note = { output_note1, output_note2 };
        tx.public_owner = fr(0);
        tx.account_index = account_index;
        tx.account_path = tree->get_hash_path(account_index);
        tx.signing_pub_key = user.signing_keys[0].public_key;
        tx.asset_id = tx_asset_id;
        tx.account_private_key = user.owner.private_key;
        tx.alias_hash = !nonce ? rollup::fixtures::generate_alias_hash("penguin") : user.alias_hash;
        tx.nonce = nonce;
        return tx;
    }

    /**
     * Add account notes and value notes (sum 150).
     * Return a join split tx that spends them.
     */
    join_split_tx simple_setup(std::array<uint32_t, 2> const& input_indicies = { 0, 1 },
                               uint32_t account_index = 0,
                               uint32_t nonce = 0)
    {
        preload_value_notes();   // indicies: [0, 1](nonce 0), [2, 3](nonce 1)
        preload_account_notes(); // indicies: [4, 5]
        return create_join_split_tx(input_indicies,
                                    { value_notes[input_indicies[0]], value_notes[input_indicies[1]] },
                                    asset_id,
                                    account_index,
                                    nonce);
    }

    waffle::plonk_proof sign_and_create_proof(join_split_tx& tx, grumpkin::fr const& signing_private_key)
    {
        tx.signature = sign_join_split_tx(tx, { signing_private_key, tx.signing_pub_key });

        auto prover = new_join_split_prover(tx);
        return prover.construct_proof();
    }

    bool sign_and_verify(join_split_tx& tx, grumpkin::fr const& signing_private_key)
    {
        return verify_proof(sign_and_create_proof(tx, signing_private_key));
    }

    bool verify_logic(join_split_tx& tx)
    {
        Composer composer(get_proving_key(), nullptr);
        join_split_circuit(composer, tx);
        if (composer.failed) {
            std::cout << "Logic failed: " << composer.err << std::endl;
        }
        return !composer.failed;
    }

    bool sign_and_verify_logic(join_split_tx& tx, grumpkin::fr const& signing_private_key)
    {
        tx.signature = sign_join_split_tx(tx, { signing_private_key, tx.signing_pub_key });

        return verify_logic(tx);
    }

    rollup::fixtures::user_context user;
    std::unique_ptr<MemoryStore> store;
    std::unique_ptr<MerkleTree<MemoryStore>> tree;
    value_note value_notes[4];
    value_note dummy_value_notes[2];
    const uint32_t asset_id = 1;
    const uint256_t max_value = (uint256_t(1) << notes::NOTE_VALUE_BIT_LENGTH) - 1;
};

TEST_F(join_split_tests, test_0_input_notes)
{
    value_note gibberish = { 0, asset_id, 0, user.owner.public_key, user.note_secret, 0 };

    join_split_tx tx = simple_setup();
    tx.num_input_notes = 0;
    tx.input_note = { gibberish, gibberish };
    tx.public_input = 30;
    tx.public_owner = fr::random_element();
    tx.output_note[0].value = 30;

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_padding_input_note_non_0_value_fails)
{
    value_note gibberish = { 10, asset_id, 0, user.owner.public_key, user.note_secret, 0 };

    join_split_tx tx = simple_setup();
    tx.num_input_notes = 0;
    tx.input_note = { gibberish, gibberish };
    tx.output_note[0].value = 20;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_1_input_note)
{
    join_split_tx tx = simple_setup();
    tx.num_input_notes = 1;
    tx.input_note[1].value = 0;
    tx.output_note[0].value = tx.input_note[0].value;

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_2_input_notes)
{
    join_split_tx tx = simple_setup();
    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_0_output_notes)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = 0;
    tx.output_note[1].value = 0;
    tx.public_output = tx.input_note[0].value + tx.input_note[1].value;
    tx.public_owner = fr::random_element();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Public values

TEST_F(join_split_tests, test_max_public_input_output)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = max_value;
    tx.public_input = max_value;
    tx.public_owner = fr::random_element();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_overflow_public_input_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = max_value;
    tx.public_input = max_value + 1;
    tx.public_owner = fr::random_element();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_overflow_public_output_fails)
{
    join_split_tx tx = simple_setup();
    tx.input_note[0].value = max_value;
    tx.public_output = max_value + 1;
    tx.public_owner = fr::random_element();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Tx fee

TEST_F(join_split_tests, test_non_zero_tx_fee)
{
    join_split_tx tx = simple_setup();
    tx.public_input += 1;
    tx.public_owner = fr::random_element();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_non_zero_tx_fee_zero_public_values)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value -= 1;

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_max_tx_fee)
{
    join_split_tx tx = simple_setup();
    auto tx_fee = (uint256_t(1) << rollup::TX_FEE_BIT_LENGTH) - 1;
    tx.public_input += tx_fee;
    tx.public_owner = fr::random_element();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_overflow_tx_fee_fails)
{
    join_split_tx tx = simple_setup();
    auto tx_fee = uint256_t(1) << rollup::TX_FEE_BIT_LENGTH;
    tx.public_input += tx_fee;
    tx.public_owner = fr::random_element();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_larger_total_output_value_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value += 1;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Asset id

TEST_F(join_split_tests, test_wrong_asset_id_fails)
{
    join_split_tx tx = simple_setup();
    tx.asset_id = 3;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_different_input_note_asset_id_fails)
{
    join_split_tx tx = simple_setup();
    tx.input_note[0].asset_id = 3;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_different_output_note_asset_id_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].asset_id = 3;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_different_input_output_asset_id_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].asset_id = 3;
    tx.output_note[1].asset_id = 3;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_invalid_asset_id_fails)
{
    uint32_t invalid_asset_id = rollup::MAX_NUM_ASSETS;
    std::vector<value_note> input_notes = { { 100, invalid_asset_id, 0, user.owner.public_key, user.note_secret, 0 },
                                            { 50, invalid_asset_id, 0, user.owner.public_key, user.note_secret, 0 } };
    append_notes(input_notes);
    auto tx = create_join_split_tx({ 0, 1 }, { input_notes[0], input_notes[1] }, invalid_asset_id);

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Input note

TEST_F(join_split_tests, test_joining_same_note_fails)
{
    join_split_tx tx = simple_setup({ 1, 1 });
    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_different_input_note_nonces_fails)
{
    join_split_tx tx = simple_setup({ 1, 2 });

    EXPECT_NE(tx.input_note[0].nonce, tx.input_note[1].nonce);
    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Input note account value id

TEST_F(join_split_tests, test_spend_notes_with_registered_account)
{
    join_split_tx tx = simple_setup({ 2, 3 }, 4, 1);
    EXPECT_TRUE(sign_and_verify_logic(tx, user.signing_keys[0].private_key));
}

TEST_F(join_split_tests, test_different_note_nonce_vs_account_nonce_fails)
{
    join_split_tx tx = simple_setup({ 2, 3 }, 4, 0);
    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_wrong_input_note_owner_fails)
{
    join_split_tx tx = simple_setup();
    tx.input_note[0].owner = grumpkin::g1::element::random_element();
    tx.input_note[1].owner = tx.input_note[0].owner;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Output note owner

TEST_F(join_split_tests, test_random_output_note_owners)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].owner = grumpkin::g1::element::random_element();
    tx.output_note[1].owner = grumpkin::g1::element::random_element();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Signature

TEST_F(join_split_tests, test_wrong_account_private_key_fails)
{
    join_split_tx tx = simple_setup();
    tx.account_private_key = grumpkin::fr::random_element();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_wrong_public_owner_sig_fail)
{
    join_split_tx tx = simple_setup();
    tx.public_input = 1;
    tx.public_owner = fr::random_element();

    // sign over a different public owner
    tx.signature = sign_join_split_tx(tx, { user.owner.private_key, tx.signing_pub_key });

    tx.public_owner = fr::random_element();

    EXPECT_FALSE(verify_logic(tx));
}

TEST_F(join_split_tests, test_spend_zero_nonce_notes_with_signing_key_fails)
{
    join_split_tx tx = simple_setup();
    EXPECT_FALSE(sign_and_verify_logic(tx, user.signing_keys[0].private_key));
}

TEST_F(join_split_tests, test_spend_registered_notes_with_owner_key_fails)
{
    auto tx = simple_setup({ 2, 3 }, 4, 1);
    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

// Account membership

TEST_F(join_split_tests, test_wrong_merkle_root_fails)
{
    join_split_tx tx = simple_setup();
    tx.old_data_root = fr::random_element();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_wrong_alias_hash_fails)
{
    join_split_tx tx = simple_setup({ 2, 3 }, 4, 1);
    tx.alias_hash = rollup::fixtures::generate_alias_hash("chicken");
    EXPECT_FALSE(sign_and_verify_logic(tx, user.signing_keys[0].private_key));
}

TEST_F(join_split_tests, test_nonregistered_signing_key_fails)
{
    join_split_tx tx = simple_setup({ 2, 3 }, 4, 1);
    auto keys = rollup::fixtures::create_key_pair(nullptr);
    tx.signing_pub_key = keys.public_key;

    EXPECT_FALSE(sign_and_verify_logic(tx, keys.private_key));
}

TEST_F(join_split_tests, test_wrong_note_hash_path_fails)
{
    join_split_tx tx = simple_setup();
    auto gibberish_path = fr_hash_path(32, std::make_pair(fr::random_element(), fr::random_element()));
    tx.input_path[0] = gibberish_path;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

HEAVY_TEST_F(join_split_tests, test_tainted_public_owner_fails)
{
    join_split_tx tx = simple_setup();
    tx.public_input = 1;
    tx.signing_pub_key = user.owner.public_key;
    uint8_t public_owner[32] = { 0x01, 0xaa, 0x42, 0xd4, 0x72, 0x88, 0x8e, 0xae, 0xa5, 0x56, 0x39,
                                 0x46, 0xeb, 0x5c, 0xf5, 0x6c, 0x81, 0x6,  0x4d, 0x80, 0xc6, 0xf5,
                                 0xa5, 0x38, 0xcc, 0x87, 0xae, 0x54, 0xae, 0xdb, 0x75, 0xd9 };
    tx.public_owner = from_buffer<fr>(public_owner);
    tx.signature = sign_join_split_tx(tx, { user.owner.private_key, user.owner.public_key });

    auto prover = new_join_split_prover(tx);
    auto proof = prover.construct_proof();

    EXPECT_EQ(proof.proof_data[InnerProofOffsets::PUBLIC_OWNER], 0x01);
    proof.proof_data[InnerProofFields::PUBLIC_OWNER] = 0x02;

    EXPECT_FALSE(verify_proof(proof));
}

TEST_F(join_split_tests, test_invalid_bridge_id)
{
    join_split_tx tx = simple_setup();
    tx.claim_note.deposit_value = 1;

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_defi_non_zero_public_input_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = 0;
    tx.output_note[1].value = 100;
    tx.claim_note.deposit_value = 50;
    tx.public_input = 1;

    bridge_id bridge_id = { 0, 2, tx.asset_id, 0, 0 };
    tx.claim_note.bridge_id = bridge_id.to_uint256_t();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_defi_non_zero_public_output_fails)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = 0;
    tx.output_note[1].value = 100;
    tx.claim_note.deposit_value = 50;
    tx.public_output = 1;

    bridge_id bridge_id = { 0, 2, tx.asset_id, 0, 0 };
    tx.claim_note.bridge_id = bridge_id.to_uint256_t();

    EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
}

TEST_F(join_split_tests, test_defi_non_zero_output_note_1_ignored)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = 10; // This should be ignored in fee calculation!
    tx.output_note[1].value = 100;
    tx.claim_note.deposit_value = 50;

    bridge_id bridge_id = { 0, 2, tx.asset_id, 0, 0 };
    tx.claim_note.bridge_id = bridge_id.to_uint256_t();

    EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
}

HEAVY_TEST_F(join_split_tests, test_deposit_full_proof)
{
    simple_setup();
    join_split_tx tx = create_join_split_tx({ 0, 1 }, { dummy_value_notes[0], dummy_value_notes[1] }, asset_id);
    tx.num_input_notes = 0;
    tx.public_input = 10;
    tx.public_owner = fr::random_element();
    tx.output_note[0].value = 7;

    auto proof = sign_and_create_proof(tx, user.owner.private_key);
    auto proof_data = inner_proof_data(proof.proof_data);

    auto input_note1_commitment = tx.input_note[0].commit();
    auto input_note2_commitment = tx.input_note[1].commit();
    auto output_note1_commitment = tx.output_note[0].commit();
    auto output_note2_commitment = tx.output_note[1].commit();
    uint256_t nullifier1 = compute_nullifier(input_note1_commitment, 0, user.owner.private_key, false);
    uint256_t nullifier2 = compute_nullifier(input_note2_commitment, 1, user.owner.private_key, false);

    EXPECT_EQ(proof_data.proof_id, ProofIds::DEPOSIT);
    EXPECT_EQ(proof_data.note_commitment1, output_note1_commitment);
    EXPECT_EQ(proof_data.note_commitment2, output_note2_commitment);
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, nullifier2);
    EXPECT_EQ(proof_data.public_value, tx.public_input);
    EXPECT_EQ(proof_data.public_owner, tx.public_owner);
    EXPECT_EQ(proof_data.asset_id, tx.asset_id);
    EXPECT_EQ(proof_data.merkle_root, tree->root());
    EXPECT_EQ(proof_data.tx_fee, uint256_t(3));
    EXPECT_EQ(proof_data.tx_fee_asset_id, tx.asset_id);
    EXPECT_EQ(proof_data.bridge_id, uint256_t(0));
    EXPECT_EQ(proof_data.defi_deposit_value, uint256_t(0));
    EXPECT_EQ(proof_data.defi_root, fr(0));

    EXPECT_TRUE(verify_proof(proof));
}

HEAVY_TEST_F(join_split_tests, test_withdraw_full_proof)
{
    join_split_tx tx = simple_setup();
    tx.public_output = 10;
    tx.public_owner = fr::random_element();
    tx.output_note[0].value -= 13;

    auto proof = sign_and_create_proof(tx, user.owner.private_key);
    auto proof_data = inner_proof_data(proof.proof_data);

    auto input_note1_commitment = tx.input_note[0].commit();
    auto input_note2_commitment = tx.input_note[1].commit();
    auto output_note1_commitment = tx.output_note[0].commit();
    auto output_note2_commitment = tx.output_note[1].commit();
    uint256_t nullifier1 = compute_nullifier(input_note1_commitment, 0, user.owner.private_key, true);
    uint256_t nullifier2 = compute_nullifier(input_note2_commitment, 1, user.owner.private_key, true);

    EXPECT_EQ(proof_data.proof_id, ProofIds::WITHDRAW);
    EXPECT_EQ(proof_data.note_commitment1, output_note1_commitment);
    EXPECT_EQ(proof_data.note_commitment2, output_note2_commitment);
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, nullifier2);
    EXPECT_EQ(proof_data.public_value, tx.public_output);
    EXPECT_EQ(proof_data.public_owner, tx.public_owner);
    EXPECT_EQ(proof_data.asset_id, tx.asset_id);
    EXPECT_EQ(proof_data.merkle_root, tree->root());
    EXPECT_EQ(proof_data.tx_fee, uint256_t(3));
    EXPECT_EQ(proof_data.tx_fee_asset_id, tx.asset_id);
    EXPECT_EQ(proof_data.bridge_id, uint256_t(0));
    EXPECT_EQ(proof_data.defi_deposit_value, uint256_t(0));
    EXPECT_EQ(proof_data.defi_root, fr(0));

    EXPECT_TRUE(verify_proof(proof));
}

HEAVY_TEST_F(join_split_tests, test_private_send_full_proof)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value -= 3;

    auto proof = sign_and_create_proof(tx, user.owner.private_key);
    auto proof_data = inner_proof_data(proof.proof_data);

    auto input_note1_commitment = tx.input_note[0].commit();
    auto input_note2_commitment = tx.input_note[1].commit();
    auto output_note1_commitment = tx.output_note[0].commit();
    auto output_note2_commitment = tx.output_note[1].commit();
    uint256_t nullifier1 = compute_nullifier(input_note1_commitment, 0, user.owner.private_key, true);
    uint256_t nullifier2 = compute_nullifier(input_note2_commitment, 1, user.owner.private_key, true);

    EXPECT_EQ(proof_data.proof_id, ProofIds::SEND);
    EXPECT_EQ(proof_data.note_commitment1, output_note1_commitment);
    EXPECT_EQ(proof_data.note_commitment2, output_note2_commitment);
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, nullifier2);
    EXPECT_EQ(proof_data.public_value, uint256_t(0));
    EXPECT_EQ(proof_data.public_owner, fr(0));
    EXPECT_EQ(proof_data.asset_id, uint256_t(0));
    EXPECT_EQ(proof_data.merkle_root, tree->root());
    EXPECT_EQ(proof_data.tx_fee, uint256_t(3));
    EXPECT_EQ(proof_data.tx_fee_asset_id, tx.asset_id);
    EXPECT_EQ(proof_data.bridge_id, uint256_t(0));
    EXPECT_EQ(proof_data.defi_deposit_value, uint256_t(0));
    EXPECT_EQ(proof_data.defi_root, fr(0));

    EXPECT_TRUE(verify_proof(proof));
}

HEAVY_TEST_F(join_split_tests, test_defi_deposit_full_proof)
{
    join_split_tx tx = simple_setup();
    tx.output_note[0].value = 10; // This should be ignored anyway!
    tx.output_note[1].value = 91;
    tx.claim_note.deposit_value = 50;

    bridge_id bridge_id = { 0, 2, tx.asset_id, 0, 0 };
    tx.claim_note.bridge_id = bridge_id.to_uint256_t();
    auto proof = sign_and_create_proof(tx, user.owner.private_key);

    auto proof_data = inner_proof_data(proof.proof_data);

    auto partial_commitment =
        value::create_partial_commitment(tx.claim_note.note_secret, tx.input_note[0].owner, tx.input_note[0].nonce, 0);
    claim::claim_note claim_note = { tx.claim_note.deposit_value, tx.claim_note.bridge_id, 0, 0, partial_commitment };

    auto input_note1_commitment = tx.input_note[0].commit();
    auto input_note2_commitment = tx.input_note[1].commit();
    auto output_note1_commitment = claim_note.partial_commit();
    auto output_note2_commitment = tx.output_note[1].commit();
    uint256_t nullifier1 = compute_nullifier(input_note1_commitment, 0, user.owner.private_key, true);
    uint256_t nullifier2 = compute_nullifier(input_note2_commitment, 1, user.owner.private_key, true);

    EXPECT_EQ(proof_data.proof_id, ProofIds::DEFI_DEPOSIT);
    EXPECT_EQ(proof_data.note_commitment1, output_note1_commitment);
    EXPECT_EQ(proof_data.note_commitment2, output_note2_commitment);
    EXPECT_EQ(proof_data.nullifier1, nullifier1);
    EXPECT_EQ(proof_data.nullifier2, nullifier2);
    EXPECT_EQ(proof_data.public_value, uint256_t(0));
    EXPECT_EQ(proof_data.public_owner, fr(0));
    EXPECT_EQ(proof_data.asset_id, uint256_t(0));
    EXPECT_EQ(proof_data.merkle_root, tree->root());
    EXPECT_EQ(proof_data.tx_fee, uint256_t(9));
    EXPECT_EQ(proof_data.tx_fee_asset_id, bridge_id.input_asset_id);
    EXPECT_EQ(proof_data.bridge_id, tx.claim_note.bridge_id);
    EXPECT_EQ(proof_data.defi_deposit_value, tx.claim_note.deposit_value);
    EXPECT_EQ(proof_data.defi_root, fr(0));

    EXPECT_TRUE(verify_proof(proof));
}

TEST_F(join_split_tests, test_non_zero_output_note_pubkey_x)
{
    {
        join_split_tx tx = simple_setup();
        tx.output_note[0].creator_pubkey = user.owner.public_key.x;
        tx.output_note[1].creator_pubkey = user.owner.public_key.x;
        EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
    }
    {
        join_split_tx tx = simple_setup();
        tx.output_note[0].creator_pubkey = user.owner.public_key.x;
        EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
    }
    {
        join_split_tx tx = simple_setup();
        tx.output_note[1].creator_pubkey = user.owner.public_key.x;
        EXPECT_TRUE(sign_and_verify_logic(tx, user.owner.private_key));
    }
}

TEST_F(join_split_tests, test_incorrect_output_note_pubkey_x)
{
    {
        join_split_tx tx = simple_setup();
        tx.output_note[0].creator_pubkey = rollup::fixtures::create_key_pair(nullptr).public_key.x;
        EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
    }
    {
        join_split_tx tx = simple_setup();
        tx.output_note[1].creator_pubkey = rollup::fixtures::create_key_pair(nullptr).public_key.x;
        EXPECT_FALSE(sign_and_verify_logic(tx, user.owner.private_key));
    }
}

} // namespace join_split
} // namespace proofs
} // namespace rollup