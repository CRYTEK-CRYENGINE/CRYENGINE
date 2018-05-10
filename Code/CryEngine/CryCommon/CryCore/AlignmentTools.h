// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <stddef.h>
#include <type_traits>

namespace Detail
{
template<typename RealType, typename BlittedElement>
struct Blitter
{
	static_assert(std::is_trivial<BlittedElement>::value, "Blittable elements should be trivial (ie, integral types)");
	static_assert((std::alignment_of<BlittedElement>::value < std::alignment_of<RealType>::value), "Blittable memory has sufficient alignment, do not use unaligned store or load");
	static_assert(sizeof(RealType) > sizeof(BlittedElement), "Blittable element is larger than real type");
	static_assert((sizeof(RealType) % sizeof(BlittedElement)) == 0, "Blitted element has the wrong size for the real type");
	typedef std::integral_constant<size_t, sizeof(RealType) / sizeof(BlittedElement)> NumElements;

	static void BlitLoad(const BlittedElement* pSource, RealType& target)
	{
		BlittedElement* pTarget = reinterpret_cast<BlittedElement*>(&target);
		for (size_t i = 0; i < NumElements::value; ++i, ++pSource, ++pTarget)
		{
			* pTarget = *pSource;
		}
	}

	static void BlitStore(const RealType& source, BlittedElement* pTarget)
	{
		const BlittedElement* pSource = reinterpret_cast<const BlittedElement*>(&source);
		for (size_t i = 0; i < NumElements::value; ++i, ++pSource, ++pTarget)
		{
			* pTarget = *pSource;
		}
	}
};

//! std::aligned_storage is not guaranteed to work for "over-aligned" types, but only up to alignof(::max_align_t).
//! This actually is the case on MSVC, where std::aligned_storage refuses to align anything to more than 8 bytes.
template<size_t Align> struct SAlignedStorageHelper
{
	static_assert(Align != Align, "Requested alignment is not a power-of-two or is too large for the system");
};
template<> struct SAlignedStorageHelper<1>
{
	typedef CRY_ALIGN (1) char type;
};
template<> struct SAlignedStorageHelper<2>
{
	typedef CRY_ALIGN (2) char type;
};
template<> struct SAlignedStorageHelper<4>
{
	typedef CRY_ALIGN (4) char type;
};
template<> struct SAlignedStorageHelper<8>
{
	typedef CRY_ALIGN (8) char type;
};
template<> struct SAlignedStorageHelper<16>
{
	typedef CRY_ALIGN (16) char type;
};
template<> struct SAlignedStorageHelper<32>
{
	typedef CRY_ALIGN (32) char type;
};
template<> struct SAlignedStorageHelper<64>
{
	typedef CRY_ALIGN (64) char type;
};
template<> struct SAlignedStorageHelper<128>
{
	typedef CRY_ALIGN (128) char type;
};
template<> struct SAlignedStorageHelper<256>
{
	typedef CRY_ALIGN (256) char type;
};
template<> struct SAlignedStorageHelper<512>
{
	typedef CRY_ALIGN (512) char type;
};
template<> struct SAlignedStorageHelper<1024>
{
	typedef CRY_ALIGN (1024) char type;
};
template<> struct SAlignedStorageHelper<2048>
{
	typedef CRY_ALIGN (2048) char type;
};
template<> struct SAlignedStorageHelper<4096>
{
	typedef CRY_ALIGN (4096) char type;
};
template<> struct SAlignedStorageHelper<8192>
{
	typedef CRY_ALIGN (8192) char type;
};

template<size_t Length, size_t Align>
struct SShouldUseFallback
{
	typedef typename std::aligned_storage<Length, Align>::type TStandardType;
	static const bool value = std::alignment_of<TStandardType>::value < Align;
};
}

//! Provides a member typedef 'type' that is a POD-type suitable for storing an object of the given length and alignment.
//! Unlike std::aligned_storage, this will either meet the requested alignment value or fail to compile.
template<size_t Length, size_t Align = TARGET_DEFAULT_ALIGN, bool bFallback = Detail::SShouldUseFallback<Length, Align>::value>
struct SAlignedStorage
{
	typedef typename std::aligned_storage<Length, Align>::type type;
	static_assert(sizeof(type) >= Length && std::alignment_of<type>::value >= Align && std::is_pod<type>::value, "Failed to provide requested alignment");
};
template<size_t Length, size_t Align>
struct SAlignedStorage<Length, Align, true>
{
	typedef union
	{
		char storage[Length];
		typename Detail::SAlignedStorageHelper<Align>::type helper;
	} type;
	static_assert(sizeof(type) >= Length && std::alignment_of<type>::value >= Align && std::is_pod<type>::value, "Failed to provide requested alignment");
};

