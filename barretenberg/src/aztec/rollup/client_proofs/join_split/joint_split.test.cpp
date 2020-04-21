#include "../../pedersen_note/pedersen_note.hpp"
#include "../../tx/user_context.hpp"
#include "join_split.hpp"
#include "sign_notes.hpp"
#include <common/streams.hpp>
#include <crypto/schnorr/schnorr.hpp>
#include <stdlib/merkle_tree/leveldb_store.hpp>
#include <gtest/gtest.h>

using namespace barretenberg;
using namespace plonk::stdlib;
using namespace plonk::stdlib::types::turbo;
using namespace rollup::client_proofs::join_split;

std::string create_leaf_data(grumpkin::g1::affine_element const& enc_note)
{
    std::vector<uint8_t> buf;
    write(buf, enc_note.x);
    write(buf, enc_note.y);
    return std::string(buf.begin(), buf.end());
}

class client_proofs_join_split : public ::testing::Test {
  protected:
    static void SetUpTestCase()
    {
        auto null_crs_factory = std::make_unique<waffle::ReferenceStringFactory>();
        init_proving_key(std::move(null_crs_factory));
        auto crs_factory = std::make_unique<waffle::FileReferenceStringFactory>("../srs_db");
        init_verification_key(std::move(crs_factory));
    }

    virtual void SetUp()
    {
        merkle_tree::LevelDbStore::destroy("/tmp/client_proofs_join_split_db");
        tree = std::make_unique<merkle_tree::LevelDbStore>("/tmp/client_proofs_join_split_db", 32);
        user = rollup::tx::create_user_context();

        tx_note note1 = { user.public_key, 100, user.note_secret };
        tx_note note2 = { user.public_key, 50, user.note_secret };

        auto enc_note1 = encrypt_note(note1);
        tree->update_element(0, create_leaf_data(enc_note1));

        auto enc_note2 = encrypt_note(note2);
        tree->update_element(1, create_leaf_data(enc_note2));
    }

    rollup::tx::user_context user;
    std::unique_ptr<merkle_tree::LevelDbStore> tree;
};

TEST_F(client_proofs_join_split, test_0_input_notes)
{
    join_split_tx tx;
    tx.owner_pub_key = user.public_key;
    tx.public_input = 30;
    tx.public_output = 0;
    tx.num_input_notes = 0;
    tx.input_index = { 0, 0 };
    tx.merkle_root = tree->root();
    tx.input_path = { merkle_tree::fr_hash_path(32), merkle_tree::fr_hash_path(32) };

    tx_note gibberish = { user.public_key, 123, fr::random_element() };
    tx.input_note = { gibberish, gibberish };

    tx_note output_note1 = { user.public_key, 20, user.note_secret };
    tx_note output_note2 = { user.public_key, 10, user.note_secret };
    tx.output_note = { output_note1, output_note2 };

    tx.signature =
        sign_notes({ gibberish, gibberish, output_note1, output_note2 }, { user.private_key, user.public_key });

    auto prover = new_join_split_prover(tx);
    auto proof = prover.construct_proof();
    auto verified = verify_proof(proof);

    EXPECT_TRUE(verified);
}

TEST_F(client_proofs_join_split, test_2_input_notes)
{
    join_split_tx tx;
    tx.owner_pub_key = user.public_key;
    tx.public_input = 0;
    tx.public_output = 0;
    tx.num_input_notes = 2;
    tx.input_index = { 0, 1 };
    tx.merkle_root = tree->root();
    tx.input_path = { tree->get_hash_path(0), tree->get_hash_path(1) };

    tx_note input_note1 = { user.public_key, 100, user.note_secret };
    tx_note input_note2 = { user.public_key, 50, user.note_secret };
    tx.input_note = { input_note1, input_note2 };

    tx_note output_note1 = { user.public_key, 70, user.note_secret };
    tx_note output_note2 = { user.public_key, 80, user.note_secret };
    tx.output_note = { output_note1, output_note2 };

    tx.signature =
        sign_notes({ input_note1, input_note2, output_note1, output_note2 }, { user.private_key, user.public_key });

    auto prover = new_join_split_prover(tx);
    auto proof = prover.construct_proof();
    auto verified = verify_proof(proof);

    EXPECT_TRUE(verified);
}