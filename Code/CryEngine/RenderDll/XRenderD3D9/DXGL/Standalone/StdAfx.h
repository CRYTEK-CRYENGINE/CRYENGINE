// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StdAfx.h
//  Version:     v1.00
//  Created:     31/07/2014 by Valerio Guagliumi.
//  Description: Utility header for standalone DXGL build
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
	#define ILINE __forceinline
#else
	#define ILINE inline
#endif

#define COMPILE_TIME_ASSERT(_Condition)

#if !defined(SAFE_RELEASE)
	#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }
#endif //!defined(SAFE_RELEASE)

#include <cstring>
typedef unsigned short   ushort;
typedef unsigned char    uchar;
typedef uchar            uint8;
typedef char             int8;
#if CRY_PLATFORM_WINDOWS
typedef unsigned __int16 uint16;
typedef __int16          int16;
typedef unsigned __int32 uint32;
typedef __int32          int32;
typedef unsigned __int64 uint64;
typedef __int64          int64;
#else
	#include <stdint.h>
typedef uint16_t    uint16;
typedef int16_t     int16;
typedef uint32_t    uint32;
typedef int32_t     int32;
typedef uint64_t    uint64;
typedef int64_t     int64;
typedef uint32      ULONG;
typedef uint32      DWORD;
typedef void*       HANDLE;
typedef void*       HWND;
typedef void*       HMODULE;
typedef uchar       BYTE;
typedef uint64      UINT64;
typedef int32       LONG;
typedef float       FLOAT;
typedef int         HRESULT;
typedef wchar_t     WCHAR;
typedef size_t      SIZE_T;
typedef const char* LPCSTR;
typedef char*       LPSTR;
typedef const void* LPCVOID;
typedef void*       LPVOID;
	#define WINAPI
#endif

#include <vector>

#if CRY_PLATFORM_WINDOWS

	#include <hash_map>

namespace stl
{

template<typename Key, typename Less = std::less<Key>>
struct hash_compare : stdext::hash_compare<Key, Less>
{
	hash_compare(){}
};

template<typename Key, typename Value, typename HashCompare = hash_compare<Key, std::less<Key>>>
struct hash_map : stdext::hash_map<Key, Value, HashCompare, std::allocator<std::pair<const Key, Value>>>
{
	hash_map(){}
};

}

#else

	#include <unordered_map>

namespace stl
{

template<typename Key, typename Less = std::less<Key>>
struct hash_compare : Less
{
	hash_compare(){}
	size_t operator()(const Key& kKey) const                     { return std::hash<Key>(kKey); }
	bool   operator()(const Key& kLeft, const Key& kRight) const { return Less::operator()(kLeft, kRight); }
};

template<typename Key, typename HashCompare>
struct hash : HashCompare
{
	hash(){}
	size_t operator()(const Key& kKey) const { return HashCompare::operator()(kKey); }
};

template<typename Key, typename HashCompare>
struct equal_to : HashCompare
{
	equal_to(){}
	bool operator()(const Key& kLeft, const Key& kRight) const { return !HashCompare::operator()(kLeft, kRight) && !HashCompare::operator()(kRight, kLeft); }
};

template<typename Key, typename Value, typename HashCompare = hash_compare<Key, std::less<Key>>>
struct hash_map : std::unordered_map<Key, Value, stl::hash<Key, HashCompare>, stl::equal_to<Key, HashCompare>>
{
	hash_map(){}
};

}

#endif

template<typename T>
struct _smart_ptr
{
	_smart_ptr(T* pPtr = NULL)
		: m_pPtr(pPtr)
	{
		if (m_pPtr)
			m_pPtr->AddRef();
	}

	_smart_ptr(const _smart_ptr& kOther)
		: m_pPtr(kOther.m_pPtr)
	{
		if (m_pPtr)
			m_pPtr->AddRef();
	}

	template<typename S>
	_smart_ptr(const _smart_ptr<S>& kOther)
		: m_pPtr(kOther.m_pPtr)
	{
		if (m_pPtr)
			m_pPtr->AddRef();
	}

	~_smart_ptr()
	{
		if (m_pPtr)
			m_pPtr->Release();
	}

	_smart_ptr& operator=(const _smart_ptr& kOther)
	{
		if (m_pPtr != kOther.m_pPtr)
		{
			if (m_pPtr)
				m_pPtr->Release();
			m_pPtr = kOther.m_pPtr;
			if (m_pPtr)
				m_pPtr->AddRef();
		}
		return *this;
	}

	template<typename S>
	_smart_ptr& operator=(const _smart_ptr<S>& kOther)
	{
		if (m_pPtr != kOther.m_pPtr)
		{
			if (m_pPtr)
				m_pPtr->Release();
			m_pPtr = kOther.m_pPtr;
			if (m_pPtr)
				m_pPtr->AddRef();
		}
		return *this;
	}

	bool operator==(T* pPtr) const
	{
		return m_pPtr == pPtr;
	}

	bool operator!=(T* pPtr) const
	{
		return m_pPtr != pPtr;
	}

	T& operator*() const
	{
		return *get();
	}

	T* operator->() const
	{
		return get();
	}

	operator T*() const
	{
		return get();
	}

	T* get() const
	{
		return m_pPtr;
	}

	T* m_pPtr;
};

#if !CRY_PLATFORM_WINDOWS

typedef struct tagRECT
{
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, * PRECT;

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

#endif

#define D3D10CreateBlob               D3DCreateBlob
#define CRY_OPENGL_ADAPT_CLIP_SPACE   1
#define CRY_OPENGL_FLIP_Y             1
#define CRY_OPENGL_MODIFY_PROJECTIONS !CRY_OPENGL_ADAPT_CLIP_SPACE

#include "../CryDXGL.hpp"
