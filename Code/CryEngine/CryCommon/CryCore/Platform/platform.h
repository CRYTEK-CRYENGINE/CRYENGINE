// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryPlatformDefines.h"
#include "CryStlConfig.h"
#include <CrySystem/CryUtils.h>

#ifdef _WINDOWS_
	#error windows.h should not be included prior to platform.h
#endif

enum class EPlatform
{
	Windows,
	Linux,
	MacOS,
	XboxOne,
	PS4,
	Android,
	iOS,

#ifdef CRY_PLATFORM_WINDOWS
	Current = Windows
#elif defined(CRY_PLATFORM_LINUX)
	Current = Linux
#elif defined(CRY_PLATFORM_APPLE) && !defined(CRY_PLATFORM_MOBILE)
	Current = MacOS
#elif defined(CRY_PLATFORM_DURANGO)
	Current = XboxOne
#elif defined(CRY_PLATFORM_ORBIS)
	Current = PS4
#elif defined(CRY_PLATFORM_ANDROID)
	Current = Android
#elif defined(CRY_PLATFORM_APPLE) && defined(CRY_PLATFORM_MOBILE)
	Current = iOS
#endif
};

// Alignment|InitializerList support.
#if CRY_COMPILER_MSVC && (_MSC_VER >= 1800)
	#define _ALLOW_INITIALIZER_LISTS
#elif CRY_COMPILER_GCC || CRY_COMPILER_CLANG
	#define _ALLOW_INITIALIZER_LISTS
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC
	#define _FILE_OFFSET_BITS 64 // define large file support > 2GB
#endif

#if defined(NOT_USE_CRY_MEMORY_MANAGER)
	#include <cstring>
#elif !defined(eCryModule)
	#if defined(_LIB)
// For static libraries, the module the library is linked into needs to set eCryModule
// However, the linker will set detect_mismatch if the final module doesn't use memory manager
	#else
		#error Code configuration error: eCryModule needs to be set for all modules using the CRYENGINE memory manager
	#endif
#else
	#if CRY_COMPILER_MSVC && !defined(CRY_IS_MONOLITHIC_BUILD)
		#define QUOTED_MODULE_VALUE_STR(x) # x
		#define QUOTED_MODULE_VALUE(x)     QUOTED_MODULE_VALUE_STR(x)
		#pragma detect_mismatch("CRY_MODULE", QUOTED_MODULE_VALUE(eCryModule))
		#undef QUOTED_MODULE_VALUE_STR
		#undef QUOTED_MODULE_VALUE
	#endif
#endif

#if defined(_DEBUG) && CRY_PLATFORM_WINAPI
	#include <crtdbg.h>
#endif

#define RESTRICT_POINTER __restrict

// Safe memory helpers
#define SAFE_ACQUIRE(p)       { if (p) (p)->AddRef(); }
#define SAFE_DELETE(p)        { if (p) { delete (p);          (p) = NULL; } }
#define SAFE_DELETE_ARRAY(p)  { if (p) { delete[] (p);        (p) = NULL; } }
#define SAFE_RELEASE(p)       { if (p) { (p)->Release();      (p) = NULL; } }
#define SAFE_RELEASE_FORCE(p) { if (p) { (p)->ReleaseForce(); (p) = NULL; } }

#ifndef CHECK_REFERENCE_COUNTS     //define that in your StdAfx.h to override per-project
	#define CHECK_REFERENCE_COUNTS 0 //default value
#endif

#if CHECK_REFERENCE_COUNTS
	#define CHECK_REFCOUNT_CRASH(x) { if (!(x)) *((int*)0) = 0; }
#else
	#define CHECK_REFCOUNT_CRASH(x)
#endif

#ifndef GARBAGE_MEMORY_ON_FREE     //define that in your StdAfx.h to override per-project
	#define GARBAGE_MEMORY_ON_FREE 0 //default value
#endif

