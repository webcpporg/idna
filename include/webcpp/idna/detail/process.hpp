// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The label driver uts46.hpp and validity.hpp share: UTS 46 section 4 streamed
 label by label, with or without its mapping, into a sink that takes the ASCII
 form, the Unicode form, or nothing.
*/
#ifndef WEBCPP_IDNA_DETAIL_PROCESS_HPP
#define WEBCPP_IDNA_DETAIL_PROCESS_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/bidi.hpp>
#include <webcpp/idna/detail/context.hpp>
#include <webcpp/idna/detail/label_buffer.hpp>
#include <webcpp/idna/detail/nfc.hpp>
#include <webcpp/idna/detail/properties.hpp>
#include <webcpp/idna/detail/utf8.hpp>
#include <webcpp/idna/errors.hpp>
#include <webcpp/idna/options.hpp>
#include <webcpp/idna/punycode.hpp>

#include <boost/system/error_code.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace webcpp::idna::detail {

/** Whether the driver maps each code point first, or takes the labels as they are given. */
enum class mode : std::uint8_t {
    /** UTS 46 section 4's Map and Normalize, then each label: to_ascii, to_unicode. */
    map,
    /** Each label as it is, split at U+002E only: check_label, check_domain. */
    as_given,
};

/** Which form of each label the driver hands to its sink. */
enum class form : std::uint8_t {
    /** The ASCII form, with the DNS lengths; the first error stops the driver. */
    ascii,
    /** The Unicode form; every error is recorded and the driver goes on. */
    unicode,
};

/** A sink of a caller's span: each piece written whole while it fits, and none after one did
    not. */
class span_sink {
public:
    /** A sink that writes into `output`. */
    explicit span_sink(std::span<char> output) noexcept : output_(output) {}

    /** Writes `piece`, whole, or marks the output cut. */
    void put(std::string_view piece) noexcept {
        if (cut_ || output_.size() - written_ < piece.size()) {
            cut_ = true;
            return;
        }
        std::ranges::copy(piece, output_.begin() + static_cast<std::ptrdiff_t>(written_));
        written_ += piece.size();
    }

    /** The number of chars written. */
    [[nodiscard]] std::size_t written() const noexcept { return written_; }

    /** Whether a piece did not fit. */
    [[nodiscard]] bool cut() const noexcept { return cut_; }

private:
    /** The caller's buffer. */
    std::span<char> output_;

    /** The chars written so far. */
    std::size_t written_ = 0;

    /** Whether a piece did not fit, after which nothing is written. */
    bool cut_ = false;
};

/** A sink of a string of a memory resource, which grows as pieces come. */
class string_sink {
public:
    /** A sink that appends to `output`. */
    explicit string_sink(std::pmr::string& output) noexcept : output_(&output) {}

    /** Appends `piece`. */
    void put(std::string_view piece) { output_->append(piece); }

private:
    /** The string appended to. */
    std::pmr::string* output_;
};

/** A sink that keeps nothing, for the validators. */
struct discard_sink {
    /** Drops `piece`. */
    void put(std::string_view /*piece*/) noexcept {}
};

/**
 The code points a label's Punycode may hold, the encoder's input or the decoder's output, the
 inline capacity: past it, a label is punycode_overflow.

 Tip: RFC 3492's encoder passes over the label once per distinct code point, and its decoder inserts
 each code point among those before it, so their work grows with the square of the label. A growing
 label, in a pmr form, has no other bound; at this one the worst label costs about 4 times one as
 long that Punycode handles in linear time, where 1,024 code points cost 14 times and 4,096 cost 43
 (measured at -O2). The span forms never reach it.
*/
inline constexpr std::size_t max_punycode_code_points = max_label_code_points;

/** Whether `text` holds only code points below U+0080. */
constexpr bool all_ascii(std::u32string_view text) noexcept {
    return std::ranges::all_of(text, [](char32_t cp) { return cp < 0x80; });
}