//! Load RealType from unaligned memory using some blittable type.
//! The source memory must be suitably aligned for accessing BlittedElement.
//! If no memory alignment can be guaranteed, use char for BlittedElement.
template<typename RealType, typename BlittedElement>
inline void LoadUnaligned(const BlittedElement* pMemory, RealType& value,
                          typename std::enable_if<(std::alignment_of<RealType>::value > std::alignment_of<BlittedElement>::value)>::type* = nullptr)
{
	MEMORY_RW_REORDERING_BARRIER;
	Detail::Blitter<RealType, BlittedElement>::BlitLoad(pMemory, value);
	MEMORY_RW_REORDERING_BARRIER;
}

//! Load RealType from aligned memory (fallback overload).
//! This is used if there is no reason to call the blitter, because sufficient alignment is guaranteed by BlittedElement.
template<typename RealType, typename BlittedElement>
inline void LoadUnaligned(const BlittedElement* pMemory, RealType& value,
                          typename std::enable_if<(std::alignment_of<RealType>::value <= std::alignment_of<BlittedElement>::value)>::type* = nullptr)
{
	MEMORY_RW_REORDERING_BARRIER;
	value = *reinterpret_cast<const RealType*>(pMemory);
	MEMORY_RW_REORDERING_BARRIER;
}

//! Store to unaligned memory using some blittable type.
//! The target memory must be suitably aligned for accessing BlittedElement.
//! If no memory alignment can be guaranteed, use char for BlittedElement.
template<typename RealType, typename BlittedElement>
inline void StoreUnaligned(BlittedElement* pMemory, const RealType& value,
                           typename std::enable_if<(std::alignment_of<RealType>::value > std::alignment_of<BlittedElement>::value)>::type* = nullptr)
{
	MEMORY_RW_REORDERING_BARRIER;
	Detail::Blitter<RealType, BlittedElement>::BlitStore(value, pMemory);
	MEMORY_RW_REORDERING_BARRIER;
}

//! Store to aligned memory (fallback overload).
//! This is used if there is no reason to call the blitter, because sufficient alignment is guaranteed by BlittedElement.
template<typename RealType, typename BlittedElement>
inline void StoreUnaligned(BlittedElement* pMemory, const RealType& value,
                           typename std::enable_if<(std::alignment_of<RealType>::value <= std::alignment_of<BlittedElement>::value)>::type* = nullptr)
{
	MEMORY_RW_REORDERING_BARRIER;
	*reinterpret_cast<RealType*>(pMemory) = value;
	MEMORY_RW_REORDERING_BARRIER;
}

//! Pads the given pointer to the next possible aligned location for RealType.
//! Use this to ensure RealType can be referenced in some buffer of BlittedElement's, without using LoadUnaligned/StoreUnaligned.
template<typename RealType, typename BlittedElement>
inline BlittedElement* AlignPointer(BlittedElement* pMemory,
                                    typename std::enable_if<(std::alignment_of<RealType>::value % std::alignment_of<BlittedElement>::value) == 0>::type* = nullptr)
{
	const size_t align = std::alignment_of<RealType>::value;
	const size_t mask = align - 1;
	const size_t address = reinterpret_cast<size_t>(pMemory);
	const size_t offset = (align - (address & mask)) & mask;
	return pMemory + (offset / sizeof(BlittedElement));
}

//! Pads the given address to the next possible aligned location for RealType.
//! Use this to ensure RealType can be referenced inside memory, without using LoadUnaligned/StoreUnaligned.
template<typename RealType>
inline size_t AlignAddress(size_t address)
{
	return reinterpret_cast<size_t>(AlignPointer<RealType>(reinterpret_cast<char*>(address)));
}

//! Provides aligned storage for T, optionally aligned at a specific boundary (default being the native alignment of T).
//! The specified T is not initialized automatically, use of placement new/delete is the user's responsibility.
template<typename T, size_t Align = std::alignment_of<T>::value>
struct SUninitialized
{
	typedef typename SAlignedStorage<sizeof(T), Align>::type Storage;
	Storage storage;

	void    DefaultConstruct()
	{
		new(static_cast<void*>(&storage))T();
	}

	void CopyConstruct(const T& value)
	{
		new(static_cast<void*>(&storage))T(value);
	}

	void MoveConstruct(T&& value)
	{
		new(static_cast<void*>(&storage))T(std::move(value));
	}

	void Destruct()
	{
		reinterpret_cast<T*>(&storage)->~T();
	}

	operator T&()
	{
		return *reinterpret_cast<T*>(&storage);
	}
};
