// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The Unicode properties idna reads, as lookups over the generated tables: each a
 binary search over sorted runs, with no state and no allocation.

 Every code point a lookup takes is at most U+10FFFF, as the UTF-8 and Punycode
 decoders guarantee: a packed key keeps 24 bits of a code point in a run list and
 21 in a composition pair, so a larger value would read another code point's value.
*/
#ifndef WEBCPP_IDNA_DETAIL_PROPERTIES_HPP
#define WEBCPP_IDNA_DETAIL_PROPERTIES_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/tables.hpp>
#include <webcpp/idna/options.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace webcpp::idna::detail {

// The tables and options.hpp name one version of Unicode: a move to another changes both.
static_assert(tables::unicode_version == idna::unicode_version);

/** A code point's entry in the UTS 46 mapping table. */
struct uts46_entry {
    /** The code point's status. */
    tables::status status = tables::status::disallowed;

    /** Whether UTS 46 marks it NV8 or XV8: DISALLOWED in IDNA2008. */
    bool not_idna2008 = false;

    /** What a mapped or deviation code point is replaced by; empty for the others. */
    std::u32string_view mapping;
};

/**
 The index of the last element of the sorted `values` that is at most `key`: a run list's run of a
 code point, whose first element is always 0.

 Tip: an index rather than an iterator, which is a pointer on some standard libraries and a class
 on others, so that no declaration depends on which.
*/
template <class T, std::size_t N>
constexpr std::size_t last_at_most(const std::array<T, N>& values, T key) noexcept {
    return static_cast<std::size_t>(std::ranges::upper_bound(values, key) - values.begin()) - 1;
}

/** The index of `key` in the sorted `values`, or N when it is not there. */
template <class T, std::size_t N>
constexpr std::size_t index_of(const std::array<T, N>& values, T key) noexcept {
    const auto at =
        static_cast<std::size_t>(std::ranges::lower_bound(values, key) - values.begin());
    return at < N && values[at] == key ? at : N;
}

/** The value of the run of packed `runs` that holds `cp`: the low 8 bits of the last run whose
    first code point is at most `cp`. */
template <std::size_t N>
constexpr std::uint8_t packed_value(const std::array<std::uint32_t, N>& runs,
                                    char32_t cp) noexcept {
    const std::uint32_t key = static_cast<std::uint32_t>(cp) << 8U | 0xFFU;
    return static_cast<std::uint8_t>(runs[last_at_most(runs, key)] & 0xFFU);
}

/** The UTS 46 status, IDNA2008 mark and mapping of `cp`. */
constexpr uts46_entry uts46_of(char32_t cp) noexcept {
    const std::uint64_t key = static_cast<std::uint64_t>(cp) << 32U | 0xFFFFFFFFU;
    const auto value =
        static_cast<std::uint32_t>(tables::uts46_runs[last_at_most(tables::uts46_runs, key)]);
    return {
        .status = static_cast<tables::status>(value & 0x7U),
        .not_idna2008 = (value & 0x8U) != 0,
        .mapping =
            std::u32string_view(tables::uts46_mappings.data() + (value >> 9U), value >> 4U & 0x1FU),
    };
}

/** The Bidi_Class of `cp`, as RFC 5893 names it. */
constexpr tables::bidi_class bidi_class_of(char32_t cp) noexcept {
    return static_cast<tables::bidi_class>(packed_value(tables::bidi_classes, cp));
}

/** The Joining_Type of `cp`, as RFC 5892 A.1 names it. */
constexpr tables::joining_type joining_type_of(char32_t cp) noexcept {
    return static_cast<tables::joining_type>(packed_value(tables::joining_types, cp));
}

/** The script of `cp`, among those RFC 5892 A.4 to A.7 name. */
constexpr tables::script script_of(char32_t cp) noexcept {
    return static_cast<tables::script>(packed_value(tables::scripts, cp));
}

/** The Canonical_Combining_Class of `cp`. */
constexpr std::uint8_t combining_class_of(char32_t cp) noexcept {
    return packed_value(tables::combining_classes, cp);
}

/** Whether `cp` is of General_Category Mark (Mn, Mc or Me). */
constexpr bool is_mark(char32_t cp) noexcept {
    return packed_value(tables::marks, cp) != 0;
}

/** The NFC_Quick_Check of `cp`. */
constexpr tables::quick_check nfc_quick_check_of(char32_t cp) noexcept {
    return static_cast<tables::quick_check>(packed_value(tables::nfc_quick_checks, cp));
}

/** The full canonical decomposition of `cp`, empty when it has none or is a Hangul syllable. */
constexpr std::u32string_view decomposition_of(char32_t cp) noexcept {
    const std::size_t at = index_of(tables::decomposition_keys, cp);
    if (at == tables::decomposition_keys.size()) {
        return {};
    }
    const std::uint32_t slice = tables::decomposition_slices[at];
    return {tables::decomposition_data.data() + (slice >> 3U), slice & 0x7U};
}

/** The primary composite of the pair `first`, `second`, or 0 when there is none, Hangul aside. */
constexpr char32_t composition_of(char32_t first, char32_t second) noexcept {
    const std::uint64_t key = static_cast<std::uint64_t>(first) << 21U | second;
    const std::size_t at = index_of(tables::composition_keys, key);
    if (at == tables::composition_keys.size()) {
        return 0;
    }
    return tables::composition_values[at];
}

}  // namespace webcpp::idna::detail

#endif  // WEBCPP_IDNA_DETAIL_PROPERTIES_HPP