/**
 Whether `text` starts with "xn--", its letters in either case when `any_case`.

 Tip: only an ASCII letter is lowercased; @ref lower would read U+0278 as its low byte, 'x'.
*/
constexpr bool has_ace_prefix(std::u32string_view text, bool any_case) noexcept {
    if (text.size() < 4) {
        return false;
    }
    const auto letter = [&](std::size_t i) {
        const char32_t cp = text[i];
        return any_case && cp < 0x80 ? static_cast<char32_t>(lower(cp)) : cp;
    };
    return letter(0) == U'x' && letter(1) == U'n' && text[2] == U'-' && text[3] == U'-';
}

/** Whether `cp` is a lowercase ASCII letter, a digit or the hyphen. */
constexpr bool is_ldh(char32_t cp) noexcept {
    return (cp >= U'a' && cp <= U'z') || (cp >= U'0' && cp <= U'9') || cp == U'-';
}

/**
 Whether the driver over a `Label` into a `Sink` throws nothing: every operation it calls on them
 is `noexcept`, as an inline label's and a span's are; a growing label and a string sink may throw
 what their memory resource throws.
*/
template <class Label, class Sink>
inline constexpr bool nothrow_process = noexcept(std::declval<Label&>().push_back(char32_t())) &&
                                        noexcept(std::declval<Label&>().reset(std::size_t())) &&
                                        noexcept(std::declval<Label&>().keep_ascii()) &&
                                        noexcept(fresh_like(std::declval<const Label&>())) &&
                                        noexcept(nfc::compose_label(std::declval<Label&>())) &&
                                        noexcept(std::declval<Sink&>().put(std::string_view()));

/**
 UTS 46 section 4 over a domain, label by label, into a sink.

 Tip: one label's code points are held at a time, in `Label`, so the working storage is one
 label's whatever the domain's length.
*/
template <class Label, class Sink>
class processor {
public:
    /** A driver of `opts` in `m`, handing `f` to `sink`, with `label` as its working storage. */
    processor(const options& opts, mode m, form f, Label& label, Sink& sink) noexcept
        : opts_(opts),
          mode_(m),
          form_(f),
          bounded_(opts.verify_dns_length && f == form::ascii),
          label_(&label),
          sink_(&sink) {}

    /** Processes `domain`, and returns the first error, empty when there was none. */
    boost::system::error_code run(std::string_view domain) noexcept(nothrow_process<Label, Sink>) {
        if (!utf8::is_valid(domain)) {
            return error::invalid_utf8;
        }
        std::size_t i = 0;
        char32_t cp = 0;
        while (utf8::decode(domain, i, cp)) {
            if (!append(cp)) {
                return first_;
            }
        }
        if (end_label(true)) {
            finish();
        }
        return first_;
    }

private:
    /** Records `found` unless an error was recorded before, and says whether the driver goes
        on. */
    bool record(const boost::system::error_code& found) noexcept {
        if (!first_) {
            first_ = found;
        }
        return form_ == form::unicode;
    }

    /** The error of a label beyond the storage: label_too_long when the DNS lengths refuse it
        anyway, as they refuse every such label, storage_exhausted otherwise. */
    [[nodiscard]] error beyond_storage() const noexcept {
        const bool too_long = opts_.verify_dns_length && form_ == form::ascii;
        return too_long ? error::label_too_long : error::storage_exhausted;
    }

    /** Stops the driver at full storage, the label's or the NFC check's copy, keeping an error
        recorded before: no form can go on without the label. */
    bool exhausted() noexcept {
        if (!first_) {
            first_ = beyond_storage();
        }
        stopped_ = true;
        return false;
    }

    /**
     Whether the label at hand is within what the driver holds: the storage's capacity, and the
     inline capacity in the ASCII form under options::verify_dns_length, whatever the storage.

     Tip: no label past the inline capacity passes the DNS lengths, so a growing label is refused
     where an inline one runs out, and both forms report the same first error.
    */
    [[nodiscard]] bool held() const noexcept {
        return !bounded_ || label_->view().size() <= max_label_code_points;
    }

