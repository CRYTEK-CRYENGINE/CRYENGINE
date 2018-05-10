// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MacSpecific.h
//  Version:     v1.00
//  Created:     Leander Beernaert based on the MacSpecifc files
//  Compilers:   Clang
//  Description: Apple specific declarations common amongst its products
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __APPLESPECIFIC_H__
#define __APPLESPECIFIC_H__

#if CRY_PLATFORM_APPLE

	#if defined(__clang__)
		#pragma diagnostic ignore "-W#pragma-messages"
	#endif

	#define RC_EXECUTABLE "rc"

//! Disable LiveCreate on Apple Operating Systems.
	#define NO_LIVECREATE

// Standard includes.
	#include <stdlib.h>
	#include <stdint.h>
	#include <math.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <time.h>
	#include <ctype.h>
	#include <limits.h>
	#include <signal.h>
	#include <unistd.h>
	#include <errno.h>
	#include <malloc/malloc.h>
	#include <Availability.h>

// Atomic operations, guaranteed to work across all apple platforms.
	#include <libkern/OSAtomic.h>

	#include <sys/types.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <stdio.h>
	#include <dirent.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <vector>
	#include <string>

	#define FP16_MESH
	#define BOOST_DISABLE_WIN32

typedef void* LPVOID;
	#define VOID  void
	#define PVOID void*

typedef unsigned int UINT;
typedef char         CHAR;
typedef float        FLOAT;

	#define PHYSICS_EXPORTS

// MSVC compiler-specific keywords
	#define __cdecl
	#define _cdecl
	#define __stdcall
	#define _stdcall
	#define __fastcall
	#define _fastcall
	#define IN
	#define OUT

	#define MAP_ANONYMOUS MAP_ANON

// Define platform-independent types.
	#include <CryCore/BaseTypes.h>

typedef double                        real;

typedef uint32                        DWORD;
typedef DWORD*                        LPDWORD;
typedef uint64                        DWORD_PTR;
typedef intptr_t INT_PTR, *           PINT_PTR;
typedef uintptr_t UINT_PTR, *         PUINT_PTR;
typedef char* LPSTR, *                PSTR;
typedef uint64                        __uint64;
	#if !defined(__clang__)
typedef int64                         __int64;
	#endif
typedef int64                         INT64;
typedef uint64                        UINT64;

typedef long LONG_PTR, * PLONG_PTR, * PLONG;
typedef unsigned long ULONG_PTR, *    PULONG_PTR;

typedef uint8                         BYTE;
typedef uint16                        WORD;
typedef void*                         HWND;
typedef UINT_PTR                      WPARAM;
typedef LONG_PTR                      LPARAM;
typedef LONG_PTR                      LRESULT;
	#define PLARGE_INTEGER LARGE_INTEGER *
typedef const char* LPCSTR, *         PCSTR;
typedef long long                     LONGLONG;
typedef ULONG_PTR                     SIZE_T;
typedef uint8                         byte;

	#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
	#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
	#define LOWORD(l)      ((WORD)((DWORD_PTR)(l) & 0xffff))
	#define HIWORD(l)      ((WORD)((DWORD_PTR)(l) >> 16))
	#define LOBYTE(w)      ((BYTE)((DWORD_PTR)(w) & 0xff))
	#define HIBYTE(w)      ((BYTE)((DWORD_PTR)(w) >> 8))

	#define CALLBACK
	#define WINAPI

	#ifndef __cplusplus
		#ifndef _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
			#define TCHAR wchar_t;
			#define _WCHAR_T_DEFINED
		#endif
	#endif
typedef wchar_t                  WCHAR; //!< wc,   16-bit UNICODE character
typedef WCHAR*                   PWCHAR;
typedef WCHAR* LPWCH, *          PWCH;
typedef const WCHAR* LPCWCH, *   PCWCH;
typedef WCHAR*                   NWPSTR;
typedef WCHAR* LPWSTR, *         PWSTR;
typedef WCHAR* LPUWSTR, *        PUWSTR;

typedef const WCHAR* LPCWSTR, *  PCWSTR;
typedef const WCHAR* LPCUWSTR, * PCUWSTR;

	#define MAKEFOURCC(ch0, ch1, ch2, ch3)              \
	  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
	   ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
	#define FILE_ATTRIBUTE_NORMAL 0x00000080

//! Conflict with Objective-C defined bool type.
typedef int          BOOL;
typedef int32_t      LONG;
typedef unsigned int ULONG;
typedef int          HRESULT;

// typedef int32 __int32;
typedef uint32 __uint32;
typedef int64  __int64;
typedef uint64 __uint64;

	#define TRUE  1
	#define FALSE 0

	#ifndef MAX_PATH
		#define MAX_PATH 256
	#endif
	#ifndef _MAX_PATH
		#define _MAX_PATH MAX_PATH
	#endif

	#define _PTRDIFF_T_DEFINED 1

	#define _A_RDONLY          (0x01) /* Read only file */
	#define _A_HIDDEN          (0x02) /* Hidden file */
	#define _A_SUBDIR          (0x10) /* Subdirectory */

