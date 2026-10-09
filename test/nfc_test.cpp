// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// NFC within a label's storage against every line of NormalizationTest.txt, and the storage
// itself: inline, and growing into a memory resource.

#include <webcpp/idna/detail/label_buffer.hpp>
#include <webcpp/idna/detail/nfc.hpp>

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nfc_vectors.hpp"

namespace detail = webcpp::idna::detail;
namespace nfc = webcpp::idna::detail::nfc;

namespace {

/** `text` decomposed as it is appended, then composed: what the driver does to a mapped label. */
template <class Label>
bool normalize_into(Label& label, std::u32string_view text) {
    for (const char32_t cp : text) {
        if (!nfc::append_decomposed(label, cp)) {
            return false;
        }
    }
    nfc::compose_label(label);
    return true;
}

void every_line_of_normalization_test() {
    for (const webcpp::test::nfc_vector& line : webcpp::test::nfc_vectors) {
        for (const std::u32string_view column : {line.source, line.nfc, line.nfd}) {
            detail::inline_label label;
            BOOST_TEST(normalize_into(label, column));
            BOOST_TEST(label.view() == line.nfc);
        }
        BOOST_TEST(nfc::is_nfc(line.nfc, detail::inline_label{}) == nfc::verdict::yes);
        const bool unchanged = line.source == line.nfc;
        BOOST_TEST_EQ(nfc::is_nfc(line.source, detail::inline_label{}) == nfc::verdict::yes,
                      unchanged);
    }
}

/** Hangul syllables decompose and compose by Unicode section 3.12's arithmetic: GA, and GAG. */
void hangul() {
    detail::inline_label label;
    BOOST_TEST(nfc::append_decomposed(label, 0xAC01));
    BOOST_TEST(label.view() == U"\u1100\u1161\u11A8");
    nfc::compose_label(label);
    BOOST_TEST(label.view() == U"\uAC01");
}

/** A full inline label refuses the next code point; a growing one moves to its resource. */
void the_storage() {
    detail::inline_label full;
    for (std::size_t i = 0; i < detail::max_label_code_points; ++i) {
        BOOST_TEST(full.push_back(U'a'));
    }
    BOOST_TEST(!full.push_back(U'a'));
    BOOST_TEST(!nfc::append_decomposed(full, 0x00E9));

    std::array<std::byte, 4096> storage{};
    std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size(),
                                                 std::pmr::null_memory_resource());
    detail::growing_label growing(resource);
    for (std::size_t i = 0; i <= detail::max_label_code_points; ++i) {
        BOOST_TEST(growing.push_back(U'a'));
    }
    BOOST_TEST_EQ(growing.view().size(), detail::max_label_code_points + 1);
    growing.shrink(3);
    BOOST_TEST(growing.view() == U"aaa");
    growing.clear();
    BOOST_TEST(growing.view().empty());
    BOOST_TEST_EQ(growing.reset(300).size(), 300U);
    BOOST_TEST_EQ(growing.reset(4).size(), 4U);
}

/** An A-label's chars are copied out, lowercase, before its code points are decoded in place. */
void an_a_label_kept_as_ascii() {
    detail::inline_label label;
    for (const char32_t cp : std::u32string_view(U"XN--Bcher-KVA")) {
        BOOST_TEST(label.push_back(cp));
    }
    BOOST_TEST_EQ(label.keep_ascii(), std::string_view("xn--bcher-kva"));
    BOOST_TEST_EQ(label.reset(9).size(), 9U);
    BOOST_TEST_EQ(label.keep_ascii().size(), 9U);
}

/** NFC_Quick_Check's Maybe normalizes a scratch copy: U+0301 composes with an e before it, not
    with an e-acute; a copy that does not fit the scratch storage says so. */
void is_nfc_maybe() {
    BOOST_TEST(nfc::is_nfc(U"e\u0301", detail::inline_label{}) == nfc::verdict::no);
    BOOST_TEST(nfc::is_nfc(U"\u00E9\u0301", detail::inline_label{}) == nfc::verdict::yes);
    std::array<char32_t, detail::max_label_code_points> text{};
    text.fill(0x00E9);  // NFC_Quick_Check Yes, and two code points decomposed
    text.back() = 0x0301;
    BOOST_TEST(nfc::is_nfc(std::u32string_view(text.data(), text.size()), detail::inline_label{}) ==
               nfc::verdict::exhausted);
}

/** A memory resource that counts what it hands out, from std::pmr::new_delete_resource. */
class counting_resource final : public std::pmr::memory_resource {
public:
    /** The number of allocations made through it. */
    [[nodiscard]] std::size_t allocations() const noexcept { return allocations_; }

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        ++allocations_;
        return std::pmr::new_delete_resource()->allocate(size, alignment);
    }

    void do_deallocate(void* memory, std::size_t size, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(memory, size, alignment);
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::size_t allocations_ = 0;
};