    /** Takes one code point of the input, mapped first in mode::map. */
    bool append(char32_t cp) {
        if (mode_ == mode::as_given) {
            if (cp == U'.') {
                return end_label(false);
            }
            return (label_->push_back(cp) && held()) || exhausted();
        }
        const uts46_entry entry = uts46_of(cp);
        switch (entry.status) {
            case tables::status::ignored: return true;
            case tables::status::mapped:
                if (opts_.transitional_processing && cp == 0x1E9E) {
                    return emit(U's') && emit(U's');
                }
                return emit_all(entry.mapping);
            case tables::status::deviation:
                return opts_.transitional_processing ? emit_all(entry.mapping) : emit(cp);
            default: return emit(cp);
        }
    }

    /** Emits every code point of a mapping. */
    bool emit_all(std::u32string_view mapping) {
        return std::ranges::all_of(mapping, [this](char32_t cp) { return emit(cp); });
    }

    /** Ends the label at U+002E, or appends the decomposition of a mapped code point. */
    bool emit(char32_t cp) {
        if (cp == U'.') {
            return end_label(false);
        }
        return (nfc::append_decomposed(*label_, cp) && held()) || exhausted();
    }

    /** Converts and validates the label held, hands it to the sink, and starts the next. */
    bool end_label(bool last) {
        if (stopped_) {
            return false;
        }
        if (mode_ == mode::map) {
            nfc::compose_label(*label_);
        }
        separator_due_ = labels_ > 0;
        ++labels_;
        const std::u32string_view text = label_->view();
        bool go_on = true;
        if (text.empty()) {
            go_on = empty(last);
        } else if (has_ace_prefix(text, mode_ == mode::as_given)) {
            go_on = ace(text);
        } else {
            go_on = plain(text);
        }
        label_->clear();
        if (go_on && separator_due_) {
            put({});
        }
        return go_on;
    }

    /** UTS 46's empty label: an error under verify_dns_length, but for a final root label in the
        Unicode form (X4_2). */
    bool empty(bool last) noexcept {
        if (!opts_.verify_dns_length || (form_ == form::unicode && last && labels_ > 1)) {
            return true;
        }
        return record(error::empty_label);
    }

    /** Records what @ref validate found, and says whether the driver goes on: never once the
        storage ran out, whose error @ref exhausted has already kept. */
    bool settle(const boost::system::error_code& found) noexcept {
        if (stopped_) {
            return false;
        }
        return !found || record(found);
    }

    /** A label that does not start with "xn--": validated, then written. */
    bool plain(std::u32string_view text) {
        if (!settle(validate(text, opts_.transitional_processing, mode_ == mode::as_given))) {
            return false;
        }
        note_bidi(text);
        return write(text);
    }

    /** A label that starts with "xn--": decoded, validated as a U-label, then written. */
    bool ace(std::u32string_view text) {
        if (!all_ascii(text)) {
            if (form_ == form::unicode) {
                put_unicode(text);
            }
            return record(error::invalid_punycode);
        }
        const std::string_view original = label_->keep_ascii();
        std::string_view encoded = original;
        encoded.remove_prefix(4);
        const std::size_t room = std::min(encoded.size(), max_punycode_code_points);
        const result<std::size_t> decoded = punycode::decode(label_->reset(room), encoded);
        if (decoded.has_error() && decoded.error() == error::output_too_small) {
            return beyond_bound(original);
        }
        if (decoded.has_error()) {
            return undecodable(original, decoded.error());
        }
        label_->shrink(decoded.unsafe_value());
        const std::u32string_view unicode = label_->view();
        if (unicode.empty()) {
            return record(error::invalid_punycode);
        }
        if (all_ascii(unicode) && !record(error::invalid_punycode)) {
            return false;
        }
        if (!settle(validate(unicode, false, true))) {
            return false;
        }
        if (mode_ == mode::as_given && !canonical(unicode, encoded) &&
            !record(error::invalid_punycode)) {
            return false;
        }
        note_bidi(unicode);
        if (mode_ == mode::as_given && form_ == form::ascii) {
            // The round trip held, so the ASCII form is the label as given, lowercased.
            put(original);
            return check_length(original.size());
        }
        return write(unicode);
    }

    /** An "xn--" label whose Punycode decodes past @ref max_punycode_code_points, the decoder's
        room: kept as it is, and punycode_overflow whatever options::ignore_invalid_punycode says,
        since a limit of the implementation is no invalid Punycode. */
    bool beyond_bound(std::string_view original) {
        put(original);
        return record(error::punycode_overflow);
    }

