#include "claim_tx.hpp"
#include "../notes/native/claim/claim_note.hpp"
#include <stdlib/merkle_tree/hash_path.hpp>
#include <common/streams.hpp>
#include <gtest/gtest.h>

using namespace barretenberg;
using namespace rollup::proofs::claim;

TEST(client_proofs_claim_tx, test_serialization)
{
    claim_tx tx;
    tx.data_root = barretenberg::fr::random_element();
    tx.claim_note_index = 1;
    tx.claim_note_path = merkle_tree::fr_hash_path(32, std::make_pair(fr::random_element(), fr::random_element()));

    tx.claim_note.bridge_id = 123;
    tx.claim_note.defi_interaction_nonce = 234;
    tx.claim_note.deposit_value = 345;
    tx.claim_note.partial_state = grumpkin::g1::element::random_element();

    tx.defi_interaction_note_path =
        merkle_tree::fr_hash_path(32, std::make_pair(fr::random_element(), fr::random_element()));

    tx.defi_interaction_note.bridge_id = 456;
    tx.defi_interaction_note.interaction_nonce = 567;
    tx.defi_interaction_note.total_input_value = 678;
    tx.defi_interaction_note.total_output_a_value = 789;
    tx.defi_interaction_note.total_output_b_value = 890;
    tx.defi_interaction_note.interaction_result = 1;

    tx.output_value_a = 888;
    tx.output_value_b = 999;

    auto buffer = to_buffer(tx);
    auto tx2 = from_buffer<claim_tx>(buffer.data());

    EXPECT_EQ(tx, tx2);
}
