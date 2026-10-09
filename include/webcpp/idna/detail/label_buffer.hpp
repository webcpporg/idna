// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The working storage of one label's code points: inline, and for the forms that
 take a memory resource, a vector of that resource beyond the inline capacity.
*/
#ifndef WEBCPP_IDNA_DETAIL_LABEL_BUFFER_HPP
#define WEBCPP_IDNA_DETAIL_LABEL_BUFFER_HPP

#include <webcpp/idna/config.hpp>

#include <boost/assert.hpp>

#include <array>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace webcpp::idna::detail {

/**
 The code points a label holds inline: every label that can pass the DNS lengths fits, since an
 A-label decodes to at most 59 code points and a full canonical decomposition is at most 4 long.
*/
inline constexpr std::size_t max_label_code_points = 256;

/** The char of the ASCII code point `cp`, an ASCII capital letter in lowercase: an A-label's
    letters are compared in either case (RFC 5890 section 2.3.2.1). */
constexpr char lower(char32_t cp) noexcept {
    const char c = static_cast<char>(cp);
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

/** A label's code points in fixed storage, which never allocates. */
class inline_label {
public:
    /** Appends `cp`, or returns false when the storage is full. */
    [[nodiscard]] bool push_back(char32_t cp) noexcept {
        if (size_ == data_.size()) {
            return false;
        }
        data_[size_++] = cp;
        return true;
    }

    /** Keeps the first `size` code points, `size` being at most the current size. */
    void shrink(std::size_t size) noexcept {
        BOOST_ASSERT(size <= size_);
        size_ = size;
    }

    /** Removes every code point. */
    void clear() noexcept { size_ = 0; }

    /** The code points, which may be changed in place. */
    [[nodiscard]] std::span<char32_t> span() noexcept { return {data_.data(), size_}; }

    /** The code points. */
    [[nodiscard]] std::u32string_view view() const noexcept { return {data_.data(), size_}; }

    /** Makes the label `size` code points long, their values unspecified, for a decoder to
        write; `size` is at most the inline capacity. */
    [[nodiscard]] std::span<char32_t> reset(std::size_t size) noexcept {
        BOOST_ASSERT(size <= data_.size());
        size_ = size;
        return span();
    }

    /** Copies the label, whose code points are all ASCII, to chars of its own with its letters
        in lowercase, and returns them. */
    [[nodiscard]] std::string_view keep_ascii() noexcept {
        for (std::size_t i = 0; i < size_; ++i) {
            ascii_[i] = lower(data_[i]);
        }
        return {ascii_.data(), size_};
    }

private:
    /** The code points. */
    std::array<char32_t, max_label_code_points> data_{};

    /** The chars @ref keep_ascii copies the label to. */
    std::array<char, max_label_code_points> ascii_{};

    /** The number of code points held. */
    std::size_t size_ = 0;
};

/** A label's code points inline, then in a vector of a memory resource once they outgrow it. */
class growing_label {
public:
    /** An empty label whose storage beyond the inline capacity comes from `resource`. */
    explicit growing_label(std::pmr::memory_resource& resource)
        : spill_(&resource), ascii_(&resource) {}

    /** Appends `cp`, moving the code points to the vector when the inline storage is full. */
    [[nodiscard]] bool push_back(char32_t cp) {
        if (!spilled_ && size_ < inline_.size()) {
            inline_[size_++] = cp;
            return true;
        }
        if (!spilled_) {
            // Room for as many again, so that the push_back below does not allocate a second time.
            spill_.reserve(2 * inline_.size());
            spill_.assign(inline_.begin(), inline_.end());
            spilled_ = true;
        }
        spill_.push_back(cp);
        return true;
    }

    /** Keeps the first `size` code points, `size` being at most the current size. */
    void shrink(std::size_t size) noexcept {
        BOOST_ASSERT(size <= view().size());
        if (spilled_) {
            spill_.erase(spill_.begin() + static_cast<std::ptrdiff_t>(size), spill_.end());
        } else {
            size_ = size;
        }
    }

    /** Removes every code point, and goes back to the inline storage. */
    void clear() noexcept {
        size_ = 0;
        spill_.clear();
        spilled_ = false;
    }

    /** The code points, which may be changed in place. */
    [[nodiscard]] std::span<char32_t> span() noexcept {
        return spilled_ ? std::span<char32_t>(spill_) : std::span<char32_t>(inline_.data(), size_);
    }

    /** The code points. */
    [[nodiscard]] std::u32string_view view() const noexcept {
        return spilled_ ? std::u32string_view(spill_.data(), spill_.size())
                        : std::u32string_view(inline_.data(), size_);
    }

    /** Makes the label `size` code points long, their values unspecified, for a decoder to
        write. */
    [[nodiscard]] std::span<char32_t> reset(std::size_t size) {
        clear();
        if (size <= inline_.size()) {
            size_ = size;
        } else {
            spill_.assign(size, U'\0');
            spilled_ = true;
        }
        return span();
    }

    /** Copies the label, whose code points are all ASCII, to chars of its own with its letters
        in lowercase, and returns them. */
    [[nodiscard]] std::string_view keep_ascii() {
        ascii_.clear();
        for (const char32_t cp : view()) {
            ascii_.push_back(lower(cp));
        }
        return ascii_;
    }

    /** The memory resource the label grows into. */
    [[nodiscard]] std::pmr::memory_resource& resource() const noexcept {
        return *spill_.get_allocator().resource();
    }

private:
    /** The code points, until they outgrow it. */
    std::array<char32_t, max_label_code_points> inline_{};

    /** The number of code points held inline. */
    std::size_t size_ = 0;

    /** The code points, once they outgrew the inline storage. */
    std::pmr::vector<char32_t> spill_;

    /** The chars @ref keep_ascii copies the label to. */
    std::pmr::string ascii_;

    /** Whether the code points are in `spill_`. */
    bool spilled_ = false;
};

/** An empty inline label, the scratch storage of a check over an inline label. */
inline inline_label fresh_like(const inline_label& /*label*/) noexcept {
    return {};
}

/** An empty label of the resource `label` grows into, the scratch storage of a check over it. */
inline growing_label fresh_like(const growing_label& label) {
    return growing_label(label.resource());
}

}  // namespace webcpp::idna::detail

#endif  // WEBCPP_IDNA_DETAIL_LABEL_BUFFER_HPP