// Win32 FileAttributes.
	#define FILE_ATTRIBUTE_READONLY            0x00000001
	#define FILE_ATTRIBUTE_HIDDEN              0x00000002
	#define FILE_ATTRIBUTE_SYSTEM              0x00000004
	#define FILE_ATTRIBUTE_DIRECTORY           0x00000010
	#define FILE_ATTRIBUTE_ARCHIVE             0x00000020
	#define FILE_ATTRIBUTE_DEVICE              0x00000040
	#define FILE_ATTRIBUTE_NORMAL              0x00000080
	#define FILE_ATTRIBUTE_TEMPORARY           0x00000100
	#define FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
	#define FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
	#define FILE_ATTRIBUTE_COMPRESSED          0x00000800
	#define FILE_ATTRIBUTE_OFFLINE             0x00001000
	#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
	#define FILE_ATTRIBUTE_ENCRYPTED           0x00004000

	#define INVALID_FILE_ATTRIBUTES            (-1)

// function renaming
	#define _finite   std::isfinite
//#define _isnan isnan
	#define stricmp   strcasecmp
	#define _stricmp  strcasecmp
	#define strnicmp  strncasecmp
	#define _strnicmp strncasecmp
	#define wcsicmp   wcscasecmp
	#define wcsnicmp  wcsncasecmp
//#define memcpy_s(dest,bytes,src,n) memcpy(dest,src,n)
	#define _isnan    ISNAN
	#define _wtof(str) wcstod(str, 0)

	#define TARGET_DEFAULT_ALIGN (0x8U)

	#define _msize               malloc_size

struct _OVERLAPPED;

typedef void (* LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, struct _OVERLAPPED* lpOverlapped);

typedef struct _OVERLAPPED
{
	//! This is orginally reserved for internal purpose, we store the Caller pointer here.
	void* pCaller;

	//! This is orginally ULONG_PTR InternalHigh and reserved for internal purpose.
	LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine;
	union
	{
		struct
		{
			DWORD Offset;
			DWORD OffsetHigh;
		};
		PVOID Pointer;
	};

	//! Additional member temporary speciying the number of bytes to be read.
	DWORD            dwNumberOfBytesTransfered;
	/*HANDLE*/ void* hEvent;
} OVERLAPPED, * LPOVERLAPPED;

typedef struct _SECURITY_ATTRIBUTES
{
	DWORD  nLength;
	LPVOID lpSecurityDescriptor;
	BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, * PSECURITY_ATTRIBUTES, * LPSECURITY_ATTRIBUTES;

	#ifdef __cplusplus

		#define __min(_S, _T) min(_S, _T)
		#define __max(_S, _T) max(_S, _T)

typedef enum {INVALID_HANDLE_VALUE = -1l} INVALID_HANDLE_VALUE_ENUM;
//! For compatibility reason we needed to create a class that actually contains an int rather than a void* and make sure it does not get mistreated.
//! U is default type for invalid handle value, T the encapsulated handle type to be used instead of void* (as under windows and never linux).
template<class T, T U>
class CHandle
{
public:
	typedef T     HandleType;
	typedef void* PointerType;    //!< for compatibility reason to encapsulate a void* as an int

	static const HandleType sciInvalidHandleValue = U;

	CHandle(const CHandle<T, U>& cHandle) : m_Value(cHandle.m_Value){}
	CHandle(const HandleType cHandle = U) : m_Value(cHandle){}
	CHandle(const PointerType cpHandle) : m_Value(reinterpret_cast<HandleType>(cpHandle)){}
	CHandle(INVALID_HANDLE_VALUE_ENUM) : m_Value(U){}      //!< to be able to use a common value for all InvalidHandle - types
		#if CRY_PLATFORM_64BIT
	//! Treat __null tyope also as invalid handle type.
	//! To be able to use a common value for all InvalidHandle - types.
	CHandle(long) : m_Value(U){}
		#endif
	operator HandleType(){ return m_Value; }
	bool           operator!() const                             { return m_Value == sciInvalidHandleValue; }
	const CHandle& operator=(const CHandle& crHandle)            { m_Value = crHandle.m_Value; return *this; }
	const CHandle& operator=(const PointerType cpHandle)         { m_Value = (HandleType) reinterpret_cast<UINT_PTR>(cpHandle); return *this; }
	const bool     operator==(const CHandle& crHandle)   const   { return m_Value == crHandle.m_Value; }
	const bool     operator==(const HandleType cHandle)  const   { return m_Value == cHandle; }
	const bool     operator==(const PointerType cpHandle)  const { return m_Value == reinterpret_cast<HandleType>(cpHandle); }
	const bool     operator!=(const HandleType cHandle)  const   { return m_Value != cHandle; }
	const bool     operator!=(const CHandle& crHandle)   const   { return m_Value != crHandle.m_Value; }
	const bool     operator!=(const PointerType cpHandle)  const { return m_Value != reinterpret_cast<HandleType>(cpHandle); }
	const bool     operator<(const CHandle& crHandle) const      { return m_Value < crHandle.m_Value; }
	HandleType     Handle() const                                { return m_Value; }

private:
	HandleType m_Value;               //!< The actual value, remember that file descriptors are ints under linux.

	typedef void ReferenceType;     //!< For compatibility reasons, to encapsulate a void* as an int.

	// Forbid these functions, which would actually not work on an int.
	PointerType   operator->();
	PointerType   operator->() const;
	ReferenceType operator*();
	ReferenceType operator*() const;
	operator PointerType();
};

typedef CHandle<int, (int) - 1l> HANDLE;

typedef HANDLE                   EVENT_HANDLE;
typedef HANDLE                   THREAD_HANDLE;

	#endif //__cplusplus

typedef union _LARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		LONG  HighPart;
	};
	struct
	{
		DWORD LowPart;
		LONG  HighPart;
	}         u;

	long long QuadPart;
} LARGE_INTEGER;

