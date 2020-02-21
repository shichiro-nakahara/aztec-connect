#include <gtest/gtest.h>

#include <barretenberg/curves/bn254/fq2.hpp>

using namespace barretenberg;

TEST(fq2, eq)
{
    fq2::field_t a = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t b = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t c = { .c0 = {{ 0x01, 0x02, 0x03, 0x05 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t d = { .c0 = {{ 0x01, 0x02, 0x04, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t e = { .c0 = {{ 0x01, 0x03, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t f = { .c0 = {{ 0x02, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x09 } } };
    fq2::field_t g = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x07, 0x07, 0x08, 0x09 } } };
    fq2::field_t h = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x08, 0x08, 0x09 } } };
    fq2::field_t i = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x09, 0x09 } } };
    fq2::field_t j = { .c0 = {{ 0x01, 0x02, 0x03, 0x04 } }, .c1 = {{ 0x06, 0x07, 0x08, 0x0a } } };

    EXPECT_EQ(a == b, true);
    EXPECT_EQ(a == c, false);
    EXPECT_EQ(a == d, false);
    EXPECT_EQ(a == e, false);
    EXPECT_EQ(a == f, false);
    EXPECT_EQ(a == g, false);
    EXPECT_EQ(a == h, false);
    EXPECT_EQ(a == i, false);
    EXPECT_EQ(a == j, false);
}

TEST(fq2, is_zero)
{
    fq2::field_t a = fq2::zero;
    fq2::field_t b = fq2::zero;
    fq2::field_t c = fq2::zero;
    b.c0.data[0] = 1;
    c.c1.data[0] = 1;
    EXPECT_EQ(a.is_zero(), true);
    EXPECT_EQ(b.is_zero(), false);
    EXPECT_EQ(c.is_zero(), false);
}

TEST(fq2, random_element)
{
    fq2::field_t a = fq2::random_element();
    fq2::field_t b = fq2::random_element();

    EXPECT_EQ(fq2::eq(a, b), false);
    EXPECT_EQ(fq2::is_zero(a), false);
    EXPECT_EQ(fq2::is_zero(b), false);
}

TEST(fq2, mul_check_against_constants)
{
    fq2::field_t a = {.c0 = {{0xd673ba38b8c4bc86, 0x860cd1cb9e2f0c85, 0x3185f9f9166177b7, 0xd043f963ced2529}}, .c1 = {{0xd4d2fad9a3de5d98, 0x260f72ca434ef415, 0xca5c20c435accb2d, 0x122a54f828a07ffe}}};
    fq2::field_t b = {.c0 = {{0x37710e0986ad0fab, 0xd9b1f41ba9d3bd92, 0xf71f600e90104795, 0x24e1f6018a4d85c6}}, .c1 = {{0x5e65448f225b0f60, 0x7783aecd5d7bfa84, 0xc7a76eed72d68723, 0xc8f427c031af99a}}};
    fq2::field_t expected = {.c0 = {{0x1652ca66b00ad519, 0x6619a315656ea7c7, 0x1d8491b044e9a08f, 0xcbe6d11bff2e56b}}, .c1 = {{0x9694fb422eff4e79, 0xebdbcf03e8539a17, 0xc4787fb63b8d10e8, 0x1a5cc397aae8811f}}};
    fq2::field_t result = a * b;
    EXPECT_EQ(result, expected);
}

TEST(fq2, sqr_check_against_constants)
{
    fq2::field_t a = {.c0 = {{0x26402fd760069ee8, 0x17828cf3bf7dd3e3, 0x4e7449f7b1149987, 0x102f6467805d7298}}, .c1 = {{0xa2a31bf895eaf6f8, 0xf0c88d415c372b16, 0xa65ccca8b7806691, 0x1b51e4526673451f}}};
    fq2::field_t expected = {.c0 = {{0xb51c9049894c45f3, 0xf8ef65c0244dfc90, 0x42c37c0f7d09aacb, 0x64ddfb845b2901f}}, .c1 = {{0x9e176fa8cdca97b1, 0xd04ae89dab7da31e, 0x637b83e950322d50, 0x155cccfadafc70b4}}};
    fq2::field_t result = a.sqr();
    EXPECT_EQ(result, expected);
}