/** Appends `count` copies of `cp` to `label`: whether every one fitted. */
template <class Label>
bool append_copies(Label& label, std::size_t count, char32_t cp) {
    for (std::size_t i = 0; i < count; ++i) {
        if (!label.push_back(cp)) {
            return false;
        }
    }
    return true;
}

/** An inline label holds max_label_code_points code points after decomposition, and refuses a
    code point whose decomposition would cross that edge. */
void the_inline_edge() {
    const std::size_t capacity = detail::max_label_code_points;
    detail::inline_label fits;
    BOOST_TEST(append_copies(fits, capacity - 2, U'a'));
    BOOST_TEST(nfc::append_decomposed(fits, 0x00E9));  // e and U+0301
    BOOST_TEST_EQ(fits.view().size(), capacity);
    nfc::compose_label(fits);
    BOOST_TEST_EQ(fits.view().size(), capacity - 1);
    BOOST_TEST(fits.view().back() == 0x00E9);

    detail::inline_label crosses;
    BOOST_TEST(append_copies(crosses, capacity - 1, U'a'));
    BOOST_TEST(!nfc::append_decomposed(crosses, 0x00E9));

    // GAG decomposes to three jamo, GA to two.
    detail::inline_label gag;
    BOOST_TEST(append_copies(gag, capacity - 3, U'a'));
    BOOST_TEST(nfc::append_decomposed(gag, 0xAC01));
    BOOST_TEST_EQ(gag.view().size(), capacity);
    detail::inline_label gag_crosses;
    BOOST_TEST(append_copies(gag_crosses, capacity - 2, U'a'));
    BOOST_TEST(!nfc::append_decomposed(gag_crosses, 0xAC01));
    detail::inline_label ga;
    BOOST_TEST(append_copies(ga, capacity - 2, U'a'));
    BOOST_TEST(nfc::append_decomposed(ga, 0xAC00));
    BOOST_TEST_EQ(ga.view().size(), capacity);

    detail::inline_label ascii;
    BOOST_TEST(append_copies(ascii, capacity, U'Z'));
    BOOST_TEST_EQ(ascii.keep_ascii(), std::string(capacity, 'z'));
    BOOST_TEST_EQ(ascii.reset(capacity).size(), capacity);
}

/** A growing label holds max_label_code_points code points inline, without its resource, takes
    the next from it, and keeps whole a code point whose decomposition crosses the edge. */
void the_growing_edge() {
    const std::size_t capacity = detail::max_label_code_points;
    counting_resource resource;
    detail::growing_label label(resource);
    BOOST_TEST(append_copies(label, capacity, U'a'));
    BOOST_TEST_EQ(resource.allocations(), 0U);
    BOOST_TEST(label.push_back(U'a'));
    BOOST_TEST_EQ(resource.allocations(), 1U);  // room for as many again, in one allocation
    BOOST_TEST_EQ(label.view().size(), capacity + 1);

    detail::growing_label crosses(resource);
    BOOST_TEST(append_copies(crosses, capacity - 1, U'a'));
    BOOST_TEST(nfc::append_decomposed(crosses, 0x00E9));
    BOOST_TEST_EQ(crosses.view().size(), capacity + 1);
    nfc::compose_label(crosses);
    BOOST_TEST_EQ(crosses.view().size(), capacity);
    BOOST_TEST(crosses.view().back() == 0x00E9);

    detail::growing_label decoded(resource);
    const std::size_t before = resource.allocations();
    BOOST_TEST_EQ(decoded.reset(capacity).size(), capacity);
    BOOST_TEST_EQ(resource.allocations(), before);
    BOOST_TEST_EQ(decoded.reset(capacity + 1).size(), capacity + 1);
    BOOST_TEST(resource.allocations() > before);
    BOOST_TEST(&detail::fresh_like(decoded).resource() == &resource);
}

/** NFC_Quick_Check's Maybe decomposes the text into the scratch label: a copy of exactly
    max_label_code_points code points fits, one more is exhausted, and a growing label holds it. */
void is_nfc_at_the_edge() {
    const std::size_t capacity = detail::max_label_code_points;
    // In NFC, since U+0301 composes with no e-acute, and three code points once decomposed.
    std::u32string fits(capacity - 3, U'b');
    fits += U"\u00E9\u0301";
    BOOST_TEST(nfc::is_nfc(fits, detail::inline_label{}) == nfc::verdict::yes);
    const std::u32string crosses = U"b" + fits;
    BOOST_TEST(nfc::is_nfc(crosses, detail::inline_label{}) == nfc::verdict::exhausted);
    counting_resource resource;
    BOOST_TEST(nfc::is_nfc(crosses, detail::growing_label(resource)) == nfc::verdict::yes);
    BOOST_TEST(resource.allocations() > 0);
}

