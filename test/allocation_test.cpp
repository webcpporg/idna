// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Tests what idna allocates, by counting every call of the global operator new this program makes:
// nothing in the span forms and the validators, whatever the input, and in the pmr forms only what
// their memory resource allocates: the page's guarantee 1, idna-invariant-1.

#include <webcpp/idna.hpp>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>
#include <string_view>

#ifdef _MSC_VER
#include <malloc.h>
#endif

namespace idna = webcpp::idna;

namespace {

// The calls of the global operator new this program has made. It is mutable global state, which
// no library holds: it is this test program's own instrument, the one place where the replaced
// operator new below can record what it sees.
std::size_t global_allocations = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

// The replacements of the global operator new and operator delete, for this program alone, which
// count every allocation, an over-aligned one included. The standard library declares them with
// parameter names reserved to the implementation, which a program may not use, hence the exemption
// on each. None is ever inlined: GCC, seeing the malloc or the free inside one, would pair it with
// the other's call and warn of a mismatch (-Wmismatched-new-delete).

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size) {
    ++global_allocations;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        std::abort();
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory) noexcept {
    std::free(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/) noexcept {
    std::free(memory);
}

namespace {

/**
 Memory of `size` bytes aligned to `alignment`: from aligned_alloc, whose size must be a multiple
 of the alignment; or, under _MSC_VER, from _aligned_malloc.

 Tip: the UCRT of MSVC has no aligned_alloc, and memory from its _aligned_malloc is given back with
 _aligned_free, never free, hence the two below. libstdc++'s new_delete_resource asks for the
 alignment of what it allocates, 4 for char32_t, and macOS's aligned_alloc refuses an alignment
 below a pointer's, so the alignment is raised to that.
*/
void* aligned_memory(std::size_t size, std::size_t requested) {
    const std::size_t alignment = requested < sizeof(void*) ? sizeof(void*) : requested;
    const std::size_t rounded =
        size == 0 ? alignment : (size + alignment - 1) / alignment * alignment;
#ifdef _MSC_VER
    return _aligned_malloc(rounded, alignment);
#else
    return std::aligned_alloc(alignment, rounded);
#endif
}

/** Gives back what aligned_memory took. */
void free_aligned_memory(void* memory) noexcept {
#ifdef _MSC_VER
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

}  // namespace

/** The aligned allocation, from aligned_memory. */
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void* operator new(std::size_t size, std::align_val_t alignment) {
    ++global_allocations;
    void* memory = aligned_memory(size, static_cast<std::size_t>(alignment));
    if (memory == nullptr) {
        std::abort();
    }
    return memory;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::align_val_t /*alignment*/) noexcept {
    free_aligned_memory(memory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
BOOST_NOINLINE void operator delete(void* memory, std::size_t /*size*/,
                                    std::align_val_t /*alignment*/) noexcept {
    free_aligned_memory(memory);
}

namespace {

/** A memory resource that counts what it hands out, and takes it from the global operator new
    through std::pmr::new_delete_resource. */
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

/** Domain names for every path of the conversions and the validators: long and many labels, every
    kind of failure, RTL labels, joiners, A-labels, and the NFC check's scratch copy. The Punycode
    decoder takes each as it is. */
std::array<std::string, 14> hostile_inputs() {
    std::string many_labels;
    for (int i = 0; i < 500; ++i) {
        many_labels += "a.";
    }
    return {
        "b\303\274cher.example",
        "xn--bcher-kva.EXAMPLE",
        std::string(300, 'a'),
        many_labels,
        "\303\050",
        "xn--zz.xn--.xn---9uc.xn--ls8h",
        "0a.\327\220.\331\204\330\247",
        "a\342\200\214b.\340\244\225\340\245\215\342\200\215",
        "e\314\201.\303\251\314\201",
        "a\302\267l.\343\203\273",
        "\357\274\241\343\200\202\357\274\242",
        "ab--cd.-x-.a_b",
        std::string(70, 'a') + "\303\274",
        "",
    };
}

/** Code points for every path of the Punycode encoder: a success, a surrogate, an overflow of
    its arithmetic, and a result longer than the output of exercise_span_forms. */
std::array<std::u32string, 4> encoder_inputs() {
    std::u32string overflow(3855, U'a');
    overflow.push_back(0x10FFFF);
    return {
        U"b\u00FCcher",
        U"ab\xD800",
        overflow,
        std::u32string(2000, U'a') + U"\u00FC",
    };
}

/**
 Every span form and validator over every input, with the strict profile, UTS 46 alone and no DNS
 lengths, and the Punycode encoder over every code point input: the number of results that
 succeeded.

 Tip: the count is checked, so no optimizer may drop a call whose result nothing reads.
*/
std::size_t exercise_span_forms(const std::array<std::string, 14>& inputs,
                                const std::array<std::u32string, 4>& encoded) {
    std::array<char, 1024> out{};
    std::array<char32_t, 1024> code_points{};
    std::size_t succeeded = 0;
    for (const idna::options& opts : {
             idna::options(),
             idna::options{.idna2008 = false},
             idna::options{.verify_dns_length = false},
         }) {
        for (const std::string& input : inputs) {
            succeeded += idna::to_ascii(out, input, opts).has_value() ? 1 : 0;
            succeeded += idna::to_unicode(out, input, opts).error ? 0 : 1;
            succeeded += idna::is_valid_domain(input, opts) ? 1 : 0;
            succeeded += idna::is_valid_label(input, opts) ? 1 : 0;
            succeeded += idna::punycode::decode(code_points, input).has_value() ? 1 : 0;
        }
    }
    for (const std::u32string& input : encoded) {
        succeeded += idna::punycode::encode(out, input).has_value() ? 1 : 0;
    }
    return succeeded;
}

/** The span forms and the validators allocate nothing, whatever the input. */
void span_forms_allocate_nothing() {
    const std::array<std::string, 14> inputs = hostile_inputs();
    const std::array<std::u32string, 4> encoded = encoder_inputs();
    const std::size_t before = global_allocations;
    const std::size_t succeeded = exercise_span_forms(inputs, encoded);
    BOOST_TEST_EQ(global_allocations, before);
    BOOST_TEST(succeeded > 0);
}

/** The pmr forms allocate through their resource alone: every call of the global operator new is
    one of the resource's, for the result or a long label's storage. */
void pmr_forms_allocate_through_their_resource() {
    const std::array<std::string, 14> inputs = hostile_inputs();
    const std::array<std::u32string, 4> encoded = encoder_inputs();
    counting_resource resource;
    const std::size_t before = global_allocations;
    for (const std::string& input : inputs) {
        static_cast<void>(
            idna::to_ascii(input, idna::options{.verify_dns_length = false}, resource));
        static_cast<void>(idna::to_unicode(input, idna::options(), resource));
        static_cast<void>(idna::punycode::decode(input, resource));
    }
    for (const std::u32string& input : encoded) {
        static_cast<void>(idna::punycode::encode(input, resource));
    }
    static_cast<void>(
        idna::punycode::encode(U"b\u00FCcher-and-more-than-a-small-string", resource));
    BOOST_TEST_EQ(global_allocations - before, resource.allocations());
    BOOST_TEST(resource.allocations() > 0);
}

}  // namespace

int main() {
    span_forms_allocate_nothing();
    pmr_forms_allocate_through_their_resource();
    return boost::report_errors();
}