extern bool QueryPerformanceCounter(LARGE_INTEGER*);
extern bool QueryPerformanceFrequency(LARGE_INTEGER* frequency);

/*
   inline uint32 GetTickCount()
   {
   LARGE_INTEGER count, freq;
   QueryPerformanceCounter(&count);
   QueryPerformanceFrequency(&freq);
   return uint32(count.QuadPart * 1000 / freq.QuadPart);
   }
 */

typedef struct in_addr_windows
{
	union
	{
		struct { unsigned char s_b1, s_b2, s_b3, s_b4; } S_un_b;
		struct { unsigned short s_w1, s_w2; }            S_un_w;
		unsigned int S_addr;
	} S_un;
}in_addr_windows;

	#define WSAEINTR           EINTR
	#define WSAEBADF           EBADF
	#define WSAEACCES          EACCES
	#define WSAEFAULT          EFAULT
	#define WSAEACCES          EACCES
	#define WSAEFAULT          EFAULT
	#define WSAEINVAL          EINVAL
	#define WSAEMFILE          EMFILE
	#define WSAEWOULDBLOCK     EAGAIN
	#define WSAEINPROGRESS     EINPROGRESS
	#define WSAEALREADY        EALREADY
	#define WSAENOTSOCK        ENOTSOCK
	#define WSAEDESTADDRREQ    EDESTADDRREQ
	#define WSAEMSGSIZE        EMSGSIZE
	#define WSAEPROTOTYPE      EPROTOTYPE
	#define WSAENOPROTOOPT     ENOPROTOOPT
	#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
	#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
	#define WSAEOPNOTSUPP      EOPNOTSUPP
	#define WSAEPFNOSUPPORT    EPFNOSUPPORT
	#define WSAEAFNOSUPPORT    EAFNOSUPPORT
	#define WSAEADDRINUSE      EADDRINUSE
	#define WSAEADDRNOTAVAIL   EADDRNOTAVAIL
	#define WSAENETDOWN        ENETDOWN
	#define WSAENETUNREACH     ENETUNREACH
	#define WSAENETRESET       ENETRESET
	#define WSAECONNABORTED    ECONNABORTED
	#define WSAECONNRESET      ECONNRESET
	#define WSAENOBUFS         ENOBUFS
	#define WSAEISCONN         EISCONN
	#define WSAENOTCONN        ENOTCONN
	#define WSAESHUTDOWN       ESHUTDOWN
	#define WSAETOOMANYREFS    ETOOMANYREFS
	#define WSAETIMEDOUT       ETIMEDOUT
	#define WSAECONNREFUSED    ECONNREFUSED
	#define WSAELOOP           ELOOP
	#define WSAENAMETOOLONG    ENAMETOOLONG
	#define WSAEHOSTDOWN       EHOSTDOWN
	#define WSAEHOSTUNREACH    EHOSTUNREACH
	#define WSAENOTEMPTY       ENOTEMPTY
	#define WSAEPROCLIM        EPROCLIM
	#define WSAEUSERS          EUSERS
	#define WSAEDQUOT          EDQUOT
	#define WSAESTALE          ESTALE
	#define WSAEREMOTE         EREMOTE

	#define WSAHOST_NOT_FOUND  (1024 + 1)
	#define WSATRY_AGAIN       (1024 + 2)
	#define WSANO_RECOVERY     (1024 + 3)
	#define WSANO_DATA         (1024 + 4)
	#define WSANO_ADDRESS      (WSANO_DATA)

//-------------------------------------end socket stuff------------------------------------------

	#define __debugbreak() raise(SIGTRAP)

	#define __assume(x)
	#define __analysis_assume(x)
	#define _flushall sync

//! We take the definition of the pthread_t type directly from the pthread file.
	#define THREADID_NULL 0

#endif // CRY_PLATFORM_APPLE

#endif // __APPLESPECIFIC_H__
