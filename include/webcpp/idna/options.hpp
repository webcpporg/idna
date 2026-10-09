// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The options of every conversion and validation, the length limits of the DNS,
 and the version of Unicode the tables follow.

 @see "Converting names", in the guide.
*/
#ifndef WEBCPP_IDNA_OPTIONS_HPP
#define WEBCPP_IDNA_OPTIONS_HPP

#include <webcpp/idna/config.hpp>

#include <cstddef>
#include <string_view>

namespace webcpp::idna {

/**
 The flags of UTS 46 section 4, and IDNA2008's stricter validity.

 The defaults are the strict profile: every check of UTS 46 on, nontransitional
 processing, the DNS lengths verified, and IDNA2008's validity on top. An
 aggregate, so a caller names only what changes:
 `idna::options{.idna2008 = false}` is UTS 46 alone.

 @see "Converting names", in the guide.
*/
struct options {
    /** CheckHyphens: no hyphens in the third and fourth positions, none at either end (UTS 46
        V2, V3); when false, only the "xn--" prefix is refused (V4). */
    bool check_hyphens = true;

    /** CheckBidi: every label of a Bidi domain name satisfies RFC 5893 section 2. */
    bool check_bidi = true;

    /** CheckJoiners: the joiners U+200C and U+200D satisfy RFC 5892 A.1 and A.2. */
    bool check_joiners = true;

    /** UseSTD3ASCIIRules: the only ASCII code points are a-z, 0-9 and the hyphen (UTS 46 U1). */
    bool use_std3_ascii_rules = true;

    /** VerifyDnsLength: labels of 1 to 63 octets, a name of 1 to 253, and no empty root label. */
    bool verify_dns_length = true;

    /** Transitional_Processing, which UTS 46 deprecates: the deviations are mapped. */
    bool transitional_processing = false;

    /** IgnoreInvalidPunycode: an "xn--" label that does not decode is kept as it is, without an
        error. */
    bool ignore_invalid_punycode = false;

    /** IDNA2008's validity on top of UTS 46: a code point UTS 46 marks NV8 or XV8 is refused, and
        the CONTEXTO rules of RFC 5892 A.3 to A.9 apply. */
    bool idna2008 = true;
};

/** The longest label of a domain name in its ASCII form, in octets (RFC 1034 section 3.1). */
inline constexpr std::size_t max_label_length = 63;

/** The longest domain name in its ASCII form, in octets, without a trailing root label. */
inline constexpr std::size_t max_domain_length = 253;

/** The version of Unicode whose data the tables were generated from. */
inline constexpr std::string_view unicode_version = "17.0.0";

}  // namespace webcpp::idna

#endif  // WEBCPP_IDNA_OPTIONS_HPP
