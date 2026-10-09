// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The configuration of idna, which each of its headers includes first: whether
 idna is built without exceptions.

 @see "Testing", in the guide.
*/
#ifndef WEBCPP_IDNA_CONFIG_HPP
#define WEBCPP_IDNA_CONFIG_HPP

// webcpp::config does not exist yet: when it does, this line becomes
// #include <webcpp/config.hpp>, and build.jam adds /webcpp/config//config.
#include <boost/config.hpp>

// MrDocs parses idna as a build with exceptions does, where the macro is not
// defined: this definition, undone at once, is the one the reference lists.
#ifdef __MRDOCS__
/**
 Defined when idna is built without exceptions: automatically when the
 compiler has none (`BOOST_NO_EXCEPTIONS`), or by the developer to disable them
 in a build that has them.

 idna raises no exception of its own: every operation that can fail returns
 its error, so the macro changes nothing in it. It is provided so that a
 program can set it for every webcpp library alike. An exception idna raised
 would go through `boost::throw_exception`, and an API that throws would be
 absent while this is defined.

 @see "Testing", in the guide.
*/
#define WEBCPP_IDNA_NO_EXCEPTIONS
#undef WEBCPP_IDNA_NO_EXCEPTIONS
#endif

#if defined(BOOST_NO_EXCEPTIONS) && !defined(WEBCPP_IDNA_NO_EXCEPTIONS)
#define WEBCPP_IDNA_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_IDNA_CONFIG_HPP
