// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef __clang__
	#error This file should only be included on Clang compiler
#endif

//! Compiler version
#define CRY_COMPILER_CLANG   1
#define CRY_COMPILER_VERSION ((__clang_major__ * 100) + (__clang_minor__))
#if CRY_COMPILER_VERSION < 301
	#error This version of clang is not supported, the minimum supported version is 3.1
#endif
#if defined(__cplusplus) && __cplusplus < 201103L
	#error The compiler is not in C++11 mode, this is required for compiling CRYENGINE
#endif

//! Compiler features
#if __has_feature(cxx_exceptions)
	#define CRY_COMPILER_EXCEPTIONS 1
#endif
#if __has_feature(cxx_rtti)
	#define CRY_COMPILER_RTTI 1
#endif

//! __FUNC__ is like __func__, but it has the class name
#define __FUNC__               __PRETTY_FUNCTION__
#define CRY_FUNC_HAS_SIGNATURE 1

//! PREfast not supported
#define PREFAST_SUPPRESS_WARNING(W)
#define PREFAST_ASSUME(cond)
#define _Out_writes_z_(x)
#define _Inout_updates_z_(x)

#if __cplusplus >= 201402L
#define CRY_DEPRECATED(message) [[deprecated(message)]]
#else
#define CRY_DEPRECATED(message) __attribute__((deprecated(message)))
#endif

//! Portable alignment helper, can be placed after the struct/class/union keyword, or before the type of a declaration.
//! Example: struct CRY_ALIGN(16) { ... }; CRY_ALIGN(16) char myAlignedChar;
#define CRY_ALIGN(bytes) __attribute__((aligned(bytes)))

//! Restricted reference (similar to restricted pointer), use like: SFoo& RESTRICT_REFERENCE myFoo = ...;
#define RESTRICT_REFERENCE __restrict__

//! Compiler-supported type-checking helper
#define PRINTF_PARAMS(...) __attribute__((format(printf, __VA_ARGS__)))
#define SCANF_PARAMS(...)  __attribute__((format(scanf, __VA_ARGS__)))

//! Barrier to prevent R/W reordering by the compiler.
//! Note: This does not emit any instruction, and it does not prevent CPU reordering!
#define MEMORY_RW_REORDERING_BARRIER asm volatile ("" ::: "memory")

//! Static branch-prediction helpers
#define IF(condition, hint)    if (__builtin_expect(!!(condition), hint))
#define IF_UNLIKELY(condition) if (__builtin_expect(!!(condition), 0))
#define IF_LIKELY(condition)   if (__builtin_expect(!!(condition), 1))

//! Inline helpers
#define NO_INLINE        __attribute__ ((noinline))
#define NO_INLINE_WEAK   __attribute__ ((noinline)) __attribute__((weak))
#define CRY_FORCE_INLINE __attribute__((always_inline)) inline

//! Packing helper, the preceding declaration will be tightly packed.
#define __PACKED __attribute__ ((packed))

// Suppress undefined behavior sanitizer errors on a function.
#if defined(CRY_UBSAN) && __has_attribute(no_sanitize)
	#define CRY_FUNCTION_CONTAINS_UNDEFINED_BEHAVIOR __attribute__((no_sanitize("undefined")))
#elif defined(CRY_UBSAN) && __has_attribute(no_sanitize_undefined)
	#define CRY_FUNCTION_CONTAINS_UNDEFINED_BEHAVIOR __attribute__((no_sanitize_undefined))
#else
	#define CRY_FUNCTION_CONTAINS_UNDEFINED_BEHAVIOR
#endif

//! Unreachable code marker for helping error handling and optimization
#define UNREACHABLE() __builtin_unreachable()

#ifdef  _MSC_VER
// For clang on MSBuild
#define stricmp   _stricmp
#define strnicmp  _strnicmp
#define wcsicmp   _wcsicmp
#define wcsnicmp  _wcsnicmp
#define alloca    _alloca
#define itoa      _itoa
#endif //CRY_PLATFORM_ORBIS