    /** An "xn--" label whose Punycode does not decode: kept as it is, with its error unless
        options::ignore_invalid_punycode, and then to the DNS lengths in the ASCII form alone. */
    bool undecodable(std::string_view original, const boost::system::error_code& failure) {
        put(original);
        if (opts_.ignore_invalid_punycode) {
            return form_ == form::unicode || check_length(original.size());
        }
        return record(failure);
    }

    /** Whether re-encoding `unicode` gives `encoded`, lowercase, back: RFC 5891 section 5.4's
        round trip. */
    static bool canonical(std::u32string_view unicode, std::string_view encoded) noexcept {
        std::size_t at = 0;
        const result<std::size_t> same = detail::punycode::encode(unicode, [&](char c) noexcept {
            if (at == encoded.size() || encoded[at] != c) {
                return false;
            }
            ++at;
            return true;
        });
        return same.has_value() && at == encoded.size();
    }

    /**
     UTS 46 4.1's validity criteria over `text`, which is not empty, with IDNA2008's under
     options::idna2008, in the order of the specification's section 5.3: the first that fails.

     Tip: an NFC check whose copy outgrows the storage stops the driver, as @ref settle then says.
    */
    boost::system::error_code validate(std::u32string_view text, bool transitional,
                                       bool check_nfc) {
        if (check_nfc) {
            // Bounded, the check's copy is held to the inline capacity too, as held() says.
            const nfc::verdict normalized = bounded_ ? nfc::is_nfc(text, inline_label())
                                                     : nfc::is_nfc(text, fresh_like(*label_));
            if (normalized == nfc::verdict::exhausted) {
                exhausted();
                return beyond_storage();
            }
            if (normalized == nfc::verdict::no) {
                return error::not_nfc;
            }
        }
        if (const boost::system::error_code hyphens = check_hyphens(text)) {
            return hyphens;
        }
        if (is_mark(text.front())) {
            return error::leading_combining_mark;
        }
        return check_code_points(text, transitional);
    }

    /** V2 and V3 under options::check_hyphens, V4 otherwise, over `text`, which is not empty. */
    [[nodiscard]] boost::system::error_code check_hyphens(std::u32string_view text) const noexcept {
        if (!opts_.check_hyphens) {
            if (has_ace_prefix(text, true)) {
                return error::ace_prefix;
            }
            return {};
        }
        if (text.size() >= 4 && text[2] == U'-' && text[3] == U'-') {
            return error::hyphen_in_third_and_fourth;
        }
        if (text.front() == U'-' || text.back() == U'-') {
            return error::hyphen_at_start_or_end;
        }
        return {};
    }

    /** V7, U1 and CONTEXTJ, then IDNA2008's NV8, XV8 and CONTEXTO, each over the whole label. */
    [[nodiscard]] boost::system::error_code check_code_points(std::u32string_view text,
                                                              bool transitional) const noexcept {
        const auto allowed = [transitional](char32_t cp) {
            const tables::status status = uts46_of(cp).status;
            return status == tables::status::valid ||
                   (status == tables::status::deviation && !transitional);
        };
        if (!std::ranges::all_of(text, allowed)) {
            return error::disallowed;
        }
        const auto ldh = [](char32_t cp) { return cp >= 0x80 || is_ldh(cp); };
        if (opts_.use_std3_ascii_rules && !std::ranges::all_of(text, ldh)) {
            return error::not_ldh;
        }
        if (opts_.check_joiners && !context::check_joiners(text)) {
            return error::contextj;
        }
        if (!opts_.idna2008) {
            return {};
        }
        if (std::ranges::any_of(text, [](char32_t cp) { return uts46_of(cp).not_idna2008; })) {
            return error::not_idna2008;
        }
        if (!context::check_others(text)) {
            return error::contexto;
        }
        return {};
    }

    /** Notes what a validated label says of the Bidi rule. */
    void note_bidi(std::u32string_view text) noexcept {
        const bidi::label_bidi found = bidi::check(text);
        any_rtl_ = any_rtl_ || found.rtl;
        any_unsatisfied_ = any_unsatisfied_ || !found.satisfied;
    }

