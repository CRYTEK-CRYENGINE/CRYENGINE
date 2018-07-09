// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryEndian.h
//  Version:     v1.00
//  Created:     16/2/2006 by Scott,Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:		 19/3/2007: Separated Endian support from basic TypeInfo declarations.
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CryEndian_h__
#define __CryEndian_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// Endian support
//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// NEED_ENDIAN_SWAP is an older define still used in several places to toggle endian swapping.
// It is only used when reading files which are assumed to be little-endian.
// For legacy support, define it to swap on big endian platforms.
/////////////////////////////////////////////////////////////////////////////////////

typedef bool EEndian;

#if defined(SYSTEM_IS_LITTLE_ENDIAN)
	#undef SYSTEM_IS_LITTLE_ENDIAN
#endif
#if defined(SYSTEM_IS_BIG_ENDIAN)
	#undef SYSTEM_IS_BIG_ENDIAN
#endif

#if 0 // no big-endian platforms right now, but keep the code
// Big-endian platform
	#define SYSTEM_IS_LITTLE_ENDIAN 0
	#define SYSTEM_IS_BIG_ENDIAN    1
#else
// Little-endian platform
	#define SYSTEM_IS_LITTLE_ENDIAN 1
	#define SYSTEM_IS_BIG_ENDIAN    0
#endif

#if SYSTEM_IS_BIG_ENDIAN
// Big-endian platform. Swap to/from little.
	#define eBigEndian    false
	#define eLittleEndian true
	#define NEED_ENDIAN_SWAP
#else
// Little-endian platform. Swap to/from big.
	#define eLittleEndian false
	#define eBigEndian    true
	#undef NEED_ENDIAN_SWAP
#endif

enum EEndianness
{
	eEndianness_Little,
	eEndianness_Big,
#if SYSTEM_IS_BIG_ENDIAN
	eEndianness_Native    = eEndianness_Big,
	eEndianness_NonNative = eEndianness_Little,
#else
	eEndianness_Native    = eEndianness_Little,
	eEndianness_NonNative = eEndianness_Big,
#endif
};

// Legacy macros
#define GetPlatformEndian() false

/////////////////////////////////////////////////////////////////////////////////////

inline bool IsSystemLittleEndian()
{
	const int a = 1;
	return 1 == *(const char*)&a;
}

/////////////////////////////////////////////////////////////////////////////////////

//! SwapEndian function, using TypeInfo.
struct CTypeInfo;
void SwapEndian(const CTypeInfo& Info, size_t nSizeCheck, void* pData, size_t nCount = 1, bool bWriting = false);

//! Default template utilizes TypeInfo.
template<class T>
ILINE void SwapEndianBase(T* t, size_t nCount = 1, bool bWriting = false)
{
	SwapEndian(TypeInfo(t), sizeof(T), t, nCount, bWriting);
}

/////////////////////////////////////////////////////////////////////////////////////
// SwapEndianBase functions.
// Always swap the data (the functions named SwapEndian swap based on an optional bSwapEndian parameter).
// The bWriting parameter must be specified in general when the output is for writing,
// but it matters only for types with bitfields.

// Overrides for base types.

template<>
ILINE void SwapEndianBase(char* p, size_t nCount, bool bWriting)
{}
template<>
ILINE void SwapEndianBase(uint8* p, size_t nCount, bool bWriting)
{}
template<>
ILINE void SwapEndianBase(int8* p, size_t nCount, bool bWriting)
{}

template<>
ILINE void SwapEndianBase(uint16* p, size_t nCount, bool bWriting)
{
	for (; nCount-- > 0; p++)
		*p = (uint16) (((*p >> 8) + (*p << 8)) & 0xFFFF);
}

template<>
ILINE void SwapEndianBase(int16* p, size_t nCount, bool bWriting)
{ SwapEndianBase((uint16*)p, nCount); }

template<>
ILINE void SwapEndianBase(uint32* p, size_t nCount, bool bWriting)
{
	for (; nCount-- > 0; p++)
		*p = (*p >> 24) + ((*p >> 8) & 0xFF00) + ((*p & 0xFF00) << 8) + (*p << 24);
}
template<>
ILINE void SwapEndianBase(int32* p, size_t nCount, bool bWriting)
{ SwapEndianBase((uint32*)p, nCount); }
template<>
ILINE void SwapEndianBase(float* p, size_t nCount, bool bWriting)
{ SwapEndianBase((uint32*)p, nCount); }

template<>
ILINE void SwapEndianBase(uint64* p, size_t nCount, bool bWriting)
{
	for (; nCount-- > 0; p++)
		*p = (*p >> 56) + ((*p >> 40) & 0xFF00) + ((*p >> 24) & 0xFF0000) + ((*p >> 8) & 0xFF000000)
		     + ((*p & 0xFF000000) << 8) + ((*p & 0xFF0000) << 24) + ((*p & 0xFF00) << 40) + (*p << 56);
}
template<>
ILINE void SwapEndianBase(int64* p, size_t nCount, bool bWriting)
{ SwapEndianBase((uint64*)p, nCount); }
template<>
ILINE void SwapEndianBase(double* p, size_t nCount, bool bWriting)
{ SwapEndianBase((uint64*)p, nCount); }

//! ---------------------------------------------------------------------------.
//! SwapEndian functions.
//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T>
ILINE void SwapEndian(T* t, size_t nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}

//! Specify int and uint as well as size_t, to resolve overload ambiguities.
//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T>
ILINE void SwapEndian(T* t, int nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}

#if CRY_PLATFORM_64BIT
//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T>
ILINE void SwapEndian(T* t, unsigned int nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}
#endif

//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T>
ILINE void SwapEndian(T& t, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(&t, 1);
}

//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T>
ILINE T SwapEndianValue(T t, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(&t, 1);
	return t;
}

//! ---------------------------------------------------------------------------.
//! Object-oriented data extraction for endian-swapping reading.
//! \param bSwapEndian Optional, defaults to swapping from LittleEndian format.
template<class T, class D>
inline T* StepData(D*& pData, size_t nCount, bool bSwapEndian)
{
	T* Elems = (T*)pData;
	SwapEndian(Elems, nCount, bSwapEndian);
	pData = (D*)((T*)pData + nCount);
	return Elems;
}

template<class T, class D>
inline T* StepData(D*& pData, bool bSwapEndian)
{
	return StepData<T, D>(pData, 1, bSwapEndian);
}

template<class T, class D>
inline void StepData(T*& Result, D*& pData, size_t nCount, bool bSwapEndian)
{
	Result = StepData<T, D>(pData, nCount, bSwapEndian);
}

template<class T, class D>
inline void StepDataCopy(T* Dest, D*& pData, size_t nCount, bool bSwapEndian)
{
	memcpy(Dest, pData, nCount * sizeof(T));
	SwapEndian(Dest, nCount, bSwapEndian);
	pData = (D*)((T*)pData + nCount);
}

template<class T, class D>
inline void StepDataWrite(D*& pDest, const T* aSrc, size_t nCount, bool bSwapEndian)
{
	memcpy(pDest, aSrc, nCount * sizeof(T));
	if (bSwapEndian)
		SwapEndianBase((T*)pDest, nCount, true);
	(T*&)pDest += nCount;
}

template<class T, class D>
inline void StepDataWrite(D*& pDest, const T& Src, bool bSwapEndian)
{
	StepDataWrite(pDest, &Src, 1, bSwapEndian);
}

#endif // __CryEndian_h__
