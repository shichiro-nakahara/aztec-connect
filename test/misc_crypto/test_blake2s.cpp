#include <gtest/gtest.h>

#include <barretenberg/curves/bn254/fr.hpp>
#include <barretenberg/misc_crypto/blake2s/blake2s.hpp>

#include <iostream>
#include <memory>
#include <vector>

struct test_vector {
    std::string input;
    std::vector<uint8_t> output;
};

test_vector test_vectors[] = {
    { "",
      {
          0x69, 0x21, 0x7A, 0x30, 0x79, 0x90, 0x80, 0x94, 0xE1, 0x11, 0x21, 0xD0, 0x42, 0x35, 0x4A, 0x7C,
          0x1F, 0x55, 0xB6, 0x48, 0x2C, 0xA1, 0xA5, 0x1E, 0x1B, 0x25, 0x0D, 0xFD, 0x1E, 0xD0, 0xEE, 0xF9,
      } },
    { "a",
      {
          0x4A, 0x0D, 0x12, 0x98, 0x73, 0x40, 0x30, 0x37, 0xC2, 0xCD, 0x9B, 0x90, 0x48, 0x20, 0x36, 0x87,
          0xF6, 0x23, 0x3F, 0xB6, 0x73, 0x89, 0x56, 0xE0, 0x34, 0x9B, 0xD4, 0x32, 0x0F, 0xEC, 0x3E, 0x90,
      } },
    { "ab",
      {
          0x19, 0xC3, 0xEB, 0xEE, 0xD2, 0xEE, 0x90, 0x06, 0x3C, 0xB5, 0xA8, 0xA4, 0xDD, 0x70, 0x0E, 0xD7,
          0xE5, 0x85, 0x2D, 0xFC, 0x61, 0x08, 0xC8, 0x4F, 0xAC, 0x85, 0x88, 0x86, 0x82, 0xA1, 0x8F, 0x0E,
      } },
    { "abc",
      {
          0x50, 0x8C, 0x5E, 0x8C, 0x32, 0x7C, 0x14, 0xE2, 0xE1, 0xA7, 0x2B, 0xA3, 0x4E, 0xEB, 0x45, 0x2F,
          0x37, 0x45, 0x8B, 0x20, 0x9E, 0xD6, 0x3A, 0x29, 0x4D, 0x99, 0x9B, 0x4C, 0x86, 0x67, 0x59, 0x82,
      } },
    { "abcd",
      {
          0x71, 0x67, 0x48, 0xCC, 0xE9, 0x7A, 0x0A, 0xBC, 0x94, 0x2E, 0x1D, 0x49, 0x1B, 0xC2, 0x51, 0x02,
          0xF5, 0xB6, 0xFF, 0x71, 0xEE, 0x62, 0xA8, 0x6A, 0xBD, 0x60, 0x5A, 0x6C, 0x40, 0x12, 0x01, 0x69,
      } },
    { "abcde",
      {
          0x4B, 0xD7, 0x24, 0x6C, 0x13, 0x72, 0x1C, 0xC5, 0xB9, 0x6F, 0x04, 0x5B, 0xE7, 0x1D, 0x49, 0xD5,
          0xC8, 0x25, 0x35, 0x33, 0x2C, 0x69, 0x03, 0x77, 0x1A, 0xFE, 0x9E, 0xF7, 0xB7, 0x72, 0x13, 0x6F,
      } },
    { "abcdef",
      {
          0x26, 0x7E, 0x44, 0x43, 0xFC, 0x1A, 0x38, 0x87, 0x9F, 0xEB, 0x10, 0x90, 0xAF, 0x1E, 0x78, 0x89,
          0x56, 0xDF, 0xD9, 0x32, 0x04, 0xCD, 0xDC, 0xBA, 0x81, 0x8D, 0x6E, 0x32, 0xEE, 0x57, 0xF3, 0x35,
      } },
    { "abcdefg",
      {
          0x44, 0x68, 0xA2, 0xB5, 0x32, 0x92, 0x24, 0xC5, 0x4C, 0x24, 0x3D, 0x4F, 0xAE, 0x24, 0xCD, 0xF0,
          0x50, 0xA2, 0x7A, 0x56, 0x34, 0x80, 0xF4, 0xAF, 0x5F, 0xED, 0xD4, 0x44, 0x6D, 0x8C, 0x4A, 0x04,
      } },
    { "abcdefgh",
      {
          0xDA, 0x65, 0x1C, 0x96, 0x5D, 0x6B, 0x93, 0xCA, 0x76, 0x11, 0xC9, 0xE9, 0x96, 0xFB, 0x8C, 0x15,
          0xA2, 0x50, 0xB3, 0x52, 0x06, 0xD1, 0x37, 0x63, 0xFF, 0x3D, 0x53, 0x85, 0x1E, 0xEF, 0x55, 0xF9,
      } },
    { "abcdefghi",
      {
          0x7E, 0xA3, 0x77, 0xE8, 0x89, 0x82, 0xE9, 0x07, 0x24, 0xAA, 0x8E, 0x2C, 0xB5, 0xC9, 0xB0, 0x86,
          0x71, 0x3C, 0x89, 0x16, 0x1D, 0x5C, 0xF6, 0x17, 0x8F, 0x31, 0xE4, 0x3A, 0xF4, 0xEB, 0x35, 0x04,
      } },
    { "abcdefghij",
      {
          0xCF, 0x49, 0xAE, 0x6D, 0xAC, 0x01, 0xA2, 0x0A, 0xC8, 0x7F, 0x50, 0x44, 0xF9, 0xEB, 0x26, 0xD7,
          0x60, 0xDF, 0xC1, 0x67, 0x04, 0x54, 0xF6, 0xA5, 0x2F, 0xF9, 0xE4, 0x6D, 0xF6, 0x91, 0xD5, 0x56,
      } },
    { "abcdefghijk",
      {
          0xE0, 0x3E, 0x25, 0x42, 0xDE, 0xDF, 0x65, 0x9C, 0x14, 0x7D, 0x75, 0x07, 0xE9, 0xFA, 0x77, 0xF1,
          0xAC, 0xBB, 0x1A, 0x17, 0xAF, 0x74, 0x76, 0x60, 0x86, 0x48, 0xDE, 0x21, 0x52, 0xAE, 0x26, 0xF8,
      } },
    { "abcdefghijkl",
      {
          0x40, 0x12, 0x51, 0xAD, 0x13, 0x78, 0x11, 0xC9, 0x41, 0xDB, 0x66, 0xBA, 0x4B, 0x3D, 0x2E, 0xC1,
          0xA1, 0x6D, 0x21, 0xD9, 0xB8, 0x61, 0xD1, 0x11, 0xCF, 0xD1, 0x33, 0xCB, 0x3D, 0xFB, 0x00, 0x48,
      } },
    { "abcdefghijklm",
      {
          0xB7, 0x09, 0xA0, 0x2D, 0xD0, 0xFF, 0xEE, 0x06, 0x07, 0x8F, 0x1D, 0x6D, 0x10, 0xEC, 0x62, 0x6C,
          0xE9, 0x13, 0xC6, 0x50, 0x8F, 0xEE, 0x29, 0xD9, 0x1B, 0x4C, 0xB5, 0x2F, 0x9A, 0xE4, 0x4D, 0xB9,
      } },
    { "abcdefghijklmn",
      {
          0x81, 0xEF, 0xCC, 0xF8, 0xF3, 0xA0, 0x63, 0x31, 0x44, 0xBD, 0xBE, 0x11, 0x27, 0xE8, 0x71, 0xED,
          0x60, 0x57, 0xB9, 0x1F, 0x66, 0x70, 0xAE, 0xF7, 0xA1, 0xD4, 0xF5, 0xC0, 0x0A, 0xF3, 0xD6, 0xE3,
      } },
    { "abcdefghijklmno",
      {
          0xF9, 0x87, 0xBD, 0xE3, 0x8F, 0xD6, 0x47, 0x79, 0xF7, 0xE9, 0xC3, 0x8C, 0x98, 0xAF, 0xD4, 0xE2,
          0x56, 0xB1, 0x58, 0xDC, 0x7B, 0x18, 0x3C, 0x63, 0x8C, 0x13, 0xE2, 0xB3, 0xD1, 0x96, 0xF6, 0xA5,
      } },
    { "abcdefghijklmnop",
      {
          0xB6, 0x77, 0x5F, 0xD6, 0x8A, 0x7B, 0x03, 0xF1, 0x77, 0x42, 0x6A, 0x0E, 0xF1, 0xAC, 0xEF, 0x97,
          0xAC, 0x07, 0x0A, 0xE0, 0xD3, 0x30, 0xBB, 0x46, 0x2E, 0xBB, 0x52, 0x93, 0x16, 0xA6, 0x1C, 0xF7,
      } },
    { "abcdefghijklmnopq",
      {
          0x13, 0x2C, 0xB5, 0x71, 0xFB, 0xAD, 0xF7, 0x05, 0x50, 0x7F, 0x01, 0x9C, 0x20, 0x31, 0xEE, 0x66,
          0xE7, 0xC3, 0x12, 0xF2, 0x37, 0x8F, 0x08, 0x93, 0x2A, 0x3F, 0x24, 0xC2, 0x47, 0xA4, 0x9D, 0xC5,
      } },
    { "abcdefghijklmnopqr",
      {
          0x02, 0x02, 0x93, 0xAE, 0xC4, 0x30, 0x76, 0x5B, 0x1F, 0x20, 0x4A, 0xB3, 0x8D, 0xC8, 0x8B, 0x28,
          0x75, 0x16, 0xC8, 0xA9, 0xE6, 0xE2, 0x0A, 0xB6, 0x2B, 0xC7, 0xE5, 0x1D, 0xF9, 0x33, 0xC6, 0x23,
      } },
    { "abcdefghijklmnopqrs",
      {
          0xBD, 0x04, 0x5D, 0x76, 0xAF, 0xC2, 0xB1, 0x68, 0x7B, 0x0B, 0xCA, 0x64, 0xEA, 0x18, 0xF7, 0x6B,
          0xEB, 0x80, 0x85, 0xCB, 0x89, 0xCA, 0x27, 0xD0, 0x6D, 0xF1, 0x7A, 0x9A, 0x49, 0xFE, 0x18, 0xBC,
      } },
    { "abcdefghijklmnopqrst",
      {
          0x63, 0x96, 0xFC, 0x1A, 0xF8, 0xC3, 0x7D, 0x41, 0x10, 0x9C, 0x77, 0x82, 0x98, 0x08, 0xDE, 0x85,
          0x61, 0xE4, 0x6C, 0xB3, 0xEF, 0x5A, 0x30, 0x56, 0xCA, 0xDC, 0xA0, 0x5E, 0x66, 0x8B, 0xDD, 0xE6,
      } },
    { "abcdefghijklmnopqrstu",
      {
          0x25, 0xB9, 0x54, 0x49, 0x5B, 0xD7, 0x53, 0xDA, 0x17, 0x99, 0x01, 0xE4, 0xBF, 0xD8, 0x13, 0x8C,
          0xA9, 0x86, 0x89, 0x14, 0x57, 0xF5, 0x8F, 0xF1, 0x6D, 0x1F, 0x7D, 0xA7, 0xDD, 0x39, 0x4D, 0x86,
      } },
    { "abcdefghijklmnopqrstuv",
      {
          0xCB, 0xC0, 0xCA, 0x9D, 0x40, 0x62, 0xC1, 0x0C, 0x40, 0x3C, 0xC0, 0x61, 0x1D, 0x2F, 0xA8, 0x68,
          0x09, 0x5C, 0x7E, 0xFE, 0x02, 0x2C, 0x84, 0x47, 0x5C, 0xBF, 0x8B, 0x3F, 0x6B, 0x98, 0xD6, 0x8D,
      } },
    { "abcdefghijklmnopqrstuvw",
      {
          0x5A, 0x51, 0xB7, 0x9F, 0xF2, 0x66, 0xAF, 0xE9, 0x16, 0xA7, 0x5D, 0x87, 0x37, 0xFC, 0xE5, 0x8F,
          0x60, 0x2C, 0x9D, 0xD7, 0x09, 0x9F, 0x5A, 0x8B, 0xB6, 0x12, 0x1B, 0x3A, 0x49, 0xEE, 0x2D, 0xC6,
      } },
    { "abcdefghijklmnopqrstuvwx",
      {
          0x9C, 0x04, 0x32, 0x4F, 0xD5, 0xD4, 0xB0, 0x98, 0x87, 0xB0, 0xFE, 0x71, 0x1C, 0xC5, 0xAD, 0x85,
          0xA7, 0xED, 0xC7, 0x12, 0xF7, 0xDA, 0x09, 0x01, 0x1A, 0x97, 0x0A, 0x11, 0x35, 0x2C, 0x04, 0xCA,
      } },
    { "abcdefghijklmnopqrstuvwxy",
      {
          0xA5, 0x16, 0xD0, 0x91, 0x2F, 0xDB, 0x3E, 0x4A, 0x1E, 0x76, 0xB5, 0x03, 0xD4, 0xF0, 0xB9, 0xB8,
          0x62, 0x08, 0xFD, 0xFC, 0x31, 0xC1, 0xFC, 0xA8, 0x00, 0xBD, 0xFF, 0x4E, 0xF0, 0xE9, 0x95, 0xA2,
      } },
    { "abcdefghijklmnopqrstuvwxyz",
      {
          0xBD, 0xF8, 0x8E, 0xB1, 0xF8, 0x6A, 0x0C, 0xDF, 0x0E, 0x84, 0x0B, 0xA8, 0x8F, 0xA1, 0x18, 0x50,
          0x83, 0x69, 0xDF, 0x18, 0x6C, 0x73, 0x55, 0xB4, 0xB1, 0x6C, 0xF7, 0x9F, 0xA2, 0x71, 0x0A, 0x12,
      } },
    { "abcdefghijklmnopqrstuvwxyz0",
      {
          0xD9, 0xFC, 0xBA, 0x8D, 0x13, 0x8E, 0x33, 0x1B, 0x17, 0xB9, 0xE1, 0x9F, 0xAB, 0xF9, 0xE6, 0xF7,
          0xDA, 0xB8, 0x67, 0xE9, 0x58, 0x01, 0xC7, 0xE5, 0x5E, 0x33, 0x28, 0x9A, 0x52, 0x7E, 0x0A, 0x5E,
      } },
    { "abcdefghijklmnopqrstuvwxyz01",
      {
          0x3C, 0x21, 0x03, 0x02, 0x8D, 0x16, 0x41, 0xAC, 0x7C, 0x59, 0x03, 0x04, 0xB0, 0x77, 0xE6, 0xDF,
          0xA4, 0x37, 0xC9, 0xA3, 0x51, 0xDE, 0xDA, 0x25, 0xE1, 0x48, 0x1C, 0x99, 0x75, 0xCE, 0x56, 0xF8,
      } },
    { "abcdefghijklmnopqrstuvwxyz012",
      {
          0x7E, 0x9D, 0xA9, 0x33, 0xB3, 0x2D, 0x3A, 0xDB, 0x54, 0xCD, 0x28, 0x0A, 0xED, 0x0E, 0x1A, 0x5E,
          0xCF, 0x6C, 0xD6, 0x32, 0x2B, 0x13, 0xEF, 0xCF, 0xCF, 0x66, 0xCD, 0xD5, 0xA0, 0x49, 0x37, 0x68,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123",
      {
          0x86, 0x8F, 0x75, 0xCE, 0x0A, 0x86, 0xF5, 0xD0, 0x78, 0x6E, 0x44, 0x59, 0xD1, 0x28, 0x5D, 0xAC,
          0xE0, 0xEF, 0x6D, 0x7B, 0x1E, 0x48, 0x56, 0x85, 0x85, 0xC0, 0x59, 0x8D, 0xAA, 0xFE, 0x37, 0x96,
      } },
    { "abcdefghijklmnopqrstuvwxyz01234",
      {
          0x75, 0xB7, 0xA1, 0x49, 0x4A, 0x29, 0xC6, 0xA4, 0x43, 0x9E, 0xA7, 0xB4, 0x95, 0xCA, 0xD6, 0x3E,
          0x7B, 0xDA, 0x7B, 0xC7, 0x08, 0xC0, 0xA4, 0x0D, 0x53, 0x82, 0xF6, 0x60, 0xCD, 0x70, 0x12, 0x68,
      } },
    { "abcdefghijklmnopqrstuvwxyz012345",
      {
          0xC0, 0xB8, 0x2A, 0x49, 0x81, 0xAD, 0x7B, 0xCC, 0xAD, 0x87, 0x36, 0x35, 0x40, 0x0A, 0x25, 0x7C,
          0x74, 0x73, 0xA8, 0x05, 0x26, 0xF3, 0xAB, 0xA9, 0xE1, 0x31, 0xEE, 0x48, 0x72, 0x34, 0x3C, 0x70,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456",
      {
          0x8A, 0x5A, 0x17, 0x99, 0x30, 0x27, 0xDF, 0xD9, 0xBB, 0x52, 0xD7, 0x7E, 0x99, 0x78, 0x03, 0x27,
          0xE3, 0x0B, 0xFB, 0x56, 0x58, 0xE6, 0x0A, 0xB7, 0x27, 0x3E, 0xDE, 0x0E, 0x97, 0x70, 0x30, 0x20,
      } },
    { "abcdefghijklmnopqrstuvwxyz01234567",
      {
          0x2D, 0x44, 0x7F, 0x36, 0x56, 0xCB, 0x42, 0xC9, 0x58, 0x08, 0x08, 0x90, 0x2E, 0xEC, 0x3A, 0x42,
          0x8F, 0x9D, 0x5E, 0xB2, 0xC9, 0xE6, 0x6D, 0x60, 0x4F, 0xAA, 0x3A, 0x19, 0xD1, 0xAB, 0xE9, 0x01,
      } },
    { "abcdefghijklmnopqrstuvwxyz012345678",
      {
          0x07, 0x5E, 0xEE, 0x56, 0xAD, 0x75, 0x27, 0x14, 0x52, 0x5B, 0xBC, 0x27, 0x3A, 0x5E, 0x85, 0x9A,
          0xC5, 0x92, 0xAF, 0xDA, 0xE4, 0xF6, 0x80, 0x48, 0x41, 0x29, 0x43, 0xCA, 0xB1, 0x72, 0x47, 0x88,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789",
      {
          0x8A, 0x77, 0xAC, 0xF4, 0xD3, 0xAC, 0xBF, 0x62, 0x3D, 0x10, 0x6A, 0x32, 0x20, 0xB1, 0x37, 0x49,
          0x7C, 0xAA, 0x95, 0xE4, 0xCD, 0x3E, 0x58, 0xD5, 0x3C, 0x51, 0xCD, 0xBF, 0x8C, 0x87, 0x40, 0x59,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789a",
      {
          0x45, 0x2D, 0x83, 0x73, 0x7F, 0xB3, 0xDC, 0x32, 0x7D, 0x61, 0x98, 0xE2, 0xB4, 0x5A, 0xA0, 0xE9,
          0x9F, 0x95, 0x14, 0x0A, 0xAB, 0x94, 0xED, 0x94, 0xE5, 0x54, 0xF3, 0x84, 0xC5, 0x2E, 0x4B, 0xF2,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789ab",
      {
          0xE6, 0xD1, 0xC4, 0x5F, 0xC9, 0x58, 0x95, 0xF8, 0x16, 0x9E, 0x71, 0xEA, 0x44, 0x57, 0xA6, 0x37,
          0xBE, 0x89, 0xE9, 0xF8, 0xC0, 0x51, 0x60, 0x69, 0x6D, 0xA7, 0x47, 0x42, 0x05, 0xC7, 0xA4, 0xA6,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abc",
      {
          0x17, 0x0D, 0x88, 0x23, 0xED, 0xBD, 0x0C, 0xAB, 0xD9, 0x3E, 0x7F, 0xD1, 0x09, 0xA6, 0x94, 0xFF,
          0xC1, 0x96, 0x67, 0x47, 0x41, 0xB5, 0x39, 0xC2, 0x90, 0x9F, 0x34, 0x28, 0x45, 0x57, 0xB6, 0x58,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcd",
      {
          0x69, 0xC2, 0x25, 0x5B, 0x75, 0x43, 0x2B, 0x45, 0x8C, 0x73, 0xB3, 0xF0, 0x8D, 0x35, 0x7E, 0x7A,
          0x81, 0x6D, 0x0D, 0x5F, 0xF2, 0xE3, 0x41, 0xDF, 0x6F, 0x49, 0xE4, 0x88, 0xDD, 0x54, 0x4D, 0x5C,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcde",
      {
          0x7C, 0x5C, 0x8D, 0x25, 0xE7, 0xC6, 0xDB, 0x89, 0x92, 0x39, 0xF9, 0x85, 0x13, 0x1E, 0x7D, 0x72,
          0xCC, 0xBD, 0x4F, 0x66, 0x87, 0x15, 0x1F, 0x2F, 0x1B, 0xD9, 0xD7, 0xE0, 0x56, 0xE6, 0x8D, 0x4D,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdef",
      {
          0xAF, 0x5C, 0x76, 0xCF, 0x37, 0x68, 0x28, 0x3D, 0x40, 0x7B, 0x59, 0x80, 0x47, 0x3D, 0x93, 0xF7,
          0xE2, 0xF4, 0x39, 0x60, 0xCD, 0x8A, 0x50, 0x18, 0x12, 0x67, 0xA6, 0x38, 0x00, 0x19, 0xE0, 0x87,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefg",
      {
          0xF7, 0x5B, 0x7F, 0x99, 0x4A, 0x33, 0x94, 0x53, 0x8D, 0x2E, 0x08, 0xF5, 0x4E, 0x7A, 0xD0, 0x97,
          0x24, 0x2B, 0x73, 0x33, 0xEE, 0x84, 0x48, 0x41, 0x38, 0x13, 0x01, 0x0A, 0xC8, 0x06, 0x61, 0x11,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefgh",
      {
          0xFE, 0x1E, 0xB8, 0x37, 0xB3, 0x5D, 0x14, 0x4A, 0xBD, 0xFD, 0xE8, 0xDF, 0x3C, 0x3C, 0x96, 0xAF,
          0x6E, 0x18, 0x27, 0xDE, 0xEF, 0x81, 0x56, 0x07, 0x7F, 0x1C, 0x31, 0x90, 0xF2, 0x9C, 0x36, 0xC5,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghi",
      {
          0x05, 0x8A, 0xE5, 0x02, 0xEA, 0xF6, 0xE8, 0xD3, 0x86, 0x65, 0xD8, 0x49, 0x19, 0x7B, 0xB7, 0xE6,
          0x53, 0xA4, 0xC6, 0xC7, 0x84, 0x2D, 0x77, 0x38, 0x5F, 0xF9, 0xAE, 0xA5, 0xCA, 0x02, 0xC4, 0xDA,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghij",
      {
          0x0D, 0xBA, 0x2C, 0xD0, 0x5F, 0x9A, 0x5D, 0xBE, 0xA5, 0xF9, 0x3C, 0x8B, 0x3A, 0x89, 0x9F, 0x7E,
          0x91, 0xE1, 0x53, 0x0A, 0xD2, 0xCB, 0x57, 0xB3, 0x91, 0x43, 0x09, 0xEA, 0xC5, 0xF8, 0x70, 0x1F,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijk",
      {
          0x45, 0x65, 0x48, 0x11, 0x96, 0xB3, 0xB1, 0xFD, 0x39, 0xCF, 0x2C, 0x1A, 0x7D, 0x74, 0xAE, 0x1D,
          0xA7, 0xF7, 0xD3, 0xBB, 0xE9, 0x25, 0x2B, 0xF0, 0x45, 0x4B, 0x5C, 0x98, 0xC9, 0x12, 0xFF, 0x22,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijkl",
      {
          0x57, 0xCF, 0xAD, 0x8C, 0xD1, 0xC4, 0x8C, 0x2E, 0x4C, 0x55, 0x59, 0xAB, 0x5E, 0xDE, 0xCC, 0xF6,
          0xFE, 0xC0, 0xBD, 0x71, 0x97, 0xAF, 0x64, 0x24, 0xB9, 0x23, 0x29, 0x6F, 0x40, 0x31, 0xD5, 0xBE,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklm",
      {
          0x2A, 0x61, 0x25, 0xE4, 0x03, 0x22, 0x18, 0xAF, 0x7B, 0x5C, 0xB4, 0x24, 0xDC, 0x8C, 0x7C, 0xD3,
          0xD1, 0x89, 0x6D, 0xCD, 0x34, 0x69, 0xCD, 0x9E, 0xEC, 0x66, 0xA3, 0x14, 0xB4, 0xD0, 0x8D, 0x2C,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmn",
      {
          0x53, 0x60, 0x2B, 0xF8, 0x56, 0x09, 0x88, 0x8B, 0xB9, 0xAC, 0xA6, 0x87, 0x74, 0x9E, 0xA8, 0x67,
          0xEA, 0xC9, 0x90, 0x56, 0xFF, 0xC3, 0x8F, 0x75, 0x76, 0x45, 0xAE, 0x7C, 0x4D, 0xB9, 0xE8, 0x6C,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmno",
      {
          0x80, 0xAD, 0xF9, 0x80, 0x92, 0x75, 0x9D, 0xE4, 0x94, 0x3E, 0xAF, 0x28, 0x29, 0x2D, 0x89, 0x5E,
          0x44, 0x7D, 0x53, 0x9A, 0xC7, 0x54, 0x0F, 0xC6, 0x79, 0x32, 0xAA, 0x23, 0xF5, 0xE1, 0xFA, 0x4F,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnop",
      {
          0x44, 0xEC, 0xBE, 0xF5, 0x84, 0xB2, 0x04, 0x53, 0x77, 0x6B, 0xDC, 0x9C, 0x7F, 0x41, 0x69, 0x45,
          0xBC, 0x63, 0xB8, 0x92, 0x27, 0xBF, 0xEA, 0x06, 0xB8, 0x77, 0x7B, 0x65, 0x9F, 0x1B, 0x49, 0xA0,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopq",
      {
          0x97, 0xBD, 0x10, 0x43, 0xA1, 0x84, 0xD0, 0xD5, 0xBE, 0x69, 0x9D, 0x3D, 0x0A, 0x89, 0x6F, 0xBB,
          0x6F, 0x76, 0x84, 0x57, 0x4F, 0xF3, 0xDE, 0x14, 0x8B, 0xE1, 0xBC, 0xFF, 0x10, 0xF7, 0x87, 0xF0,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqr",
      {
          0x7D, 0x50, 0x96, 0xC5, 0xAA, 0x96, 0x23, 0x88, 0x35, 0x48, 0x77, 0x6A, 0x08, 0xD8, 0xDC, 0x92,
          0xA8, 0x4A, 0x6F, 0x62, 0xB0, 0x6D, 0x82, 0x4A, 0x57, 0xE7, 0x2B, 0x90, 0xAD, 0xB8, 0x03, 0x97,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrs",
      {
          0x19, 0x05, 0x09, 0xA4, 0xB4, 0x72, 0x3A, 0x76, 0x7E, 0x1F, 0x13, 0x83, 0x90, 0x35, 0x99, 0x12,
          0x39, 0xF8, 0x24, 0x01, 0x87, 0xBA, 0x08, 0xB7, 0xE9, 0x95, 0xBB, 0x1E, 0x45, 0x25, 0x38, 0x5C,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrst",
      {
          0xAF, 0x01, 0xC1, 0x82, 0x16, 0x81, 0x37, 0x7B, 0xF4, 0xDF, 0xB4, 0xA4, 0xFB, 0x65, 0xA9, 0x4F,
          0xD7, 0x37, 0x4B, 0x9D, 0xB4, 0x91, 0xCB, 0x94, 0x3F, 0x93, 0x7B, 0x34, 0xC7, 0xF4, 0xE6, 0xA0,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstu",
      {
          0xB8, 0x03, 0xE3, 0xFD, 0x54, 0x09, 0x52, 0x7A, 0xB5, 0x73, 0x84, 0xD7, 0x3B, 0x92, 0x07, 0x7D,
          0xA3, 0x38, 0x54, 0x7C, 0xBF, 0xA7, 0xD0, 0x83, 0x32, 0x69, 0xA0, 0x3D, 0x7D, 0x6D, 0x63, 0x7E,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuv",
      {
          0x82, 0x95, 0x7A, 0x28, 0x2B, 0xCF, 0x5A, 0x09, 0x8E, 0x27, 0x07, 0xB5, 0xC1, 0xA6, 0x29, 0x2D,
          0xFB, 0x9A, 0x4A, 0x21, 0x5C, 0x19, 0x5F, 0xD8, 0x74, 0xB7, 0x4E, 0x99, 0xBE, 0x18, 0xF4, 0x32,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvw",
      {
          0xA8, 0xDF, 0x28, 0xF6, 0xC9, 0xF9, 0x0D, 0x86, 0xAB, 0xC7, 0x6E, 0x34, 0x3C, 0x1F, 0xBB, 0x50,
          0xF3, 0x76, 0x56, 0x9E, 0xB3, 0x14, 0x19, 0x23, 0x0E, 0x4F, 0xAD, 0x2D, 0xD5, 0xCE, 0x91, 0x72,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwx",
      {
          0xD0, 0x6D, 0x0B, 0x49, 0xC4, 0x9B, 0xAF, 0x6E, 0x95, 0xEF, 0x6A, 0x88, 0xE2, 0x2C, 0xD6, 0x96,
          0x3F, 0xE4, 0xA1, 0xA0, 0x69, 0x4D, 0xE7, 0x7B, 0x34, 0x0A, 0xA8, 0x53, 0x56, 0xBB, 0xC4, 0xC7,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxy",
      {
          0x71, 0xEF, 0x33, 0x12, 0xC6, 0x40, 0x58, 0xEC, 0xB5, 0x99, 0x19, 0xE2, 0x7D, 0x68, 0xCA, 0x52,
          0x59, 0x73, 0x69, 0x8A, 0xCA, 0x2A, 0x53, 0x16, 0xAC, 0x8B, 0x3D, 0xBC, 0x0C, 0x2E, 0x82, 0x73,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz",
      {
          0x47, 0x10, 0xF8, 0x6D, 0xA6, 0x2B, 0x70, 0x81, 0x3F, 0xE3, 0xC2, 0xDF, 0xFA, 0xB8, 0xEF, 0x81,
          0xE0, 0x97, 0xC7, 0xB1, 0x0F, 0xD6, 0x74, 0xB3, 0x62, 0xF0, 0xC9, 0x0E, 0xA2, 0xCE, 0xD4, 0xED,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0",
      {
          0x59, 0xFF, 0xCE, 0x58, 0xB7, 0xCC, 0x67, 0xEC, 0x4B, 0xA6, 0x51, 0x86, 0x0A, 0x75, 0x88, 0x47,
          0x27, 0x2E, 0x4B, 0x77, 0xB8, 0xD5, 0xA1, 0x75, 0x53, 0xF0, 0xEE, 0xCA, 0xF7, 0x1C, 0xCC, 0x5F,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz01",
      {
          0x72, 0x76, 0x15, 0x78, 0x6E, 0x11, 0xB4, 0x2C, 0xEF, 0x15, 0x0B, 0xD7, 0x2C, 0x6F, 0x07, 0x08,
          0x0A, 0xA6, 0x7F, 0xBE, 0x16, 0xFD, 0x67, 0x16, 0xB8, 0x4A, 0xD3, 0x55, 0xE8, 0x2E, 0x73, 0xF5,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz012",
      {
          0xE8, 0x20, 0xC0, 0x57, 0x65, 0x3A, 0xAD, 0x4C, 0xC6, 0x3E, 0x3F, 0x86, 0xB1, 0x97, 0xB0, 0x08,
          0x73, 0x1A, 0xF0, 0xFB, 0x79, 0xBC, 0xB6, 0x87, 0xDD, 0x53, 0xD2, 0x5C, 0x8B, 0xD9, 0x32, 0x12,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123",
      {
          0xD4, 0xB2, 0x5D, 0x15, 0xAA, 0x53, 0x08, 0x7A, 0x45, 0x97, 0xBC, 0xDC, 0x55, 0x53, 0xD6, 0x53,
          0xBB, 0x6D, 0xF8, 0x0C, 0xC4, 0x6E, 0x17, 0x6B, 0xB3, 0xCA, 0x96, 0x5E, 0x0D, 0x36, 0xF9, 0xD1,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz01234",
      {
          0x8F, 0x19, 0x2D, 0x38, 0xF7, 0xCB, 0x90, 0x52, 0x15, 0x7F, 0xAE, 0xCA, 0x28, 0x9F, 0xAE, 0xA7,
          0xE3, 0xEF, 0x02, 0x7A, 0x61, 0x50, 0x7C, 0xF7, 0x69, 0x45, 0xDC, 0x7C, 0xF7, 0xC5, 0x4A, 0x50,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz012345",
      {
          0xC5, 0x36, 0x0F, 0x67, 0x44, 0xC7, 0xD9, 0x23, 0x5B, 0x3E, 0x58, 0xE7, 0xE8, 0xC5, 0xF7, 0xE5,
          0x67, 0xED, 0x14, 0x5A, 0xD2, 0x5B, 0x8D, 0x26, 0xB9, 0xE8, 0x4C, 0x14, 0x44, 0x3C, 0x2C, 0xBB,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456",
      {
          0x99, 0x9E, 0xF1, 0x41, 0x0C, 0xF9, 0x42, 0x25, 0x0B, 0x58, 0x37, 0xA4, 0xEF, 0x0A, 0x59, 0x61,
          0xD8, 0x73, 0xC6, 0x0C, 0x89, 0x9B, 0xED, 0xAA, 0xCE, 0x45, 0x72, 0x8E, 0x34, 0x7B, 0x52, 0xF0,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz01234567",
      {
          0xCE, 0xB1, 0x9A, 0x9C, 0x69, 0x7C, 0x12, 0x2E, 0x4D, 0xEF, 0xAD, 0x4A, 0xBC, 0x34, 0x09, 0x2B,
          0x3E, 0x82, 0x48, 0xE5, 0xA6, 0x38, 0xAA, 0xCC, 0x84, 0xB9, 0x52, 0x9B, 0x47, 0xF0, 0x3E, 0x73,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz012345678",
      {
          0xF8, 0x2A, 0x15, 0x49, 0x1E, 0xA9, 0x37, 0xD6, 0x0D, 0x74, 0xA0, 0xCE, 0xA7, 0xF9, 0xD1, 0x05,
          0x35, 0xB0, 0xB8, 0x50, 0x87, 0x1C, 0xC7, 0x7D, 0x97, 0x94, 0x8C, 0x60, 0x76, 0x62, 0xBD, 0x43,
      } },
    { "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789",
      {
          0x44, 0xDD, 0xDB, 0x39, 0xBD, 0xB2, 0xAF, 0x80, 0xC1, 0x47, 0x89, 0x4C, 0x1D, 0x75, 0x6A, 0xDA,
          0x3D, 0x1C, 0x2A, 0xC2, 0xB1, 0x00, 0x54, 0x1E, 0x04, 0xFE, 0x87, 0xB4, 0xA5, 0x9E, 0x12, 0x43,
      } },
};

TEST(misc_blake2s, test_vectors)
{
    for (auto v : test_vectors) {
        std::vector<uint8_t> input(v.input.begin(), v.input.end());
        EXPECT_EQ(blake2::blake2s(input), v.output);
    }
}