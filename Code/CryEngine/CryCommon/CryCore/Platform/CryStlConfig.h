// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// We need CRY_COMPILER_XXX here, which are set via platform.h
// platform.h should be the first include by convention, so we treat it's absence as an error
#if !defined(CRY_PLATFORM)
	#error Include platform.h first
#endif

// CRY_ITERATOR_DEBUGGING can be set to 0/1 before including platform.h.
// This enables iterator debugging feature in the STL, if available.
// If it's not set yet, the feature is enabled if _DEBUG is defined.
// Be careful when using a non-default value, as third-party libraries using C++ STL need to match.
#if !defined(CRY_ITERATOR_DEBUGGING)
	#if defined(_DEBUG)
		#define CRY_ITERATOR_DEBUGGING 1
	#else
		#define CRY_ITERATOR_DEBUGGING 0
	#endif
#endif

// For now, GCC/Clang may use libstdc++ on Linux/Mac, which has a bug in std::array::swap (in debug).
// Therefore, disable STL debugging feature on this compiler.
#if (CRY_COMPILER_CLANG || CRY_COMPILER_GCC) && !CRY_PLATFORM_ORBIS
	#undef CRY_ITERATOR_DEBUGGING
	#define CRY_ITERATOR_DEBUGGING 0
#endif

// For Dinkumware style STL (as shipped with MSVC and Orbis SDK), we set:
// _HAS_ITERATOR_DEBUGGING
// _ITERATOR_DEBUG_LEVEL
// _HAS_EXCEPTIONS
// For other STL (libstdc++), we set:
// _GLIBCXX_DEBUG
#if CRY_COMPILER_MSVC || CRY_PLATFORM_ORBIS
	#if CRY_ITERATOR_DEBUGGING
		#define _HAS_ITERATOR_DEBUGGING 1
		#define _ITERATOR_DEBUG_LEVEL   2
	#else
		#define _HAS_ITERATOR_DEBUGGING 0
		#define _ITERATOR_DEBUG_LEVEL   0
	#endif
	#if CRY_COMPILER_EXCEPTIONS
		#define _HAS_EXCEPTIONS 1
	#else
		#define _HAS_EXCEPTIONS 0
	#endif
#else
	#if CRY_ITERATOR_DEBUGGING
		#define _GLIBCXX_DEBUG
	#else
		#undef _GLIBCXX_DEBUG
	#endif
#endif

// Disable CRT/STL deprecation and warnings for standard C++ functions.
// You can define CRY_SAFE_RUNTIME before including platform.h if you DO want the secure warnings behavior.
#if CRY_COMPILER_MSVC
	#if !defined(CRY_SAFE_RUNTIME)
		#define _CRT_SECURE_NO_DEPRECATE
		#define _CRT_SECURE_NO_WARNINGS
		#define _CRT_NONSTDC_NO_DEPRECATE
		#define _CRT_NONSTDC_NO_WARNINGS
		#define _SCL_SECURE_NO_WARNINGS
	#endif

// This enables a safer version of rand, which we should be using by default
	#define _CRT_RAND_S
#endif

// Detect mismatch of iterator debugging feature at link-time.
// If such a linker error is seen, it means that you are using incompatible STL ABI layouts within the same binary.
// This is a violation of the ODR rule, and may lead to problems for non-inlined function COMDATs.
#if CRY_COMPILER_MSVC
	#if CRY_ITERATOR_DEBUGGING
		#pragma detect_mismatch("CRY_ITERATOR_DEBUGGING", "enabled")
	#else
		#pragma detect_mismatch("CRY_ITERATOR_DEBUGGING", "disabled")
	#endif
#endif
