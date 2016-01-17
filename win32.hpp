// win32.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#pragma warning (disable: 4355 ) // 'this' : used in base member initializer list
#pragma warning (disable: 4258 ) // definition from the for loop is ignored; the definition from the enclosing scope is used
#pragma warning (disable: 4503 ) // decorated name length exceeded, name was truncated
#pragma warning (disable: 4351 ) // new behavior: elements of array 'xxx' will be default initialized
#pragma warning (disable: 4512 ) // assignment operator could not be generated
#pragma warning (disable: 4521 ) // multiple copy constructors specified
#pragma warning (disable: 4996 ) // 'function': was declared deprecated
#pragma warning (disable: 4345 ) // behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized

#if _MSC_VER < 1900
#pragma execution_character_set("utf-8")
#define u8
#endif

#ifdef NDEBUG

#ifndef _SCL_SECURE_NO_WARNINGS
	#define _SCL_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _SECURE_SCL
	#undef _SECURE_SCL
#endif
#define _SECURE_SCL 0
#ifdef _HAS_ITERATOR_DEBUGGING
	#undef _HAS_ITERATOR_DEBUGGING
#endif
#define _HAS_ITERATOR_DEBUGGING 0

#endif

#include "targetver.hpp"

#ifndef WIN32_LEAN_AND_MEAN
	#undef WIN32_LEAN_AND_MEAN
#endif

#include <algorithm> // for min/max

using std::min;
using std::max;

#ifdef USING_BOOST
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp>
#endif