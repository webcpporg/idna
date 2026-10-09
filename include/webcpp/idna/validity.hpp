// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 IDNA2008's validity of a label or a domain name as it is given, without
 UTS 46's mapping: RFC 5891 section 5.4, RFC 5892 and RFC 5893.

 @see "Validity", in the guide.
*/
#ifndef WEBCPP_IDNA_VALIDITY_HPP
#define WEBCPP_IDNA_VALIDITY_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/label_buffer.hpp>
#include <webcpp/idna/detail/process.hpp>
#include <webcpp/idna/detail/utf8.hpp>
#include <webcpp/idna/errors.hpp>
#include <webcpp/idna/options.hpp>

#include <boost/system/error_code.hpp>

#include <string_view>

namespace webcpp::idna {

/**
 Checks one label, in UTF-8, as it is given: a U-label, an A-label or an
 NR-LDH label.

 An A-label, "xn--" in any ASCII case, is decoded, validated as a U-label,
 re-encoded and compared with its input, ASCII letters in either case. Nothing
 else is mapped, so a label not in NFC, or holding a code point UTS 46 would
 map (an ASCII capital letter outside an A-label among them), is refused. The
 Bidi rule is applied to the label as to a domain of one label. An empty label
 is refused under `opts.verify_dns_length` and accepted without it, as UTS 46
 says. Nothing allocates.

 @param label The label, without a separator.
 @param opts The checks to apply; the defaults are the strict profile.
 @return Nothing, or the first @ref error found, in the order of the guide:
 @ref error::disallowed for a U+002E inside `label`, @ref error::storage_exhausted
 for a label beyond the inline storage when `opts.verify_dns_length` is off.
 @see "Validity", in the guide.
*/
[[nodiscard]] inline result<void> check_label(std::string_view label,
                                              const options& opts = options()) noexcept {
    if (!detail::utf8::is_valid(label)) {
        return error::invalid_utf8;
    }
    if (label.find('.') != std::string_view::npos) {
        return error::disallowed;
    }
    detail::inline_label storage;
    detail::discard_sink sink;
    const boost::system::error_code found =
        detail::process(label, opts, detail::mode::as_given, detail::form::ascii, storage, sink);
    if (found) {
        return found;
    }
    return {};
}

/**
 Checks a domain name, in UTF-8, as it is given: every label as
 @ref check_label checks it, the Bidi rule across the labels, and the DNS
 lengths.

 Labels are separated by U+002E alone. Nothing allocates.

 @param domain The domain name.
 @param opts The checks to apply; the defaults are the strict profile.
 @return Nothing, or the first @ref error found: the labels' in their order,
 then @ref error::bidi, then @ref error::domain_too_long. @ref
 error::storage_exhausted for a label beyond the inline storage when
 `opts.verify_dns_length` is off.
 @see "Validity", in the guide.
 @see "Limits", in the guide.
*/
[[nodiscard]] inline result<void> check_domain(std::string_view domain,
                                               const options& opts = options()) noexcept {
    detail::inline_label storage;
    detail::discard_sink sink;
    const boost::system::error_code found =
        detail::process(domain, opts, detail::mode::as_given, detail::form::ascii, storage, sink);
    if (found) {
        return found;
    }
    return {};
}

/**
 Whether @ref check_label finds no error in `label`.

 @param label The label.
 @param opts The checks to apply.
 @return `check_label(label, opts).has_value()`.
 @see "Validity", in the guide.
*/
[[nodiscard]] inline bool is_valid_label(std::string_view label,
                                         const options& opts = options()) noexcept {
    return check_label(label, opts).has_value();
}

/**
 Whether @ref check_domain finds no error in `domain`.

 @param domain The domain name.
 @param opts The checks to apply.
 @return `check_domain(domain, opts).has_value()`.
 @see "Validity", in the guide.
*/
[[nodiscard]] inline bool is_valid_domain(std::string_view domain,
                                          const options& opts = options()) noexcept {
    return check_domain(domain, opts).has_value();
}

}  // namespace webcpp::idna

#endif  // WEBCPP_IDNA_VALIDITY_HPP