TEST(fq2, add_check_against_constants)
{
    fq2::field_t a = {.c0 = {{0x517c157ce1664f30, 0x114ba401b0996437, 0x11b9ae2d856012e8, 0xcc19341ea7cf685}}, .c1 = {{0x17c6020dde15fdc0, 0x310bc25961b2f002, 0xa766e7e94a865c0d, 0x20176bc8e6b82863}}};
    fq2::field_t b = {.c0 = {{0xffad1c8ac38be684, 0x2a953b27cb1f541d, 0xfc12b9dfe76a0f12, 0x434c570deb975a6}}, .c1 = {{0x87430d4b17897ace, 0x33ab4d0e55e8932a, 0xe4465ff65990dd31, 0x83db0b3c55f9e9f}}};
    fq2::field_t expected = {.c0 = {{0x51293207a4f235b4, 0x3be0df297bb8b855, 0xdcc680d6cca21fa, 0x10f658b2c9366c2c}}, .c1 = {{0x9f090f58f59f788e, 0x64b70f67b79b832c, 0x8bad47dfa417393e, 0x28551c7cac17c703}}};
    fq2::field_t result = a + b;
    EXPECT_EQ(result, expected);
}

TEST(fq2, sub_check_against_constants)
{
    fq2::field_t a = {.c0 = {{0x3212c3a7d7886da5, 0xcea893f4addae4aa, 0x5c8bfca7a7ed01be, 0x1a8e9dfecd598ef1}}, .c1 = {{0x4a8d9e6443fda462, 0x93248a3fde6374e7, 0xf4a6c52f75c0fc2e, 0x270aaabb4ae43370}}};
    fq2::field_t b = {.c0 = {{0x875cef17b3b46751, 0xbba7211cb92b554b, 0xa4790f1657f85606, 0x74e61182f5b5068}}, .c1 = {{0x8a84fff282dfd5a3, 0x77986fd41c21a7a3, 0xdc7072908fe375a9, 0x2e98a18c7d570269}}};
    fq2::field_t expected = {.c0 = {{0xaab5d49023d40654, 0x130172d7f4af8f5e, 0xb812ed914ff4abb8, 0x13403ce69dfe3e88}}, .c1 = {{0xfc292a88999acc06, 0xb30d84fd2ab397d0, 0xd0869855675edee2, 0x28d657a1aebed130}}};
    fq2::field_t result = a - b;
    EXPECT_EQ(result, expected);
}

TEST(fq2, to_montgomery_form)
{
    fq2::field_t result = fq2::zero;
    result.c0.data[0] = 1;
    fq2::field_t expected = fq2::one;
    result.self_to_montgomery_form();
    EXPECT_EQ(result, expected);
}

TEST(fq2, from_montgomery_form)
{
    fq2::field_t result = fq2::one;
    fq2::field_t expected = fq2::zero;
    expected.c0.data[0] = 1;
    result.self_from_montgomery_form();
    EXPECT_EQ(result, expected);
}

TEST(fq2, montgomery_consistency_check)
{
    fq2::field_t a = fq2::random_element();
    fq2::field_t b = fq2::random_element();
    fq2::field_t aR;
    fq2::field_t bR;
    fq2::field_t aRR;
    fq2::field_t bRR;
    fq2::field_t bRRR;
    fq2::field_t result_a;
    fq2::field_t result_b;
    fq2::field_t result_c;
    fq2::field_t result_d;
    fq2::__to_montgomery_form(a, aR);
    fq2::__to_montgomery_form(aR, aRR);
    fq2::__to_montgomery_form(b, bR);
    fq2::__to_montgomery_form(bR, bRR);
    fq2::__to_montgomery_form(bRR, bRRR);
    fq2::__mul(aRR, bRR, result_a); // abRRR
    fq2::__mul(aR, bRRR, result_b); // abRRR
    fq2::__mul(aR, bR, result_c);   // abR
    fq2::__mul(a, b, result_d);     // abR^-1
    EXPECT_EQ(fq2::eq(result_a, result_b), true);
    fq2::__from_montgomery_form(result_a, result_a); // abRR
    fq2::__from_montgomery_form(result_a, result_a); // abR
    fq2::__from_montgomery_form(result_a, result_a); // ab
    fq2::__from_montgomery_form(result_c, result_c); // ab
    fq2::__to_montgomery_form(result_d, result_d);   // ab
    EXPECT_EQ(fq2::eq(result_a, result_c), true);
    EXPECT_EQ(fq2::eq(result_a, result_d), true);
}

