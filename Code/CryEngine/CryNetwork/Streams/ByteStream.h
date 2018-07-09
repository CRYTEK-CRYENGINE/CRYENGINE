// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BYTESTREAM_H__
#define __BYTESTREAM_H__

#pragma once

#include <CryCore/Containers/MiniQueue.h>
#include <CryCore/Typelist.h>
#include "NetProfile.h"

// abstracts away offset selection for byte buffer manipulation; used for both reading and writing
// tries to pack data into a byte buffer smartly:
//   4 byte or bigger things are aligned on 4 byte boundaries
//   2 byte things are aligned on 2 byte boundaries
//   1 byte things are aligned on 1 byte boundaries
// selection of where to place something is done in a branch free manner for fixed sized things
// TODO: determine if our 64-bit targets require 64-bit alignment for 64-bit values, and expand this class as needed
class CByteStreamPacker
{
public:
	CByteStreamPacker()
	{
		for (int i = 0; i < 3; i++)
			m_data[i] = 0;
	}

	uint32 GetNextOfs(uint32 sz);

	template<int N>
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<N> )
	{
		return GetNextOfs(N);
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<0> )
	{
		return m_data[eD_Size];
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<1> )
	{
		// SEL is a lookup table (packed into bits)
		static const uint32 SEL = 0x0e; // binary 1110
		// selector will be 0 or 1
		//   if there was a valid size 1 location free, it's 1
		//   otherwise it's 0
		// a valid size 1 location exists if Ofs1 is not a multiple of four
		uint32 selector = (SEL >> (m_data[eD_Ofs1] & 3)) & 1;

		// use this value to lookup the offset we need to use
		// (produces a useless copy of selector == 1, but eliminates a branch)
		uint32 out = m_data[eD_Ofs1] = m_data[selector];
		m_data[eD_Ofs1]++;
		m_data[eD_Size] += 4 - 4 * selector;

		return out;
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<2> )
	{
		// SEL is a lookup table (packed into bits)
		static const uint32 SEL = 0x1c; // binary 1110<<1
		// selector will be 0 or 2
		//   if there was a valid size 2 location free, it's 2
		//   otherwise it's 0
		// a valid size 2 location exists if Ofs2 is not a multiple of four
		uint32 selector = (SEL >> (m_data[eD_Ofs2] & 3)) & 2;

		// use this value to lookup the offset we need to use
		// (produces a useless copy of selector == 2, but eliminates a branch)
		uint32 out = m_data[eD_Ofs2] = m_data[selector];
		m_data[eD_Ofs2] += 2;
		m_data[eD_Size] += 4 - 2 * selector;

		return out;
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<4> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 4;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<8> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 8;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<12> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 12;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed(NTypelist::Int2Type<16> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 16;
		return ofs;
	}

protected:
	enum EData
	{
		eD_Size = 0, // size of our output
		eD_Ofs1,     // position of the next free size 1 element
		eD_Ofs2,     // position of the next free size 2 element
	};
	uint32 m_data[3];
};

class CByteOutputStream : private CByteStreamPacker
{
public:
	CByteOutputStream(IStreamAllocator* pSA, size_t initSize = 16, void* caller = 0) : m_pSA(pSA), m_capacity(initSize), m_buffer((uint8*)pSA->Alloc(initSize, caller ? caller : UP_STACK_PTR))
	{
		memset(m_buffer, 0, initSize);
	}

	// the horrible slow way to write variable sized things
	void Put(const void* pWhat, size_t sz)
	{
		uint32 where = GetNextOfs(sz);
		if (m_data[eD_Size] > m_capacity)
			Grow(m_data[eD_Size]);
		memcpy(m_buffer + where, pWhat, sz);
#if NET_PROFILE_ENABLE
		m_bytes += sz;
#endif
	}

	// the nice fast way to write known size things
	template<class T>
	ILINE T& PutTyped()
	{
		uint32 where = GetNextOfs_Fixed(NTypelist::Int2Type<sizeof(T)>());
		if (m_data[eD_Size] > m_capacity)
			Grow(m_data[eD_Size]);
#if NET_PROFILE_ENABLE
		m_bytes += sizeof(T);
#endif
		return *reinterpret_cast<T*>(m_buffer + where);
	}

	ILINE void PutByte(uint8 c)
	{
		PutTyped<uint8>() = c;
	}

	ILINE size_t GetSize()
	{
		return m_data[eD_Size];
	}

	const uint8* GetBuffer() const
	{
		return m_buffer;
	}

#if NET_PROFILE_ENABLE
	void ConditionalPrelude()  { m_bytes = 0; }
	void ConditionalPostlude() { NET_PROFILE_ADD_WRITE_BITS(m_bytes * 8); }
#endif

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CByteOutputStream");

		pSizer->Add(*this);
		if (m_pSA)
			pSizer->Add(m_buffer, m_capacity);
	}

private:
	void Grow(size_t sz);

	IStreamAllocator* m_pSA;
	uint32            m_capacity;
	uint8*            m_buffer;
#if NET_PROFILE_ENABLE
	uint32            m_bytes;
#endif
};

class CByteInputStream : private CByteStreamPacker
{
public:
	CByteInputStream(const uint8* pData, size_t sz) : m_pData(pData), m_capacity(sz)
	{
	}

	~CByteInputStream()
	{
	}

	ILINE void Get(void* pWhere, size_t sz)
	{
		NET_ASSERT(sz <= m_data[eD_Size]);
		memcpy(pWhere, Get(sz), sz);
#if NET_PROFILE_ENABLE
		m_bytes += sz;
#endif
	}

	ILINE const void* Get(size_t sz)
	{
		uint32 where = GetNextOfs(sz);
		NET_ASSERT((where + sz) <= m_capacity);
#if NET_PROFILE_ENABLE
		m_bytes += sz;
#endif
		return m_pData + where;
	}

	template<class T>
	ILINE const T& GetTyped()
	{
		uint32 where = GetNextOfs_Fixed(NTypelist::Int2Type<sizeof(T)>());
		NET_ASSERT((where + sizeof(T)) <= m_capacity);
#if NET_PROFILE_ENABLE
		m_bytes += sizeof(T);
#endif
		return *static_cast<const T*>(static_cast<const void*>(m_pData + where));
	}

	ILINE uint8 GetByte()
	{
		return GetTyped<uint8>();
	}

#if NET_PROFILE_ENABLE
	void ConditionalPrelude()  { m_bytes = 0; }
	void ConditionalPostlude() { NET_PROFILE_ADD_READ_BITS(m_bytes * 8); }
#endif

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CByteInputStream");

		pSizer->Add(*this);
		pSizer->Add(m_pData, m_capacity);
	}

	size_t GetSize() const
	{
		return m_data[eD_Size];
	}

private:
	const uint8* m_pData;
	size_t       m_capacity;
#if NET_PROFILE_ENABLE
	uint32       m_bytes;
#endif
};

#endif