#if GARBAGE_MEMORY_ON_FREE
	#ifndef GARBAGE_MEMORY_RANDOM     //define that in your StdAfx.h to override per-project
		#define GARBAGE_MEMORY_RANDOM 1 //0 to change it to progressive pattern
	#endif
#endif

// Translate some predefined macros.

// NDEBUG disables std asserts, etc.
// Define it automatically if not compiling with Debug libs, or with ADEBUG flag.
#if !defined(_DEBUG) && !defined(ADEBUG) && !defined(NDEBUG)
	#define NDEBUG
#endif

#if CRY_PLATFORM_DURANGO && defined(_RELEASE) && !defined(_LIB) && !defined(CRY_IS_SCALEFORM_HELPER)
// Build static library when compiling release on Durango
	#error _LIB is expected to be set for release Durango
#endif

#if CRY_PLATFORM_ORBIS && !defined(_LIB) && !defined(CRY_IS_SCALEFORM_HELPER)
	#error _LIB is expected to be set for Orbis
#endif

//render thread settings, as this is accessed inside 3dengine and renderer and needs to be compile time defined, we need to do it here
//enable this macro to strip out the overhead for render thread
//	#define STRIP_RENDER_THREAD
#ifdef STRIP_RENDER_THREAD
	#define RT_COMMAND_BUF_COUNT 1
#else
//can be enhanced to triple buffering, FlushFrame needs to be adjusted and RenderObj would become 132 bytes
	#define RT_COMMAND_BUF_COUNT 2
#endif

#if CRY_PLATFORM_WINAPI
	#include <inttypes.h>
	#define PLATFORM_I64(x) x ## i64
#else
	#define __STDC_FORMAT_MACROS
	#include <inttypes.h>
	#if CRY_PLATFORM_APPLE || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_ORBIS
		#undef PRIX64
		#undef PRIx64
		#undef PRId64
		#undef PRIu64
		#undef PRIi64

		#define PRIX64 "llX"
		#define PRIx64 "llx"
		#define PRId64 "lld"
		#define PRIu64 "llu"
		#define PRIi64 "lli"
	#endif
	#define PLATFORM_I64(x) x ## ll
#endif

#if !defined(PRISIZE_T)
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT || CRY_PLATFORM_DURANGO
		#define PRISIZE_T "I64u"     //size_t defined as unsigned __int64
	#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT) || CRY_PLATFORM_ANDROID
		#define PRISIZE_T "u"
	#elif CRY_PLATFORM_APPLE || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_ORBIS
		#define PRISIZE_T "lu"
	#else
		#error "Please define PRISIZE_T for this platform"
	#endif
#endif

#if !defined(PRI_PTRDIFF_T)
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#define PRI_PTRDIFF_T "I64d"
	#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT) || CRY_PLATFORM_ANDROID
		#define PRI_PTRDIFF_T "d"
	#elif CRY_PLATFORM_APPLE || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO
		#define PRI_PTRDIFF_T "ld"
	#else
		#error "Please defined PRI_PTRDIFF_T for this platform"
	#endif
#endif

#if !defined(PRI_THREADID)
	#if (CRY_PLATFORM_APPLE && defined(__LP64__)) || CRY_PLATFORM_ORBIS
		#define PRI_THREADID "llu"
	#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) || CRY_PLATFORM_ANDROID || CRY_PLATFORM_DURANGO
		#define PRI_THREADID "lu"
	#elif (CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT) || (CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT)
		#define PRI_THREADID "u"
	#else
		#error "Please defined PRI_THREADID for this platform"
	#endif
#endif

#include <CryCore/Project/ProjectDefines.h>  // to get some defines available in every CryEngine project
#include <CryGame/ExtensionDefines.h>

// Include standard CRT headers used almost everywhere.
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

#if CRY_PLATFORM_IOS || (CRY_PLATFORM_ANDROID && defined(__clang__))
	#define USE_PTHREAD_TLS
