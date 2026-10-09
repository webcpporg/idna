// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Normalization Form C (Unicode 17.0.0 section 3.11, UAX 15) within a label's
 storage: canonical decomposition as code points are appended, then canonical
 ordering and canonical composition in place.

 Each step does a bounded amount of work per code point, however long the label
 and its runs of non-starters: a decomposition is at most 4 code points long, a
 composition looks back at one starter, and canonical ordering sorts a run of
 more than a few non-starters by counting, through a buffer on the stack, or,
 for a run longer than an inline label, one from the label's memory resource.
*/
#ifndef WEBCPP_IDNA_DETAIL_NFC_HPP
#define WEBCPP_IDNA_DETAIL_NFC_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/label_buffer.hpp>
#include <webcpp/idna/detail/properties.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace webcpp::idna::detail::nfc {

/** The first Hangul syllable, and the Hangul constants of Unicode section 3.12. */
inline constexpr char32_t s_base = 0xAC00;
/** The first leading consonant jamo. */
inline constexpr char32_t l_base = 0x1100;
/** The first vowel jamo. */
inline constexpr char32_t v_base = 0x1161;
/** The code point before the first trailing consonant jamo. */
inline constexpr char32_t t_base = 0x11A7;
/** The number of leading consonants. */
inline constexpr char32_t l_count = 19;
/** The number of vowels. */
inline constexpr char32_t v_count = 21;
/** The number of trailing consonants, the absent one included. */
inline constexpr char32_t t_count = 28;
/** The syllables per leading consonant. */
inline constexpr char32_t n_count = v_count * t_count;
/** The number of Hangul syllables. */
inline constexpr char32_t s_count = l_count * n_count;

/** Appends the full canonical decomposition of `cp` to `label`, or returns false when the label's
    storage is full, the label's contents being unspecified then. */
template <class Label>
[[nodiscard]] bool append_decomposed(Label& label, char32_t cp) {
    if (cp >= s_base && cp < s_base + s_count) {
        const char32_t index = cp - s_base;
        const char32_t trailing = t_base + (index % t_count);
        if (!label.push_back(l_base + (index / n_count)) ||
            !label.push_back(v_base + (index % n_count / t_count))) {
            return false;
        }
        return trailing == t_base || label.push_back(trailing);
    }
    const std::u32string_view decomposition = decomposition_of(cp);
    if (decomposition.empty()) {
        return label.push_back(cp);
    }
    for (const char32_t part : decomposition) {
        if (!label.push_back(part)) {
            return false;
        }
    }
    return true;
}

/** The longest run of non-starters sorted by insertion, whose moves grow with the square of the
    run: past it, a counting sort moves each code point twice. */
inline constexpr std::size_t max_insertion_run = 32;

/** Sorts the non-starters `run` stably by combining class, by insertion. */
inline void insertion_sort(std::span<char32_t> run) noexcept {
    for (std::size_t i = 1; i < run.size(); ++i) {
        const char32_t cp = run[i];
        const std::uint8_t cc = combining_class_of(cp);
        std::size_t j = i;
        for (; j > 0 && combining_class_of(run[j - 1]) > cc; --j) {
            run[j] = run[j - 1];
        }
        run[j] = cp;
    }
}

/**
 Sorts the non-starters `run` stably by combining class through `buffer`, at least as long as the
 run: a counting sort over the 256 classes.

 Tip: never inlined, so that its counts are on the stack only while a long run is sorted.
*/
BOOST_NOINLINE inline void counting_sort(std::span<char32_t> run,
                                         std::span<char32_t> buffer) noexcept {
    // Once summed, next[c] is where the next code point of class c goes.
    std::array<std::size_t, 257> next{};
    for (const char32_t cp : run) {
        ++next[combining_class_of(cp) + 1U];
    }
    std::partial_sum(next.begin(), next.end(), next.begin());
    for (const char32_t cp : run) {
        buffer[next[combining_class_of(cp)]++] = cp;
    }
    std::ranges::copy(buffer.first(run.size()), run.begin());
}

/**
 Sorts the non-starters `run`, at most as long as an inline label, by counting through a buffer on
 the stack.

 Tip: never inlined, so that the buffer is on the stack only while such a run is sorted, and not in
 the frame of every normalization.
*/
BOOST_NOINLINE inline void counting_sort_on_stack(std::span<char32_t> run) noexcept {
    std::array<char32_t, max_label_code_points> buffer{};
    counting_sort(run, buffer);
}

/**
 Sorts the non-starters `run` stably by combining class: by insertion up to max_insertion_run, by
 counting through the stack up to an inline label's length, and by counting through `spill` beyond,
 which only a growing label lends, as long as its longest run.

 Tip: a run longer than `spill` breaks that precondition, which an assertion states; a build without
 assertions sorts it by insertion, correct and in a time that grows with its square.
*/
inline void sort_run(std::span<char32_t> run, std::span<char32_t> spill) noexcept {
    if (run.size() <= max_insertion_run) {
        insertion_sort(run);
        return;
    }
    if (run.size() <= max_label_code_points) {
        counting_sort_on_stack(run);
        return;
    }
    BOOST_ASSERT(run.size() <= spill.size());
    if (run.size() <= spill.size()) {
        counting_sort(run, spill);
        return;
    }
    insertion_sort(run);
}

