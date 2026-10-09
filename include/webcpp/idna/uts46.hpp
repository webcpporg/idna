// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 UTS 46 processing with its mapping: ToASCII (section 4.2) and ToUnicode
 (section 4.3), each into a caller's buffer or into a string of a memory
 resource.

 @see "Converting names", in the guide.
*/
#ifndef WEBCPP_IDNA_UTS46_HPP
#define WEBCPP_IDNA_UTS46_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/label_buffer.hpp>
#include <webcpp/idna/detail/process.hpp>
#include <webcpp/idna/errors.hpp>
#include <webcpp/idna/options.hpp>

#include <boost/system/error_code.hpp>

#include <cstddef>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

namespace webcpp::idna {

/**
 Converts a domain name to its ASCII form (UTS 46 section 4.2), into a
 caller's buffer.

 The name is mapped, normalized to NFC, split into labels, and each label is
 validated and, when it is not ASCII, written as "xn--" and its Punycode. With
 `opts.verify_dns_length`, the default, the result never exceeds
 @ref max_domain_length, so a `std::array<char, max_domain_length>` always
 holds it. Nothing allocates.

 @param output Where the ASCII form is written.
 @param domain The domain name, in UTF-8.
 @param opts The processing's flags; the defaults are the strict profile.
 @return The number of chars written, or the first @ref error found, a
 label's in their order, then @ref error::bidi, @ref error::domain_too_long,
 and @ref error::output_too_small last. @ref error::storage_exhausted for a
 label beyond the inline storage when `opts.verify_dns_length` is off.
 @note After an error, the contents of `output` are unspecified.
 @see "Converting names", in the guide.
 @see "Limits", in the guide.
*/
[[nodiscard]] inline result<std::size_t> to_ascii(std::span<char> output, std::string_view domain,
                                                  const options& opts = options()) noexcept {
    detail::inline_label label;
    detail::span_sink sink(output);
    const boost::system::error_code found =
        detail::process(domain, opts, detail::mode::map, detail::form::ascii, label, sink);
    if (found) {
        return found;
    }
    if (sink.cut()) {
        return error::output_too_small;
    }
    return sink.written();
}

/**
 Converts a domain name to its ASCII form into a string of the given memory
 resource, which also holds a label beyond the inline working storage.

 Under `opts.verify_dns_length` no label beyond the inline storage can pass,
 so such a label is refused as the span form refuses it, and both forms report
 the same error. Without it, a label of any length is converted, but a label
 of more than 256 code points that is not ASCII, or an A-label whose Punycode
 decodes to more, is @ref error::punycode_overflow, whatever
 `opts.ignore_invalid_punycode` says, since Punycode's work grows with the
 square of a label, and the bound keeps it a small multiple of the rest of the
 processing.

 @param domain The domain name, in UTF-8.
 @param opts The processing's flags.
 @param resource Where the string's memory, and a long label's, comes from.
 @return The ASCII form, or the first @ref error, as the span form orders them.
 @note Throws only what `resource` throws when it cannot allocate.
 @see "Allocation", in the guide.
*/
[[nodiscard]] inline result<std::pmr::string> to_ascii(
    std::string_view domain, const options& opts = options(),
    std::pmr::memory_resource& resource = *std::pmr::get_default_resource()) {
    detail::growing_label label(resource);
    std::pmr::string out(&resource);
    detail::string_sink sink(out);
    const boost::system::error_code found =
        detail::process(domain, opts, detail::mode::map, detail::form::ascii, label, sink);
    if (found) {
        return found;
    }
    return out;
}

/** What @ref to_unicode wrote into a caller's buffer, and the first error UTS 46 recorded. */
struct unicode_result {
    /** The number of chars written. */
    std::size_t size = 0;

    /** Empty when the processing succeeded; else its first @ref error, or
        @ref error::output_too_small when the buffer could not hold the whole result. */
    boost::system::error_code error;
};

/**
 Converts a domain name to its Unicode form (UTS 46 section 4.3), into a
 caller's buffer, as far as it can.

 As UTS 46 says, the conversion goes on after an error: each label is written
 as the processing leaves it, mapped and normalized, an A-label decoded when
 its Punycode decodes and kept as it is otherwise; no U+FFFD is inserted.
 Nothing allocates.

 @param output Where the Unicode form is written, in UTF-8; the writing stops
 at a code point boundary when it is full.
 @param domain The domain name, in UTF-8; ill-formed UTF-8 writes nothing.
 @param opts The processing's flags; the defaults are the strict profile.
 @return What was written, and the first error recorded; a label beyond the
 inline storage stops the writing there with @ref error::storage_exhausted,
 after any earlier label's error.
 @see "Converting names", in the guide.
 @see "Limits", in the guide.
*/
[[nodiscard]] inline unicode_result to_unicode(std::span<char> output, std::string_view domain,
                                               const options& opts = options()) noexcept {
    detail::inline_label label;
    detail::span_sink sink(output);
    const boost::system::error_code found =
        detail::process(domain, opts, detail::mode::map, detail::form::unicode, label, sink);
    if (sink.cut()) {
        return {.size = sink.written(), .error = error::output_too_small};
    }
    return {.size = sink.written(), .error = found};
}

/** The string @ref to_unicode produced into a memory resource, and the first error UTS 46
    recorded. */
struct unicode_string {
    /** The Unicode form, in UTF-8. */
    std::pmr::string value;

    /** Empty when the processing succeeded; else its first @ref error. */
    boost::system::error_code error;
};

/**
 Converts a domain name to its Unicode form into a string of the given memory
 resource, as far as it can.

 A label of any length is converted, but an A-label whose Punycode decodes to
 more than 256 code points is kept as it is, with
 @ref error::punycode_overflow whatever `opts.ignore_invalid_punycode` says,
 since the decoder's work grows with the square of the label and a limit of
 the implementation is no invalid Punycode.

 @param domain The domain name, in UTF-8.
 @param opts The processing's flags.
 @param resource Where the string's memory, and a long label's, comes from.
 @return The Unicode form, and the first error recorded.
 @note Throws only what `resource` throws when it cannot allocate.
 @see "Allocation", in the guide.
*/
[[nodiscard]] inline unicode_string to_unicode(
    std::string_view domain, const options& opts = options(),
    std::pmr::memory_resource& resource = *std::pmr::get_default_resource()) {
    detail::growing_label label(resource);
    unicode_string out{.value = std::pmr::string(&resource), .error = {}};
    detail::string_sink sink(out.value);
    out.error =
        detail::process(domain, opts, detail::mode::map, detail::form::unicode, label, sink);
    return out;
}

}  // namespace webcpp::idna

#endif  // WEBCPP_IDNA_UTS46_HPP