#endif

// Storage class modifier for thread local storage.
#if defined(USE_PTHREAD_TLS)
// Does not support thread storage
	#define THREADLOCAL
#elif defined(__GNUC__) || CRY_PLATFORM_MAC
	#define THREADLOCAL __thread
#else
	#define THREADLOCAL __declspec(thread)
#endif

// define Read Write Barrier macro needed for lockless programming
#if defined(__arm__)
/**
 * (ARMv7) Full memory barriar.
 *
 * None of GCC 4.6/4.8 or clang 3.3/3.4 have a builtin intrinsic for ARM's ldrex/strex or dmb
 * instructions.  This is a placeholder until supplied by the toolchain.
 */
static inline void __dmb()
{
	// The linux kernel uses "dmb ish" to only sync with local monitor (arch/arm/include/asm/barrier.h):
	//#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")
	//#define smp_mb()        dmb(ish)
	__asm__ __volatile__ ("dmb ish" : : : "memory");
}

	#define READ_WRITE_BARRIER { __dmb(); }
#else
	#define READ_WRITE_BARRIER
#endif
//////////////////////////////////////////////////////////////////////////

//! default stack size for threads, currently only used on pthread platforms
#if CRY_PLATFORM_ORBIS
	#define SIMPLE_THREAD_STACK_SIZE_KB (256)
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#if !defined(_DEBUG)
		#define SIMPLE_THREAD_STACK_SIZE_KB (256)
	#else
		#define SIMPLE_THREAD_STACK_SIZE_KB (256 * 4)
	#endif
#else
	#define SIMPLE_THREAD_STACK_SIZE_KB (32)
#endif

#if defined(__GNUC__)
	#define DLL_EXPORT __attribute__ ((visibility("default")))
	#define DLL_IMPORT __attribute__ ((visibility("default")))
#else
	#define DLL_EXPORT __declspec(dllexport)
	#define DLL_IMPORT __declspec(dllimport)
#endif

// Define BIT macro for use in enums and bit masks.
#if !defined(SWIG)
	#define BIT(x)    (1u << (x))
	#define BIT64(x)  (1ull << (x))
	#define MASK(x)   (BIT(x) - 1U)
	#define MASK64(x) (BIT64(x) - 1ULL)
#else
	#define BIT(x)    (1 << (x))
	#define BIT64(x)  (1 << (x))
	#define MASK(x)   (BIT(x) - 1)
	#define MASK64(x) (BIT64(x) - 1)
#endif

//! ILINE always maps to CRY_FORCE_INLINE, which is the strongest possible inline preference.
//! Note: Only use when shown that the end-result is faster when ILINE macro is used instead of inline.
#if !defined(_DEBUG) && !defined(CRY_UBSAN)
	#define ILINE CRY_FORCE_INLINE
#else
	#define ILINE inline
#endif

// Help message, all help text in code must be wrapped in this define.
// Only include these in non RELEASE builds
#if !defined(_RELEASE)
	#define _HELP(x) x
#else
	#define _HELP(x) ""
#endif

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
	#include <CryCore/Platform/Win32specific.h>
#endif

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#include <CryCore/Platform/Win64specific.h>
#endif

#if CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT
	#include <CryCore/Platform/Linux64Specific.h>
#endif

#if CRY_PLATFORM_LINUX && CRY_PLATFORM_32BIT
	#include <CryCore/Platform/Linux32Specific.h>
#endif

#if CRY_PLATFORM_ANDROID && CRY_PLATFORM_64BIT
	#include <CryCore/Platform/Android64Specific.h>
#endif

#if CRY_PLATFORM_ANDROID && CRY_PLATFORM_32BIT
	#include <CryCore/Platform/Android32Specific.h>
#endif

#if CRY_PLATFORM_DURANGO
	#include <CryCore/Platform/DurangoSpecific.h>
#endif