    /** Writes a validated label in the driver's form. */
    bool write(std::u32string_view text) {
        if (form_ == form::unicode) {
            put_unicode(text);
            return true;
        }
        return write_ascii(text);
    }

    /** Writes the ASCII form of a label, "xn--" and its Punycode when it is not ASCII, and checks
        its length. */
    bool write_ascii(std::u32string_view text) {
        if (all_ascii(text)) {
            for (const char32_t cp : text) {
                const char c = static_cast<char>(cp);
                put(std::string_view(&c, 1));
            }
            return check_length(text.size());
        }
        if (text.size() > max_punycode_code_points) {
            return record(error::punycode_overflow);
        }
        put("xn--");
        const result<std::size_t> encoded = detail::punycode::encode(text, [this](char c) {
            put(std::string_view(&c, 1));
            return true;
        });
        if (encoded.has_error()) {
            return record(encoded.error());
        }
        return check_length(4 + encoded.unsafe_value());
    }

    /** UTS 46 A4_2 under options::verify_dns_length: a label of at most 63 octets. */
    bool check_length(std::size_t length) noexcept {
        if (opts_.verify_dns_length && length > max_label_length) {
            return record(error::label_too_long);
        }
        return true;
    }

    /**
     Hands a piece of a label to the sink, after the separator before the label when it is due, and
     counts both toward the domain's length, which the ASCII form alone reads.

     Tip: the separator waits for the label's first piece, or its end, so that a label that stops
     the driver leaves none behind.
    */
    void put(std::string_view piece) {
        if (separator_due_) {
            separator_due_ = false;
            ascii_length_ += 1;
            sink_->put(".");
        }
        ascii_length_ += piece.size();
        if (!piece.empty()) {
            sink_->put(piece);
        }
    }

    /** Hands the UTF-8 of `text` to the sink, a code point at a time. */
    void put_unicode(std::u32string_view text) {
        std::array<char, 4> octets{};
        for (const char32_t cp : text) {
            put(std::string_view(octets.data(), utf8::encode(cp, octets.data())));
        }
    }

    /** What is decided once every label is seen: the Bidi rule across labels, then the domain's
        length. */
    void finish() noexcept {
        if (opts_.check_bidi && any_rtl_ && any_unsatisfied_) {
            record(error::bidi);
        }
        if (form_ == form::ascii && opts_.verify_dns_length && ascii_length_ > max_domain_length) {
            record(error::domain_too_long);
        }
    }

    /** The flags of the processing. */
    const options& opts_;

    /** Whether code points are mapped first. */
    mode mode_;

    /** The form handed to the sink. */
    form form_;

    /** Whether a label is held to the inline capacity whatever its storage, as @ref held says. */
    bool bounded_;

    /** The working storage of the label at hand. */
    Label* label_;

    /** Where the output goes. */
    Sink* sink_;

    /** The first error recorded, empty while there is none. */
    boost::system::error_code first_;

    /** The labels ended so far. */
    std::size_t labels_ = 0;

    /** The octets of the ASCII form so far, separators included. */
    std::size_t ascii_length_ = 0;

    /** Whether the label at hand follows another, so that a separator comes before it. */
    bool separator_due_ = false;

    /** Whether a label holds R, AL or AN: the domain is a Bidi domain name. */
    bool any_rtl_ = false;

    /** Whether a validated label breaks the Bidi rule. */
    bool any_unsatisfied_ = false;

    /** Whether the storage ran out, which ends the processing. */
    bool stopped_ = false;
};

/** Runs a @ref processor over `domain`, and returns its first error. */
template <class Label, class Sink>
boost::system::error_code process(std::string_view domain, const options& opts, mode m, form f,
                                  Label& label, Sink& sink) noexcept(nothrow_process<Label, Sink>) {
    processor<Label, Sink> driver(opts, m, f, label, sink);
    return driver.run(domain);
}

}  // namespace webcpp::idna::detail

#endif  // WEBCPP_IDNA_DETAIL_PROCESS_HPP