/** Puts every run of non-starters of `text` in canonical order, a stable sort by combining class,
    with `spill` for a run longer than an inline label (@ref sort_run). */
inline void reorder_through(std::span<char32_t> text, std::span<char32_t> spill) noexcept {
    std::size_t begin = 0;
    while (begin < text.size()) {
        if (combining_class_of(text[begin]) == 0) {
            ++begin;
            continue;
        }
        std::size_t end = begin + 1;
        while (end < text.size() && combining_class_of(text[end]) != 0) {
            ++end;
        }
        sort_run(text.subspan(begin, end - begin), spill);
        begin = end;
    }
}

/** Puts every run of non-starters of `text`, none longer than an inline label, as in an inline
    label, in canonical order; a longer run is a broken precondition (@ref sort_run). */
inline void reorder(std::span<char32_t> text) noexcept {
    reorder_through(text, {});
}

/** The length of the longest run of non-starters of `text`. */
inline std::size_t longest_run(std::u32string_view text) noexcept {
    std::size_t longest = 0;
    std::size_t current = 0;
    for (const char32_t cp : text) {
        current = combining_class_of(cp) == 0 ? 0 : current + 1;
        longest = std::max(longest, current);
    }
    return longest;
}

/** Puts every run of non-starters of `text` in canonical order, with a buffer from `resource` for
    a run longer than an inline label; may throw what the resource throws. */
inline void reorder(std::span<char32_t> text, std::pmr::memory_resource& resource) {
    const std::size_t longest = longest_run(std::u32string_view(text.data(), text.size()));
    if (longest <= max_label_code_points) {
        reorder(text);
        return;
    }
    std::pmr::vector<char32_t> buffer(longest, &resource);
    reorder_through(text, buffer);
}

/** The primary composite of `first` and `second`, Hangul included, or 0 when there is none. */
constexpr char32_t compose_pair(char32_t first, char32_t second) noexcept {
    if (first >= l_base && first < l_base + l_count && second >= v_base &&
        second < v_base + v_count) {
        return s_base + ((((first - l_base) * v_count) + (second - v_base)) * t_count);
    }
    if (first >= s_base && first < s_base + s_count && (first - s_base) % t_count == 0 &&
        second > t_base && second < t_base + t_count) {
        return first + (second - t_base);
    }
    return composition_of(first, second);
}

/** Composes decomposed, canonically ordered `text` in place, and returns its new length. */
inline std::size_t compose(std::span<char32_t> text) noexcept {
    if (text.empty()) {
        return 0;
    }
    // A text that starts with non-starters has no starter until its first one: they compose with
    // nothing.
    constexpr std::size_t no_starter = std::numeric_limits<std::size_t>::max();
    std::size_t starter = combining_class_of(text[0]) == 0 ? 0 : no_starter;
    int last_class = 0;
    std::size_t written = 1;
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char32_t cp = text[i];
        const int cc = combining_class_of(cp);
        const char32_t composite = starter == no_starter ? 0 : compose_pair(text[starter], cp);
        if (composite != 0 && (last_class < cc || last_class == 0)) {
            text[starter] = composite;
            continue;
        }
        if (cc == 0) {
            starter = written;
        }
        last_class = cc;
        text[written++] = cp;
    }
    return written;
}

/** A label whose storage grows into a memory resource, which also lends canonical ordering its
    buffer for a run of non-starters longer than an inline label. */
template <class Label>
concept growing = requires(const Label& label) { label.resource(); };

/** Puts the decomposed code points of `label` in NFC, in place; a growing label may throw what
    its resource throws. */
template <class Label>
void compose_label(Label& label) noexcept(!growing<Label>) {
    if constexpr (growing<Label>) {
        reorder(label.span(), label.resource());
    } else {
        reorder(label.span());
    }
    label.shrink(compose(label.span()));
}

/** What @ref is_nfc finds. */
enum class verdict : std::uint8_t {
    /** The text is in NFC. */
    yes,
    /** The text is not in NFC. */
    no,
    /** The scratch storage could not hold the text's normalization. */
    exhausted,
};

/**
 Whether `text` is in NFC: NFC_Quick_Check answers Yes or No at once, and for Maybe, `text` is
 normalized into the empty label `scratch` and compared.
*/
template <class Label>
[[nodiscard]] verdict is_nfc(std::u32string_view text, Label scratch) {
    bool maybe = false;
    std::uint8_t last_class = 0;
    for (const char32_t cp : text) {
        const std::uint8_t cc = combining_class_of(cp);
        if (cc != 0 && last_class > cc) {
            return verdict::no;
        }
        last_class = cc;
        const tables::quick_check check = nfc_quick_check_of(cp);
        if (check == tables::quick_check::no) {
            return verdict::no;
        }
        maybe = maybe || check == tables::quick_check::maybe;
    }
    if (!maybe) {
        return verdict::yes;
    }
    for (const char32_t cp : text) {
        if (!append_decomposed(scratch, cp)) {
            return verdict::exhausted;
        }
    }
    compose_label(scratch);
    return scratch.view() == text ? verdict::yes : verdict::no;
}

}  // namespace webcpp::idna::detail::nfc

#endif  // WEBCPP_IDNA_DETAIL_NFC_HPP