#if CRY_PLATFORM_ORBIS
	#include <CryCore/Platform/OrbisSpecific.h>
#endif

#if CRY_PLATFORM_MAC
	#include <CryCore/Platform/MacSpecific.h>
#endif

#if CRY_PLATFORM_IOS
	#include "CryCore/Platform/iOSSpecific.h"
#endif

#if !defined(TARGET_DEFAULT_ALIGN)
	#error "No default alignment specified for target architecture"
#endif

#include "CryPlatform.h"

// Indicates potentially dangerous cast on 64bit machines
typedef UINT_PTR TRUNCATE_PTR;
typedef UINT_PTR EXPAND_PTR;

#include <stdio.h>

// Includes core CryEngine modules definitions.
#include <CryCore/Project/CryModuleDefs.h>

// Provide special cast function which mirrors C++ style casts to support aliasing correct type punning casts in gcc with strict-aliasing enabled
template<typename DestinationType, typename SourceType> inline DestinationType alias_cast(SourceType pPtr)
{
	union
	{
		SourceType      pSrc;
		DestinationType pDst;
	} conv_union;
	conv_union.pSrc = pPtr;
	return conv_union.pDst;

}

// CryModule memory manager routines must always be included.
// They are used by any module which doesn't define NOT_USE_CRY_MEMORY_MANAGER
// No Any STL includes must be before this line.
#if 1  //#ifndef NOT_USE_CRY_MEMORY_MANAGER
	#define USE_NEWPOOL
	#include <CryMemory/CryMemoryManager.h>
#else
inline int IsHeapValid()
{
	#if defined(_DEBUG) && !defined(RELEASE_RUNTIME)
	return _CrtCheckMemory();
	#else
	return true;
	#endif
}
#endif // NOT_USE_CRY_MEMORY_MANAGER

// Memory manager breaks strdup
// Use something higher level, like CryString
#undef strdup
#define strdup dont_use_strdup

#undef STATIC_CHECK
#define STATIC_CHECK(expr, msg) static_assert((expr) != 0, # msg)

// Conditionally execute code in debug only
#ifdef _DEBUG
	#define IF_DEBUG(expr) (expr)
#else
	#define IF_DEBUG(expr)
#endif

// Assert dialog box macros
#include <CryCore/Assert/CryAssert.h>

// Platform dependent functions that emulate Win32 API. Mostly used only for debugging!

enum EQuestionResult
{
	eQR_None,
	eQR_Cancel,
	eQR_Yes,
	eQR_No,
	eQR_Abort,
	eQR_Retry,
	eQR_Ignore
};

enum EMessageBox
{
	eMB_Info,
	eMB_YesCancel,
	eMB_YesNoCancel,
	eMB_Error,
	eMB_AbortRetryIgnore
};

//! Loads CrySystem from disk and initializes the engine, commonly called from the Launcher implementation
//! \param bManualEngineLoop Whether or not the caller will start and maintain the engine loop themselves. Otherwise the loop is started and engine shut down automatically inside the function.
bool			CryInitializeEngine(struct SSystemInitParams& startupParams, bool bManualEngineLoop = false);
void            CryDebugBreak();
void            CrySleep(unsigned int dwMilliseconds);
void            CryLowLatencySleep(unsigned int dwMilliseconds);
EQuestionResult CryMessageBox(const char* lpText, const char* lpCaption, EMessageBox uType = eMB_Info);
EQuestionResult CryMessageBox(const wchar_t* lpText, const wchar_t* lpCaption, EMessageBox uType = eMB_Info);
bool            CryCreateDirectory(const char* lpPathName);
bool            CryDirectoryExists(const char* szPath);
void            CryGetCurrentDirectory(unsigned int pathSize, char* szOutPath);
short           CryGetAsyncKeyState(int vKey);
unsigned int    CryGetFileAttributes(const char* lpFileName);
int             CryGetWritableDirectory(unsigned int pathSize, char* szOutPath);

