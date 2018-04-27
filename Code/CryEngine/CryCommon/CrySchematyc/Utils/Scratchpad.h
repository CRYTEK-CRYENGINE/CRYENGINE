// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move functions to .inl file?
// #SchematycTODO : Do we need to worry about data alignment?

#pragma once

#include <type_traits>

#include <CryMath/Cry_Math.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Reflection/TypeDesc.h"
#include "CrySchematyc/Utils/Any.h"

namespace Schematyc
{

class CScratchpad
{
private:

	class CAnyValueImpl : public CAnyValue
	{
	public:

		inline CAnyValueImpl(const CCommonTypeDesc& typeDesc, const void* pValue)
			: CAnyValue(typeDesc, pValue)
		{}
	};

public:

	inline CScratchpad() {}

	inline CScratchpad(const CScratchpad& rhs)
	{
		Copy(rhs);
	}

	explicit inline CScratchpad(const SInPlaceStorageParams& storage)
		: m_capacity(storage.capacity)
		, m_pData(static_cast<uint8*>(storage.pData))
		, m_bInPlace(true)
	{}

	inline CScratchpad(const SInPlaceStorageParams& storage, const CScratchpad& rhs)
		: m_capacity(storage.capacity)
		, m_pData(static_cast<uint8*>(storage.pData))
		, m_bInPlace(true)
	{
		Copy(rhs);
	}

	inline ~CScratchpad()
	{
		Clear();
		if (m_pData && !m_bInPlace)
		{
			CryModuleFree(m_pData);
		}
	}

	inline void Reserve(uint32 capacity)
	{
		if (capacity > m_capacity)
		{
			static const uint32 minCapacity = 16;
			static const uint32 growthFactor = 2;
			m_capacity = max(max(capacity, m_capacity * growthFactor), minCapacity);

			uint8* pData = static_cast<uint8*>(CryModuleMalloc(m_capacity));

			if (m_pData)
			{
				MoveData(pData, m_pData, m_size);
				if (!m_bInPlace)
				{
					CryModuleFree(m_pData);
				}
			}

			m_pData = pData;
			m_bInPlace = false;
		}
	}

	inline uint32 Add(const CAnyConstRef& value)
	{
		const CCommonTypeDesc& typeDesc = value.GetTypeDesc();

		const uint32 size = m_size + sizeof(CAnyValueImpl) + typeDesc.GetSize();
		Reserve(size);

		const uint32 pos = m_size;
		m_size = size;

		new(m_pData + pos)CAnyValueImpl(typeDesc, value.GetValue());

		return pos;
	}

	inline CAnyPtr Get(uint32 pos)
	{
		return pos < m_size ? reinterpret_cast<CAnyValue*>(m_pData + pos) : CAnyPtr();
	}

	inline CAnyConstPtr Get(uint32 pos) const
	{
		return pos < m_size ? reinterpret_cast<const CAnyValue*>(m_pData + pos) : CAnyConstPtr();
	}

	inline void ShrinkToFit()
	{
		if (m_pData && !m_bInPlace)
		{
			static const uint32 shrinkThreshold = 64;
			if ((m_capacity - m_size) >= shrinkThreshold)
			{
				uint8* pData = static_cast<uint8*>(CryModuleMalloc(m_capacity));
				MoveData(pData, m_pData, m_size);

				CryModuleFree(m_pData);

				m_capacity = m_size;
				m_pData = pData;
			}
		}
	}

	inline void Clear()
	{
		if (m_pData)
		{
			ClearData(m_pData, m_size);

			m_size = 0;
		}
	}

	inline void operator=(const CScratchpad& rhs)
	{
		Copy(rhs);
	}

	inline bool Compare(const CScratchpad &rhs) const
	{
		if (m_size != rhs.m_size)
			return false;
		return 0 == memcmp(m_pData,rhs.m_pData,m_size);
	}

protected:

	inline void Copy(const CScratchpad& rhs)
	{
		if (this != &rhs)
		{
			Clear();
			Reserve(rhs.m_size);
			CloneData(m_pData, rhs.m_pData, rhs.m_size);
			m_size = rhs.m_size;
		}
	}

private:

	inline void CloneData(uint8* pDst, const uint8* pSrc, uint32 size) const
	{
		for (uint32 pos = 0; pos < size; )
		{
			const CAnyValueImpl* pSrcValue = reinterpret_cast<const CAnyValueImpl*>(pSrc + pos);
			const CCommonTypeDesc& typeDesc = pSrcValue->GetTypeDesc();
			new(pDst + pos)CAnyValueImpl(typeDesc, pSrcValue->GetValue());
			pos += sizeof(CAnyValueImpl) + typeDesc.GetSize();
		}
	}

	inline void ClearData(uint8* pData, uint32 size) const
	{
		for (uint32 pos = 0; pos < size; )
		{
			CAnyValueImpl* pValue = reinterpret_cast<CAnyValueImpl*>(pData + pos);
			const CCommonTypeDesc& typeDesc = pValue->GetTypeDesc();
			pos += sizeof(CAnyValueImpl) + typeDesc.GetSize();
			pValue->~CAnyValueImpl();
		}
	}

	inline void MoveData(uint8* pDst, uint8* pSrc, uint32 size) const
	{
		CloneData(pDst, pSrc, size);
		ClearData(pSrc, size);
	}

private:

	uint32 m_capacity = 0;
	uint32 m_size = 0;
	uint8* m_pData = nullptr;
	bool   m_bInPlace = false;
};

template<uint32 CAPACITY> class CInPlaceScratchpad : public CScratchpad
{
public:

	inline CInPlaceScratchpad()
		: CScratchpad(SInPlaceStorageParams(CAPACITY, m_storage))
	{}

	inline CInPlaceScratchpad(const CScratchpad& rhs)
		: CScratchpad(SInPlaceStorageParams(CAPACITY, m_storage), rhs)
	{}

	inline CInPlaceScratchpad(const CInPlaceScratchpad& rhs)
		: CScratchpad(SInPlaceStorageParams(CAPACITY, m_storage), rhs)
	{}

	template <uint32 RHS_CAPACITY> inline CInPlaceScratchpad(const CInPlaceScratchpad<RHS_CAPACITY>& rhs)
		: CScratchpad(SInPlaceStorageParams(CAPACITY, m_storage), rhs)
	{}

private:

	uint8 m_storage[CAPACITY];
};

typedef CScratchpad             HeapScratchpad;
typedef CInPlaceScratchpad<256> StackScratchpad;

} // Schematyc