/** `count` non-starters out of canonical order: a cycle of marks of six classes, two of class
    230, so that the order of equal classes shows. */
std::u32string marks(std::size_t count) {
    static constexpr std::array<char32_t, 7> cycle{
        U'\u0301',  // 230
        U'\u0316',  // 220
        U'\u0300',  // 230
        U'\u0334',  // 1
        U'\u0327',  // 202
        U'\u031B',  // 216
        U'\u0345',  // 240
    };
    std::u32string text;
    for (std::size_t i = 0; i < count; ++i) {
        text.push_back(cycle.at(i % cycle.size()));
    }
    return text;
}

/** `text` with each run of non-starters stably sorted by combining class, by std::stable_sort. */
std::u32string canonical_order(std::u32string text) {
    const auto non_starter = [](char32_t cp) { return detail::combining_class_of(cp) != 0; };
    const auto by_class = [](char32_t a, char32_t b) {
        return detail::combining_class_of(a) < detail::combining_class_of(b);
    };
    auto run = text.begin();
    while (run != text.end()) {
        run = std::find_if(run, text.end(), non_starter);
        const auto end = std::find_if_not(run, text.end(), non_starter);
        std::stable_sort(run, end, by_class);
        run = end;
    }
    return text;
}

/** Canonical ordering of runs of every length around its thresholds, and of long runs at both
    ends of a text: through a memory resource, asked for storage only by a run longer than an inline
    label, and, for the texts an inline label holds, without one. */
void long_runs_reordered() {
    std::vector<std::u32string> texts;
    for (const std::size_t count : {2U, 31U, 32U, 33U, 250U, 256U, 257U, 1000U}) {
        texts.push_back(U"a" + marks(count) + U"b" + marks(5));
    }
    texts.push_back(marks(40) + U"a" + marks(300) + U"b" + marks(33));
    texts.push_back(marks(256) + U"a" + marks(33) + U"b" + marks(256));
    for (const std::u32string& text : texts) {
        const std::u32string expected = canonical_order(text);
        const bool long_run = nfc::longest_run(text) > detail::max_label_code_points;

        counting_resource resource;
        std::u32string with = text;
        nfc::reorder(std::span<char32_t>(with), resource);
        BOOST_TEST(with == expected);
        BOOST_TEST_EQ(resource.allocations() != 0, long_run);

        if (!long_run) {
            std::u32string without = text;
            nfc::reorder(std::span<char32_t>(without));
            BOOST_TEST(without == expected);
        }
    }
}

/** The nanoseconds of the fastest of three normalizations of `text` into a growing label. */
long long fastest_of_three(std::u32string_view text) {
    auto fastest = std::chrono::nanoseconds::max();
    for (int i = 0; i < 3; ++i) {
        detail::growing_label label(*std::pmr::new_delete_resource());
        const auto start = std::chrono::steady_clock::now();
        BOOST_TEST(normalize_into(label, text));
        const auto elapsed = std::chrono::steady_clock::now() - start;
        fastest = std::min(fastest, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return fastest.count();
}

/**
 A run of non-starters in the order furthest from canonical, a block of class 230 before a block of
 class 220, normalized in less than 20 times the time of a text as long without a non-starter, the
 fastest of three runs each: linear work keeps that ratio near 1 on any machine, and an insertion
 sort, whose moves grow with the square of the run, takes it to hundreds.
*/
void long_runs_in_linear_time() {
    constexpr std::size_t half = 5000;
    const std::u32string hostile =
        U"a" + std::u32string(half, U'\u0301') + std::u32string(half, U'\u0316');
    detail::growing_label label(*std::pmr::new_delete_resource());
    BOOST_TEST(normalize_into(label, hostile));
    // a with U+0301 composes past the U+0316 run, which is of a lower class.
    const std::u32string expected =
        U"\u00E1" + std::u32string(half, U'\u0316') + std::u32string(half - 1, U'\u0301');
    BOOST_TEST(label.view() == expected);

    const long long baseline = fastest_of_three(std::u32string(hostile.size(), U'a'));
    const long long measured = fastest_of_three(hostile);
    BOOST_TEST_LT(measured, 20 * baseline);
}

}  // namespace

int main() {
    every_line_of_normalization_test();
    hangul();
    the_storage();
    an_a_label_kept_as_ascii();
    is_nfc_maybe();
    the_inline_edge();
    the_growing_edge();
    is_nfc_at_the_edge();
    long_runs_reordered();
    long_runs_in_linear_time();
    return boost::report_errors();
}