void            CryGetExecutableFolder(unsigned int pathSize, char* szOutPath);
void            CryFindEngineRootFolder(unsigned int pathSize, char* szOutPath);
void            CrySetCurrentWorkingDirectory(const char* szWorkingDirectory);
void            CryFindRootFolderAndSetAsCurrentWorkingDirectory();

inline void     CryHeapCheck()
{
#if CRY_PLATFORM_WINDOWS // todo: this might be readded with later xdks?
	int Result = _heapchk();
	assert(Result != _HEAPBADBEGIN);
	assert(Result != _HEAPBADNODE);
	assert(Result != _HEAPBADPTR);
	assert(Result != _HEAPEMPTY);
	assert(Result == _HEAPOK);
#endif
}

//! Useful function to clean a structure.
template<class T>
inline void ZeroStruct(T& t)
{
	memset(&t, 0, sizeof(t));
}

//! Useful function to clean a C-array.
template<class T>
inline void ZeroArray(T& t)
{
	PREFAST_SUPPRESS_WARNING(6260)
	memset(&t, 0, sizeof(t[0]) * CRY_ARRAY_COUNT(t));
}

//! Useful functions to init and destroy objects.
template<class T>
inline void Construct(T& t)
{
	new(&t)T();
}

template<class T, class U>
inline void Construct(T& t, U const& u)
{
	new(&t)T(u);
}

template<class T>
inline void Destruct(T& t)
{
	t.~T();
}

//! Cast one type to another, asserting there is no conversion loss.
//! Usage: DestType dest = check_cast<DestType>(src);
template<class D, class S>
inline D check_cast(S const& s)
{
	D d = D(s);
	assert(S(d) == s);
	return d;
}

//! Convert one type to another, asserting there is no conversion loss.
//! Usage: DestType dest;  check_convert(dest, src);
template<class D, class S>
inline D& check_convert(D& d, S const& s)
{
	d = D(s);
	assert(S(d) == s);
	return d;
}

//! Convert one type to another, asserting there is no conversion loss.
//! Usage: DestType dest;  check_convert(dest) = src;
template<class D>
struct CheckConvert
{
	CheckConvert(D& d)
		: dest(&d)
	{}

	template<class S>
	D& operator=(S const& s)
	{
		return check_convert(*dest, s);
	}

protected:
	D* dest;
};

template<class D>
inline CheckConvert<D> check_convert(D& d)
{
	return d;
}

//! Use NoCopy as a base class to easily prevent copy ctor & operator for any class.
struct NoCopy
{
	NoCopy() = default;
	NoCopy(const NoCopy&) = delete;
	NoCopy& operator=(const NoCopy&) = delete;
	NoCopy(NoCopy&&) = default;
	NoCopy& operator=(NoCopy&&) = default;
};

//! Use NoMove as a base class to easily prevent move ctor & operator for any class.
struct NoMove
{
	NoMove() = default;
	NoMove(const NoMove&) = default;
	NoMove& operator=(const NoMove&) = default;
	NoMove(NoMove&&) = delete;
	NoMove& operator=(NoMove&&) = delete;
};

//! ZeroInit: base class to zero the memory of the derived class before initialization, so local objects initialize the same as static.
//! Usage:
//!     class MyClass: ZeroInit<MyClass> {...}
//!     class MyChild: public MyClass, ZeroInit<MyChild> {...}		// ZeroInit must be the last base class.
template<class TDerived>
struct ZeroInit
{
#if defined(__clang__) || defined(__GNUC__)
	bool __dummy;             //!< Dummy var to create non-zero size, ensuring proper placement in TDerived.
#endif

	ZeroInit(bool bZero = true)
	{
		// Optional bool arg to selectively disable zeroing.
		if (bZero)
		{
			// Infer offset of this base class by static casting to derived class.
			// Zero only the additional memory of the derived class.
			TDerived* struct_end = static_cast<TDerived*>(this) + 1;
			size_t memory_size = (char*)struct_end - (char*)this;
			memset(this, 0, memory_size);
		}
	}
};