TEST(fq2, mul_sqr_consistency)
{
    fq2::field_t a = fq2::random_element();
    fq2::field_t b = fq2::random_element();
    fq2::field_t t1;
    fq2::field_t t2;
    fq2::field_t mul_result;
    fq2::field_t sqr_result;
    t1 = a - b;
    t2 = a + b;
    mul_result = t1 * t2;
    t1 = a.sqr();
    t2 = b.sqr();
    sqr_result = t1 - t2;
    EXPECT_EQ(mul_result, sqr_result);
}

TEST(fq2, add_mul_consistency)
{
    fq2::field_t multiplicand = { .c0 = {{ 0x09, 0x00, 0x00, 0x00 } }, .c1 = {{ 0x00, 0x00, 0x00, 0x00 } } };
    fq2::__to_montgomery_form(multiplicand, multiplicand);

    fq2::field_t a = fq2::random_element();
    fq2::field_t result;
    fq2::__add(a, a, result);             // 2
    fq2::__add(result, result, result);   // 4
    fq2::__add(result, result, result);   // 8
    fq2::__add(result, a, result);        // 9

    fq2::field_t expected;
    fq2::__mul(a, multiplicand, expected);

    EXPECT_EQ(fq2::eq(result, expected), true);
}


TEST(fq2, sub_mul_consistency)
{
    fq2::field_t multiplicand = { .c0 = {{ 0x05, 0, 0, 0 } } , .c1 = {{ 0x00, 0x00, 0x00, 0x00 } } };
    fq2::__to_montgomery_form(multiplicand, multiplicand);

    fq2::field_t a = fq2::random_element();
    fq2::field_t result;
    fq2::__add(a, a, result);             // 2
    fq2::__add(result, result, result);   // 4
    fq2::__add(result, result, result);   // 8
    fq2::__sub(result, a, result);        // 7
    fq2::__sub(result, a, result);        // 6
    fq2::__sub(result, a, result);        // 5

    fq2::field_t expected;
    fq2::__mul(a, multiplicand, expected);

    EXPECT_EQ(fq2::eq(result, expected), true);  
}

TEST(fq2, invert)
{
    fq2::field_t input = fq2::random_element();
    fq2::field_t inverse = input.invert();
    fq2::field_t result = inverse * input;
    EXPECT_EQ(result, fq2::one);
}

TEST(fq2, copy)
{
    fq2::field_t result = fq2::random_element();
    fq2::field_t expected;
    fq2::__copy(result, expected);
    EXPECT_EQ(fq2::eq(result, expected), true);
}

TEST(fq2, serialize)
{
    uint8_t buffer[64];
    fq::field_t expected_c0 = {{ 0x1234567876543210, 0x2345678987654321, 0x3456789a98765432, 0x006789abcba98765 }};
    fq::field_t expected_c1 = {{ 0x12a4e67f76b43210, 0x23e56f898a65cc21, 0x005678add98e5432, 0x1f6789a2cba98700 }};
    fq2::field_t expected{ expected_c0, expected_c1 };

    fq2::serialize_to_buffer(expected, &buffer[0]);

    fq2::field_t result = fq2::serialize_from_buffer(&buffer[0]);

    EXPECT_EQ(fq2::eq(result, expected), true);
}