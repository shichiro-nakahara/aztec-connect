#include <cstdint>

#define WASM_EXPORT __attribute__((visibility("default")))

extern "C" {

WASM_EXPORT void notes__sign_4_notes(uint8_t* pk_buffer,
                                     uint8_t const* output_owner_buffer,
                                     uint8_t const* note_buffer,
                                     uint8_t const* tx_fee_buffer,
                                     uint8_t* output);

WASM_EXPORT void notes__commit_value_note(uint8_t const* note_buffer, uint8_t* output);

WASM_EXPORT void notes__compute_value_note_nullifier(
    uint8_t const* enc_note_buffer, uint8_t* acc_pk_buffer, uint32_t index, bool is_real, uint8_t* output);

WASM_EXPORT void notes__create_partial_value_note(uint8_t const* note_buffer,
                                                  uint8_t* public_key_buffer,
                                                  uint32_t nonce,
                                                  uint8_t* output);

WASM_EXPORT void notes__commit_claim_note(uint8_t const* note_buffer, uint8_t* output);

WASM_EXPORT void notes__compute_claim_note_nullifier(uint8_t const* enc_note_buffer, uint32_t index, uint8_t* output);

WASM_EXPORT void notes__complete_partial_claim_note(uint8_t const* note_buffer,
                                                    uint32_t interaction_nonce,
                                                    uint8_t* output);

WASM_EXPORT void notes__commit_defi_interaction_note(uint8_t const* note_buffer, uint8_t* output);
}