// Quick const-manipulation macros

//! Declare a const and variable version of a function simultaneously.
#define CONST_VAR_FUNCTION(head, body) \
  ILINE head body                      \
  ILINE const head const body          \

template<class T>
ILINE T& non_const(const T& t)
{
	return const_cast<T&>(t);
}

template<class T>
ILINE T* non_const(const T* t)
{
	return const_cast<T*>(t);
}

// Member operator generators

//! Define simple operator, automatically generate compound.
//! Example: COMPOUND_MEMBER_OP(TThis, +, TOther) { return add(a, b); }
#define COMPOUND_MEMBER_OP(T, op, B)                                     \
  ILINE T& operator op ## = (const B& b) { return *this = *this op b; }  \
  ILINE T operator op(const B& b) const                                  \

//! Define compound operator, automatically generate simple.
//! Example: COMPOUND_STRUCT_MEMBER_OP(TThis, +, TOther) { return a = add(a, b); }
#define COMPOUND_MEMBER_OP_EQ(T, op, B)                                      \
  ILINE T operator op(const B& b) const { T t = *this; return t op ## = b; } \
  ILINE T& operator op ## = (const B& b)                                     \

#define using_type(super, type) \
  typedef typename super::type type;

typedef unsigned char          uchar;
typedef unsigned int           uint;
typedef const char*            cstr;

//! Align function works on integer or pointer values. Only supports power-of-two alignment.
template<typename T>
ILINE T Align(T nData, size_t nAlign)
{
	assert((nAlign & (nAlign - 1)) == 0);
	size_t size = ((size_t)nData + (nAlign - 1)) & ~(nAlign - 1);
	return T(size);
}

template<typename T>
ILINE bool IsAligned(T nData, size_t nAlign)
{
	assert((nAlign & (nAlign - 1)) == 0);
	return (size_t(nData) & (nAlign - 1)) == 0;
}

template<typename T, typename U>
ILINE void SetFlags(T& dest, U flags, bool b)
{
	if (b)
		dest |= flags;
	else
		dest &= ~flags;
}

// Wrapper code for non-windows builds.
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_MAC || CRY_PLATFORM_IOS || CRY_PLATFORM_ANDROID
	#include <CryCore/Platform/Linux_Win32Wrapper.h>
#elif CRY_PLATFORM_ORBIS
	#include <CryCore/Platform/Orbis_Win32Wrapper.h>
#endif

// Platform wrappers must be included before CryString.h
#include <CryString/CryString.h>

// Include support for meta-type data.
#include <CryCore/TypeInfo_decl.h>

bool     CrySetFileAttributes(const char* lpFileName, uint32 dwFileAttributes);
threadID CryGetCurrentThreadId();
int64    CryGetTicks();

#if !defined(NOT_USE_CRY_STRING)
// Fixed-Sized (stack based string)
// put after the platform wrappers because of missing wcsicmp/wcsnicmp functions
	#include <CryString/CryFixedString.h>
#endif

//! Need this in a common header file and any other file would be too misleading.
enum ETriState
{
	eTS_false,
	eTS_true,
	eTS_maybe
};

#if CRY_PLATFORM_WINDOWS
extern "C" {
	__declspec(dllimport) unsigned long __stdcall TlsAlloc();
	__declspec(dllimport) void* __stdcall         TlsGetValue(unsigned long dwTlsIndex);
	__declspec(dllimport) int __stdcall           TlsSetValue(unsigned long dwTlsIndex, void* lpTlsValue);
}

	#define TLS_DECLARE(type, var) extern int var ## idx;
	#define TLS_DEFINE(type, var)                  \
	  int var ## idx;                              \
	  struct Init ## var {                         \
	    Init ## var() { var ## idx = TlsAlloc(); } \
	  };                                           \
	  Init ## var g_init ## var;
	#define TLS_DEFINE_DEFAULT_VALUE(type, var, value)                                      \
	  int var ## idx;                                                                       \
	  struct Init ## var {                                                                  \
	    Init ## var() { var ## idx = TlsAlloc(); TlsSetValue(var ## idx, (void*)(value)); } \
	  };                                                                                    \
	  Init ## var g_init ## var;
	#define TLS_GET(type, var)                              (type)TlsGetValue(var ## idx)
	#define TLS_SET(var, val)                               TlsSetValue(var ## idx, (void*)(val))
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_MAC
	#define TLS_DECLARE(type, var)                          extern THREADLOCAL type var;
	#define TLS_DEFINE(type, var)                           THREADLOCAL type var = 0;
	#define TLS_DEFINE_DEFAULT_VALUE(type, var, value)      THREADLOCAL type var = value;
	#define TLS_GET(type, var)                              (var)
	#define TLS_SET(var, val)                               (var = (val))
