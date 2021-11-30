#include "join_split_circuit.hpp"
#include "../../constants.hpp"
#include "../notes/circuit/value/value_note.hpp"
#include "../notes/circuit/account/account_note.hpp"
#include "../notes/circuit/claim/claim_note.hpp"
#include "../notes/circuit/compute_nullifier.hpp"
#include "verify_signature.hpp"
#include <stdlib/merkle_tree/membership.hpp>
#include <stdlib/primitives/field/pow.hpp>

// #pragma GCC diagnostic ignored "-Wunused-variable"
// #pragma GCC diagnostic ignored "-Wunused-parameter"
namespace rollup {
namespace proofs {
namespace join_split {

using namespace plonk;
using namespace notes::circuit;
using namespace plonk::stdlib::merkle_tree;

/**
 * Check that the input note data, follows the given hash paths, to the publically given merkle root.
 * The note does not need to exist in the tree if it's not real, or if it's consumed (i.e. propagated = input).
 * Return the nullifier for the input note. If the input note is consumed, the nullifier becomes 0.
 */
field_ct process_input_note(field_ct const& account_private_key,
                            field_ct const& merkle_root,
                            merkle_tree::hash_path const& hash_path,
                            field_ct const& index,
                            value::value_note const& note,
                            bool_ct is_propagated,
                            bool_ct is_real)
{
    bool_ct valid_value = note.value == 0 || is_real;
    valid_value.assert_equal(true, "padding note non zero");

    auto exists = merkle_tree::check_membership(merkle_root, hash_path, note.commitment, byte_array_ct(index));
    auto valid = exists || is_propagated || !is_real;
    valid.assert_equal(true, "input note not a member");

    return compute_nullifier(note.commitment, account_private_key, is_real);
}

join_split_outputs join_split_circuit_component(join_split_inputs const& inputs)
{
    auto is_deposit = inputs.proof_id == field_ct(ProofIds::DEPOSIT);
    auto is_withdraw = inputs.proof_id == field_ct(ProofIds::WITHDRAW);
    auto is_send = inputs.proof_id == field_ct(ProofIds::SEND);
    auto is_public_tx = is_deposit || is_withdraw;
    auto is_defi_deposit = inputs.proof_id == field_ct(ProofIds::DEFI_DEPOSIT);
    auto not_defi_deposit = !is_defi_deposit;
    auto input_note1 = value::value_note(inputs.input_note1);
    auto input_note2 = value::value_note(inputs.input_note2);
    auto output_note1 = value::value_note(inputs.output_note1);
    auto output_note2 = value::value_note(inputs.output_note2);
    auto claim_note = claim::partial_claim_note(inputs.claim_note, inputs.input_note1.owner, inputs.input_note1.nonce);
    auto output_note1_commitment =
        field_ct::conditional_assign(is_defi_deposit, claim_note.partial_commitment, output_note1.commitment);
    auto public_asset_id = inputs.asset_id * is_public_tx;
    auto public_input = inputs.public_value * is_deposit;
    auto public_output = inputs.public_value * is_withdraw;
    auto defi_deposit_value = inputs.claim_note.deposit_value * is_defi_deposit;
    auto bridge_id = claim_note.bridge_id * is_defi_deposit;
    auto inote1_valid = inputs.num_input_notes == 1 || inputs.num_input_notes == 2;
    auto inote2_valid = inputs.num_input_notes == 2;
    auto inote1_value = input_note1.value;
    auto inote2_value = input_note2.value;
    auto onote1_value = output_note1.value * not_defi_deposit;
    auto onote2_value = output_note2.value;

    // Input data range contraints.
    inputs.alias_hash.create_range_constraint(224, "alias hash too large");
    inputs.public_value.create_range_constraint(NOTE_VALUE_BIT_LENGTH, "public value too large");
    inputs.asset_id.create_range_constraint(ASSET_ID_BIT_LENGTH, "asset id too large");
    inputs.input_note1_index.create_range_constraint(DATA_TREE_DEPTH, "input note 1 index too large");
    inputs.input_note2_index.create_range_constraint(DATA_TREE_DEPTH, "input note 2 index too large");
    inputs.account_index.create_range_constraint(DATA_TREE_DEPTH, "account note index too large");

    // Check public value and owner are not zero for deposit and withdraw, otherwise they must be zero.
    (is_public_tx == inputs.public_value.is_zero()).assert_equal(false, "public value incorrect");
    (is_public_tx == inputs.public_owner.is_zero()).assert_equal(false, "public owner incorrect");

    // Circuit operates in one of several cases. Assert we're only in one of these cases and rules apply.
    {
        // Case 0: 0 input notes, all notes have same asset ids, can only DEPOSIT.
        const auto case0 = !inote1_valid && !inote2_valid;
        // Case 1: 1 real asset note, all notes have same asset ids, any function.
        const auto case1 = !input_note1.is_virtual && inote1_valid && !inote2_valid;
        // Case 2: 2 real asset notes, all notes have same asset ids, any function.
        const auto case2 = !input_note1.is_virtual && !input_note2.is_virtual && inote2_valid;
        // Case 3: 1 virtual asset note, all notes have same asset ids, can only SEND.
        const auto case3 = input_note1.is_virtual && !inote2_valid;
        // Case 4: 2 virtual asset notes, all notes have same asset ids, can only SEND.
        const auto case4 = input_note1.is_virtual && input_note2.is_virtual && inote2_valid;
        // Case 5: 1st note real, 2nd note virtual, different input asset ids allowed, values equal, can only
        // DEFI_DEPOSIT, virtual notes interaction nonce must match that in the bridge id.
        const auto case5 = !input_note1.is_virtual && input_note2.is_virtual;

        // Check we are exactly one of the defined cases.
        (field_ct(case0) + case1 + case2 + case3 + case4 + case5).assert_equal(1, "unsupported case");

        // Assert case rules.
        const auto onote1_aid = field_ct::conditional_assign(
            is_defi_deposit, inputs.claim_note.bridge_id_data.input_asset_id, inputs.output_note1.asset_id);
        const auto all_asset_ids_match =
            input_note1.asset_id == input_note2.asset_id && input_note1.asset_id == onote1_aid &&
            input_note1.asset_id == output_note2.asset_id && input_note1.asset_id == inputs.asset_id;
        (case0 || case1 || case2 || case3 || case4).must_imply(all_asset_ids_match, "asset ids don't match");
        (case1 || case2).must_imply(is_deposit || is_send || is_withdraw || is_defi_deposit, "unknown function");
        (case0).must_imply(is_deposit, "can only deposit");
        (case3 || case4).must_imply(is_send, "can only send");

        case5.must_imply(is_defi_deposit, "can only defi deposit");
        case5.must_imply(inote1_value == inote2_value, "input note values must match");
        case5.must_imply(input_note1.asset_id == onote1_aid && input_note1.asset_id == output_note2.asset_id,
                         "asset ids don't match");
        case5.must_imply(inputs.claim_note.bridge_id_data.opening_nonce == input_note2.virtual_note_nonce,
                         "incorrect interaction nonce in bridge id");

        // Don't consider second note value for case5 in the input/output balancing equations.
        inote2_value *= !case5;
    }

    // Check we're not joining the same input note.
    input_note1.commitment.assert_not_equal(input_note2.commitment, "joining same note");

    // Transaction chaining.
    bool_ct note1_propagated = inputs.propagated_input_index == 1;
    bool_ct note2_propagated = inputs.propagated_input_index == 2;
    {
        // propagated_input_index must be in {0, 1, 2}
        bool_ct no_note_propagated = inputs.propagated_input_index == 0;
        (no_note_propagated || note1_propagated || note2_propagated)
            .assert_equal(true, "propagated_input_index out of range");

        // allow_chain must be in {0, 1, 2, 3}
        bool_ct allow_chain_1_and_2 = inputs.allow_chain == 3;
        bool_ct allow_chain_1 = inputs.allow_chain == 1 || allow_chain_1_and_2;
        bool_ct allow_chain_2 = inputs.allow_chain == 2 || allow_chain_1_and_2;

        (inputs.allow_chain == 0 || allow_chain_1 || allow_chain_2).assert_equal(true, "allow_chain out of range");

        // Prevent chaining from a partial claim note.
        is_defi_deposit.must_imply(!allow_chain_1, "cannot chain from a partial claim note");

        bool_ct note1_linked = inputs.backward_link == input_note1.commitment;
        bool_ct note2_linked = inputs.backward_link == input_note2.commitment;
        note1_propagated.must_imply(note1_linked, "inconsistent backward_link & propagated_input_index");
        note2_propagated.must_imply(note2_linked, "inconsistent backward_link & propagated_input_index");
        (!note1_linked && !note2_linked)
            .must_imply(no_note_propagated, "inconsistent backward_link & propagated_input_index");

        // When allowing chaining, ensure propagation is to one's self (and not to some other user).
        point_ct self = input_note1.owner;
        allow_chain_1.must_imply(output_note1.owner == self, "inter-user chaining disallowed");
        allow_chain_2.must_imply(output_note2.owner == self, "inter-user chaining disallowed");
    }

    // Derive tx_fee.
    field_ct total_in_value = public_input + inote1_value + inote2_value;
    field_ct total_out_value = public_output + onote1_value + onote2_value + defi_deposit_value;
    field_ct tx_fee = total_in_value - total_out_value;
    tx_fee.create_range_constraint(TX_FEE_BIT_LENGTH, "tx fee too large");

    // Verify input notes have the same account public key and nonce.
    input_note1.owner.assert_equal(input_note2.owner, "input note owners don't match");
    input_note1.nonce.assert_equal(input_note2.nonce, "input note nonces don't match");

    // And thus check both input notes have the correct public key (derived from the private key) and nonce.
    auto account_public_key = group_ct::fixed_base_scalar_mul<254>(inputs.account_private_key);
    account_public_key.assert_equal(input_note1.owner, "account_private_key incorrect");
    inputs.nonce.assert_equal(input_note1.nonce, "nonce incorrect");

    // Verify output notes creator_pubkey is either account_public_key.x or 0.
    account_public_key.x.assert_equal(
        account_public_key.x.madd(output_note1.creator_pubkey.is_zero(), output_note1.creator_pubkey),
        "output note 1 creator_pubkey mismatch");
    account_public_key.x.assert_equal(
        account_public_key.x.madd(output_note2.creator_pubkey.is_zero(), output_note2.creator_pubkey),
        "output note 2 creator_pubkey mismatch");

    // Signer is the account public key if account nonce is 0, else it's the given signing key.
    bool_ct zero_nonce = inputs.nonce.is_zero();
    point_ct signer = { field_ct::conditional_assign(zero_nonce, account_public_key.x, inputs.signing_pub_key.x),
                        field_ct::conditional_assign(zero_nonce, account_public_key.y, inputs.signing_pub_key.y) };

    // Verify that the signing key account note exists if nonce > 0.
    {
        auto account_alias_id = inputs.alias_hash + (inputs.nonce * field_ct(uint256_t(1) << 224));
        auto account_note_data = account::account_note(account_alias_id, account_public_key, signer);
        auto signing_key_exists = merkle_tree::check_membership(
            inputs.merkle_root, inputs.account_path, account_note_data.commitment, byte_array_ct(inputs.account_index));
        (signing_key_exists || zero_nonce).assert_equal(true, "account check_membership failed");
    }

    // Verify each input note exists in the tree, and compute nullifiers.
    field_ct nullifier1 = process_input_note(inputs.account_private_key,
                                             inputs.merkle_root,
                                             inputs.input_path1,
                                             inputs.input_note1_index,
                                             input_note1,
                                             note1_propagated,
                                             inote1_valid);
    field_ct nullifier2 = process_input_note(inputs.account_private_key,
                                             inputs.merkle_root,
                                             inputs.input_path2,
                                             inputs.input_note2_index,
                                             input_note2,
                                             note2_propagated,
                                             inote2_valid);

    // Assert that input nullifiers in the output note commitments equal the input note nullifiers.
    output_note1.input_nullifier.assert_equal(nullifier1, "output note 1 has incorrect input nullifier");
    output_note2.input_nullifier.assert_equal(nullifier2, "output note 2 has incorrect input nullifier");
    claim_note.input_nullifier.assert_equal(nullifier1 * is_defi_deposit, "claim note has incorrect input nullifier");

    verify_signature(inputs.public_value,
                     inputs.public_owner,
                     public_asset_id,
                     output_note1_commitment,
                     output_note2.commitment,
                     nullifier1,
                     nullifier2,
                     signer,
                     inputs.propagated_input_index,
                     inputs.backward_link,
                     inputs.allow_chain,
                     inputs.signature);

    return { nullifier1, nullifier2, output_note1_commitment, output_note2.commitment, public_asset_id,
             tx_fee,     bridge_id,  defi_deposit_value };
}

void join_split_circuit(Composer& composer, join_split_tx const& tx)
{
    join_split_inputs inputs = {
        witness_ct(&composer, tx.proof_id),
        witness_ct(&composer, tx.public_value),
        witness_ct(&composer, tx.public_owner),
        witness_ct(&composer, tx.asset_id),
        witness_ct(&composer, tx.num_input_notes),
        witness_ct(&composer, tx.input_index[0]),
        witness_ct(&composer, tx.input_index[1]),
        value::witness_data(composer, tx.input_note[0]),
        value::witness_data(composer, tx.input_note[1]),
        value::witness_data(composer, tx.output_note[0]),
        value::witness_data(composer, tx.output_note[1]),
        claim::claim_note_tx_witness_data(composer, tx.claim_note),
        { witness_ct(&composer, tx.signing_pub_key.x), witness_ct(&composer, tx.signing_pub_key.y) },
        stdlib::schnorr::convert_signature(&composer, tx.signature),
        witness_ct(&composer, tx.old_data_root),
        merkle_tree::create_witness_hash_path(composer, tx.input_path[0]),
        merkle_tree::create_witness_hash_path(composer, tx.input_path[1]),
        witness_ct(&composer, tx.account_index),
        merkle_tree::create_witness_hash_path(composer, tx.account_path),
        witness_ct(&composer, static_cast<fr>(tx.account_private_key)),
        witness_ct(&composer, tx.alias_hash),
        witness_ct(&composer, tx.nonce),
        witness_ct(&composer, tx.propagated_input_index),
        witness_ct(&composer, tx.backward_link),
        witness_ct(&composer, tx.allow_chain),
    };

    auto outputs = join_split_circuit_component(inputs);

    const field_ct defi_root = witness_ct(&composer, 0);
    defi_root.assert_is_zero();

    // The following make up the public inputs to the circuit.
    inputs.proof_id.set_public();
    outputs.output_note1.set_public();
    outputs.output_note2.set_public();
    outputs.nullifier1.set_public();
    outputs.nullifier2.set_public();
    inputs.public_value.set_public();
    inputs.public_owner.set_public();
    outputs.public_asset_id.set_public();

    // Any public witnesses exposed from here on, will not be exposed by the rollup, and thus will
    // not be part of the calldata on chain, and will also not be part of tx id generation, or be signed over.
    inputs.merkle_root.set_public();
    outputs.tx_fee.set_public();
    inputs.asset_id.set_public();
    outputs.bridge_id.set_public();
    outputs.defi_deposit_value.set_public();
    defi_root.set_public();
    inputs.propagated_input_index.set_public();
    inputs.backward_link.set_public();
    inputs.allow_chain.set_public();
}

} // namespace join_split
} // namespace proofs
} // namespace rollup
