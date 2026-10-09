// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Punycode against RFC 3492 section 7.1's samples, both ways, and its failures.

#include <webcpp/idna/punycode.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace idna = webcpp::idna;
namespace punycode = webcpp::idna::punycode;

namespace {

// The span forms throw nothing; the pmr forms may throw what their memory resource throws
// (idna-invariant-2).
static_assert(noexcept(punycode::encode(std::declval<std::span<char>>(), std::u32string_view())));
static_assert(noexcept(punycode::decode(std::declval<std::span<char32_t>>(), std::string_view())));
static_assert(!noexcept(punycode::encode(std::u32string_view())));
static_assert(!noexcept(punycode::decode(std::string_view())));

/** One sample of RFC 3492 section 7.1: its code points, and its Punycode as the RFC prints it,
    mixed-case annotation included. */
struct sample {
    std::u32string_view code_points;
    std::string_view encoded;
};

constexpr auto samples = std::to_array<sample>({
    {
        .code_points = U"\u0644\u064A\u0647\u0645\u0627\u0628\u062A\u0643\u0644\u0645\u0648\u0634"
                       U"\u0639\u0631\u0628\u064A\u061F",
        .encoded = "egbpdaj6bu4bxfgehfvwxn",
    },
    {
        .code_points = U"\u4ED6\u4EEC\u4E3A\u4EC0\u4E48\u4E0D\u8BF4\u4E2D\u6587",
        .encoded = "ihqwcrb4cv8a8dqg056pqjye",
    },
    {
        .code_points = U"\u4ED6\u5011\u7232\u4EC0\u9EBD\u4E0D\u8AAA\u4E2D\u6587",
        .encoded = "ihqwctvzc91f659drss3x8bo0yb",
    },
    {
        .code_points = U"Pro\u010Dprost\u011Bnemluv\u00ED\u010Desky",
        .encoded = "Proprostnemluvesky-uyb24dma41a",
    },
    {
        .code_points = U"\u05DC\u05DE\u05D4\u05D4\u05DD\u05E4\u05E9\u05D5\u05D8\u05DC\u05D0\u05DE"
                       U"\u05D3\u05D1\u05E8\u05D9\u05DD\u05E2\u05D1\u05E8\u05D9\u05EA",
        .encoded = "4dbcagdahymbxekheh6e0a7fei0b",
    },
    {
        .code_points = U"\u092F\u0939\u0932\u094B\u0917\u0939\u093F\u0928\u094D\u0926\u0940\u0915"
                       U"\u094D\u092F\u094B\u0902\u0928\u0939\u0940\u0902\u092C\u094B\u0932\u0938"
                       U"\u0915\u0924\u0947\u0939\u0948\u0902",
        .encoded = "i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd",
    },
    {
        .code_points = U"\u306A\u305C\u307F\u3093\u306A\u65E5\u672C\u8A9E\u3092\u8A71\u3057\u3066"
                       U"\u304F\u308C\u306A\u3044\u306E\u304B",
        .encoded = "n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa",
    },
    {
        .code_points = U"\uC138\uACC4\uC758\uBAA8\uB4E0\uC0AC\uB78C\uB4E4\uC774\uD55C\uAD6D\uC5B4"
                       U"\uB97C\uC774\uD574\uD55C\uB2E4\uBA74\uC5BC\uB9C8\uB098\uC88B\uC744\uAE4C",
        .encoded = "989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0dt30a5jpsd879ccm6fea98c",
    },
    {
        .code_points =
            U"\u043F\u043E\u0447\u0435\u043C\u0443\u0436\u0435\u043E\u043D\u0438\u043D\u0435\u0433"
            U"\u043E\u0432\u043E\u0440\u044F\u0442\u043F\u043E\u0440\u0443\u0441\u0441\u043A\u0438",
        .encoded = "b1abfaaepdrnnbgefbaDotcwatmq2g4l",
    },
    {
        .code_points = U"Porqu\u00E9nopuedensimplementehablarenEspa\u00F1ol",
        .encoded = "PorqunopuedensimplementehablarenEspaol-fmd56a",
    },
    {
        .code_points = U"T\u1EA1isaoh\u1ECDkh\u00F4ngth\u1EC3ch\u1EC9n\u00F3iti\u1EBFngVi\u1EC7t",
        .encoded = "TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g",
    },
    {
        .code_points = U"3\u5E74B\u7D44\u91D1\u516B\u5148\u751F",
        .encoded = "3B-ww4c5e180e575a65lsy2b",
    },
    {
        .code_points = U"\u5B89\u5BA4\u5948\u7F8E\u6075-with-SUPER-MONKEYS",
        .encoded = "-with-SUPER-MONKEYS-pc58ag80a8qai00g7n9n",
    },
    {
        .code_points = U"Hello-Another-Way-\u305D\u308C\u305E\u308C\u306E\u5834\u6240",
        .encoded = "Hello-Another-Way--fc4qua05auwb3674vfr0b",
    },
    {
        .code_points = U"\u3072\u3068\u3064\u5C4B\u6839\u306E\u4E0B2",
        .encoded = "2-u9tlzr9756bt3uc0v",
    },
    {
        .code_points = U"Maji\u3067Koi\u3059\u308B5\u79D2\u524D",
        .encoded = "MajiKoi5-783gue6qz075azm5e",
    },
    {
        .code_points = U"\u30D1\u30D5\u30A3\u30FCde\u30EB\u30F3\u30D0",
        .encoded = "de-jg4avhby1noc0d",
    },
    {
        .code_points = U"\u305D\u306E\u30B9\u30D4\u30FC\u30C9\u3067",
        .encoded = "d9juau41awczczp",
    },
    {
        .code_points = U"-> $1.00 <-",
        .encoded = "-> $1.00 <--",
    },
});

/** Inputs that tell apart what RFC 3492's samples leave alike, each checked against Python's
    punycode codec on 2026-10-09: a first delta whose division by damp, rather than by damp + 1,
    changes the bias; a delta that ends adapt's loop at exactly 455; and two whose thresholds
    differ under an initial bias of 70 or of 83, where every bias from 71 to 82 gives the same. */
constexpr auto edges = std::to_array<sample>({
    {
        .code_points = U"\U000DA442\u17C3w",
        .encoded = "w-kzjy4831q",
    },
    {
        .code_points = U"b\U00036610\U000B1709\U0007A6F1",
        .encoded = "b-rp77aex91o9xjw",
    },
    {
        .code_points = U"\u24EDp",
        .encoded = "p-4hp",
    },
    {
        .code_points = U"\U000700C1nf",
        .encoded = "nf-gw73i",
    },
});

/** `text` with its ASCII letters in lowercase: the encoder writes no mixed-case annotation. */
std::string lowercase(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

/** The basic code points are copied as they are, so only the part after the last delimiter is
    lowercase in what the encoder writes. */
std::string as_encoded(std::string_view rfc) {
    const std::size_t delimiter = rfc.rfind('-');
    if (delimiter == std::string_view::npos) {
        return lowercase(rfc);
    }
    return std::string(rfc.substr(0, delimiter + 1)) + lowercase(rfc.substr(delimiter + 1));
}

/** Encodes the sample's code points into its Punycode, and decodes them back. */
void both_ways(const sample& s) {
    std::array<char, 128> encoded{};
    const idna::result<std::size_t> written = punycode::encode(encoded, s.code_points);
    if (BOOST_TEST(written.has_value())) {
        BOOST_TEST_EQ(std::string_view(encoded.data(), written.value()), as_encoded(s.encoded));
    }
    std::array<char32_t, 128> decoded{};
    const idna::result<std::size_t> read = punycode::decode(decoded, s.encoded);
    if (BOOST_TEST(read.has_value())) {
        BOOST_TEST(std::u32string_view(decoded.data(), read.value()) == s.code_points);
    }
}

void rfc_3492_samples_both_ways() {
    for (const sample& s : samples) {
        both_ways(s);
    }
}

void edges_both_ways() {
    for (const sample& s : edges) {
        both_ways(s);
    }
}

/** Digits are read in either case; the basic code points keep theirs (RFC 3492 section 5). */
/** The code points `input` decodes to, or a string no decode gives when it fails. */
std::u32string decoded_from(std::string_view input) {
    std::array<char32_t, 16> decoded{};
    const idna::result<std::size_t> read = punycode::decode(decoded, input);
    if (!BOOST_TEST(read.has_value())) {
        return U"failed";
    }
    return {decoded.data(), read.unsafe_value()};
}

void case_of_digits_and_basic_code_points() {
    BOOST_TEST(decoded_from("bcher-kva") == U"b\u00FCcher");
    BOOST_TEST(decoded_from("bcher-KVA") == U"b\u00FCcher");
    BOOST_TEST(decoded_from("BCHER-kva") == U"B\u00FCCHER");
}

void failures() {
    std::array<char32_t, 16> decoded{};
    // A delimiter at the very start consumes nothing, and is then no digit (xn---9uc).
    BOOST_TEST(punycode::decode(decoded, "-9uc").error() == idna::error::invalid_punycode);
    BOOST_TEST(punycode::decode(decoded, "a!b").error() == idna::error::invalid_punycode);
    BOOST_TEST(punycode::decode(decoded, "\303\274-kva").error() == idna::error::invalid_punycode);
    // A number cut short: "k" asks for another digit.
    BOOST_TEST(punycode::decode(decoded, "bcher-k").error() == idna::error::invalid_punycode);
    // A decoded value beyond U+10FFFF, then a surrogate.
    BOOST_TEST(punycode::decode(decoded, "en32g").error() == idna::error::invalid_punycode);
    BOOST_TEST(punycode::decode(decoded, "ib9b").error() == idna::error::invalid_punycode);
    // The surrogates' last, U+DFFF, and the scalar values on either side of them.
    BOOST_TEST(punycode::decode(decoded, "zy0c").error() == idna::error::invalid_punycode);
    BOOST_TEST(punycode::decode(decoded, "hb9b").value() == 1U && decoded[0] == U'\uD7FF');
    BOOST_TEST(punycode::decode(decoded, "0y0c").value() == 1U && decoded[0] == U'\uE000');
    // "a" is the first code point that is not basic, U+0080.
    BOOST_TEST(punycode::decode(decoded, "a").value() == 1U && decoded[0] == U'\x80');
    // Digits whose value overflows 32 bits (RFC 3492 section 6.4).
    BOOST_TEST(punycode::decode(decoded, "999999999999999999999").error() ==
               idna::error::punycode_overflow);
    std::array<char, 16> encoded{};
    BOOST_TEST(punycode::encode(encoded, U"\xD800").error() == idna::error::invalid_punycode);
    // The encoder's delta for U+10FFFF after h basic code points is (0x10FFFF - 0x80) * (h + 1),
    // past 32 bits from h = 3855 on (RFC 3492 section 6.4).
    std::array<char, 4096> long_output{};
    std::u32string near_the_limit(3854, U'a');
    near_the_limit.push_back(0x10FFFF);
    BOOST_TEST(punycode::encode(long_output, near_the_limit).has_value());
    std::u32string past_the_limit(3855, U'a');
    past_the_limit.push_back(0x10FFFF);
    BOOST_TEST(punycode::encode(long_output, past_the_limit).error() ==
               idna::error::punycode_overflow);
    BOOST_TEST(punycode::encode(encoded, std::u32string_view(U"\x110000", 1)).error() ==
               idna::error::invalid_punycode);
    BOOST_TEST(punycode::encode(encoded, U"\xDFFF").error() == idna::error::invalid_punycode);
}

/** A code point that is no scalar value, and a byte above 0x7F before the delimiter, are found
    before anything is written, so the span form returns the pmr form's error whatever its size. */
void invalid_input_whatever_the_output() {
    std::array<char, 1> one_char{};
    BOOST_TEST(punycode::encode(one_char, U"ab\xD800").error() == idna::error::invalid_punycode);
    BOOST_TEST(punycode::encode(U"ab\xD800").error() == idna::error::invalid_punycode);
    std::array<char32_t, 1> one_code_point{};
    BOOST_TEST(punycode::decode(one_code_point, "ab\303\274-kva").error() ==
               idna::error::invalid_punycode);
    BOOST_TEST(punycode::decode("ab\303\274-kva").error() == idna::error::invalid_punycode);
}

/**
 Each overflow of RFC 3492 section 6.4 is punycode_overflow, wherever it happens: in the decoder's
 i or n, in the encoder's delta or in its increments.

 Tip: "k0902716a" is 2^32 - 1 as one number under the initial bias, so n, 0x80 plus it, overflows;
 "k0902716b" ends on a digit one larger, so i itself does. The encoder's delta for U+10FF70 after
 3,855 basic code points is (0x10FF70 - 0x80) * 3,856, 255 below 2^32, and the 3,855 increments
 that follow, one per smaller code point, pass it.
*/
void every_overflow_guard() {
    std::array<char32_t, 16> decoded{};
    BOOST_TEST(punycode::decode(decoded, "k0902716a").error() == idna::error::punycode_overflow);
    BOOST_TEST(punycode::decode(decoded, "k0902716b").error() == idna::error::punycode_overflow);
    // Exactly 2^32 - 1 is no overflow: n reaches it, beyond Unicode.
    BOOST_TEST(punycode::decode(decoded, "ww902716a").error() == idna::error::invalid_punycode);
    std::array<char, 4096> long_output{};
    std::u32string increments(3855, U'a');
    increments.push_back(0x10FF70);
    BOOST_TEST(punycode::encode(long_output, increments).error() == idna::error::punycode_overflow);
}

/**
 Arithmetic that reaches 2^32 - 1 exactly, and no further, succeeds both ways.

 Tip: after 4,095 basic code points, U+10007F's delta is (0x10007F - 0x80) * 4,096, 4,095 below
 2^32, and the 4,095 increments that follow reach 2^32 - 1, which the decoder reads back as its
 i; checked against Python's punycode codec on 2026-10-09.
*/
void arithmetic_at_its_limit() {
    std::u32string code_points(4095, U'a');
    code_points.push_back(0x10007F);
    const std::string expected = std::string(4095, 'a') + "-k0902716a";
    std::array<char, 4200> encoded{};
    const idna::result<std::size_t> written = punycode::encode(encoded, code_points);
    if (BOOST_TEST(written.has_value())) {
        BOOST_TEST(std::string_view(encoded.data(), written.unsafe_value()) == expected);
    }
    std::array<char32_t, 4200> decoded{};
    const idna::result<std::size_t> read = punycode::decode(decoded, expected);
    if (BOOST_TEST(read.has_value())) {
        BOOST_TEST(std::u32string_view(decoded.data(), read.unsafe_value()) == code_points);
    }
}

/** Every length below the result's is output_too_small, and nothing is written past it. */
void output_too_small_at_every_length() {
    const std::u32string_view input = U"b\u00FCcher";
    for (std::size_t size = 0; size < 9; ++size) {
        std::array<char, 9> encoded{};
        encoded.fill('#');
        const idna::result<std::size_t> written =
            punycode::encode(std::span<char>(encoded.data(), size), input);
        BOOST_TEST(written.error() == idna::error::output_too_small);
        BOOST_TEST_EQ(encoded[size], '#');
    }
    std::array<char, 9> encoded{};
    BOOST_TEST_EQ(punycode::encode(encoded, input).value(), 9U);
    // Code points all basic end with the delimiter, which must fit too (RFC 3492 sample S).
    for (std::size_t size = 0; size < 4; ++size) {
        BOOST_TEST(punycode::encode(std::span<char>(encoded.data(), size), U"abc").error() ==
                   idna::error::output_too_small);
    }
    BOOST_TEST_EQ(punycode::encode(encoded, U"abc").value(), 4U);
    BOOST_TEST_EQ(std::string_view(encoded.data(), 4), "abc-");
    // The decoder's output of exactly the result's size suffices, whether the last code point is
    // inserted by the digits or copied from before the delimiter.
    std::array<char32_t, 6> exact{};
    const idna::result<std::size_t> six = punycode::decode(exact, "bcher-kva");
    BOOST_TEST(six.has_value() &&
               std::u32string_view(exact.data(), six.unsafe_value()) == U"b\u00FCcher");
    const idna::result<std::size_t> three =
        punycode::decode(std::span<char32_t>(exact.data(), 3), "abc-");
    BOOST_TEST(three.has_value() &&
               std::u32string_view(exact.data(), three.unsafe_value()) == U"abc");
    BOOST_TEST(punycode::decode(std::span<char32_t>(exact.data(), 2), "abc-").error() ==
               idna::error::output_too_small);
    const idna::result<std::size_t> one =
        punycode::decode(std::span<char32_t>(exact.data(), 1), "a");
    BOOST_TEST(one.has_value() && one.unsafe_value() == 1U && exact[0] == U'\x80');
    for (std::size_t size = 0; size < 6; ++size) {
        std::array<char32_t, 6> decoded{};
        const idna::result<std::size_t> read =
            punycode::decode(std::span<char32_t>(decoded.data(), size), "bcher-kva");
        BOOST_TEST(read.error() == idna::error::output_too_small);
    }
}

void the_pmr_forms() {
    std::array<std::byte, 512> storage{};
    std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size(),
                                                 std::pmr::null_memory_resource());
    const idna::result<std::pmr::string> encoded = punycode::encode(U"b\u00FCcher", resource);
    BOOST_TEST(encoded.has_value() && encoded.value() == "bcher-kva");
    const idna::result<std::pmr::u32string> decoded = punycode::decode("bcher-kva", resource);
    BOOST_TEST(decoded.has_value() && decoded.value() == U"b\u00FCcher");
    BOOST_TEST(punycode::decode("-9uc", resource).error() == idna::error::invalid_punycode);
}

}  // namespace

int main() {
    rfc_3492_samples_both_ways();
    edges_both_ways();
    case_of_digits_and_basic_code_points();
    failures();
    invalid_input_whatever_the_output();
    every_overflow_guard();
    arithmetic_at_its_limit();
    output_too_small_at_every_length();
    the_pmr_forms();
    return boost::report_errors();
}