#elif defined(USE_PTHREAD_TLS)
	#define TLS_DECLARE(_TYPE, _VAR)                        extern SCryPthreadTLS<_TYPE> _VAR ## TLSKey;
	#define TLS_DEFINE(_TYPE, _VAR)                         SCryPthreadTLS<_TYPE> _VAR ## TLSKey;
	#define TLS_DEFINE_DEFAULT_VALUE(_TYPE, _VAR, _DEFAULT) SCryPthreadTLS<_TYPE> _VAR ## TLSKey = _DEFAULT;
	#define TLS_GET(_TYPE, _VAR)                            _VAR ## TLSKey.Get()
	#define TLS_SET(_VAR, _VALUE)                           _VAR ## TLSKey.Set(_VALUE)
#else
	#define TLS_DECLARE(type, var)                          extern THREADLOCAL type var;
	#define TLS_DEFINE(type, var)                           THREADLOCAL type var;
	#define TLS_DEFINE_DEFAULT_VALUE(type, var, value)      THREADLOCAL type var = value;
	#define TLS_GET(type, var)                              (var)
	#define TLS_SET(var, val)                               (var = (val))
#endif

// Include MultiThreading support.
#include <CryThreading/CryThread.h>

// Include most commonly used STL headers.
// They end up in precompiled header and make compilation faster.
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <deque>
#include <stack>
#include <algorithm>
#include <functional>
#include <iterator>
#include <CryMemory/STLAlignedAlloc.h>

// In RELEASE disable printf and fprintf
#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP && !defined(RELEASE_LOGGING)
	#undef printf
	#define printf(...)  (void) 0
	#undef fprintf
	#define fprintf(...) (void) 0
#endif

#define _STRINGIFY(x) # x
#define STRINGIFY(x)  _STRINGIFY(x)

#if CRY_PLATFORM_WINAPI
	#define MESSAGE(msg) message(__FILE__ "(" STRINGIFY(__LINE__) "): " msg)
#else
	#define MESSAGE(msg)
#endif

#ifdef _MSC_VER
	#define CRY_PP_ERROR(msg) __pragma(message( __FILE__ "(" STRINGIFY(__LINE__) ")" ": error: " msg))
#else
	#define CRY_PP_ERROR(msg)
#endif

#define DECLARE_SHARED_POINTERS(name)                   \
  typedef std::shared_ptr<name> name ##       Ptr;      \
  typedef std::shared_ptr<const name> name ## ConstPtr; \
  typedef std::weak_ptr<name> name ##         WeakPtr;  \
  typedef std::weak_ptr<const name> name ##   ConstWeakPtr;

// Include array.
#include <CryCore/Containers/CryArray.h>

// Include static auto registration function
#include <CryCore/StaticInstanceList.h>

#ifdef _WINDOWS_
	#error windows.h should not be included through any headers within platform.h
#endif
