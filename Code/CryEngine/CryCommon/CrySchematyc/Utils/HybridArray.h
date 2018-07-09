// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Do we need to worry about data alignment?

#pragma once

#include <CryMath/Cry_Math.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Assert.h"

namespace Schematyc
{

template<typename VALUE_TYPE> class CHybridArray
{
public:

	inline CHybridArray() {}

	inline CHybridArray(const CHybridArray& rhs)
	{
		push_back(rhs.size(), rhs.begin());
	}

	explicit inline CHybridArray(const SInPlaceStorageParams& storage)
		: m_capacity(storage.capacity / sizeof(VALUE_TYPE))
		, m_pData(static_cast<VALUE_TYPE*>(storage.pData))
		, m_bInPlace(true)
	{}

	inline CHybridArray(const SInPlaceStorageParams& storage, const CHybridArray& rhs)
		: m_capacity(storage.capacity / sizeof(VALUE_TYPE))
		, m_pData(static_cast<VALUE_TYPE*>(storage.pData))
		, m_bInPlace(true)
	{
		push_back(rhs.size(), rhs.begin());
	}

	inline ~CHybridArray()
	{
		clear();
		if (m_pData && !m_bInPlace)
		{
			CryModuleFree(m_pData);
		}
	}

	inline void reserve(uint32 capacity)
	{
		if (capacity > m_capacity)
		{
			static const uint32 minCapacity = 16;
			static const uint32 growthFactor = 2;
			m_capacity = max(max(capacity, m_capacity * growthFactor), minCapacity);

			VALUE_TYPE* pData = static_cast<VALUE_TYPE*>(CryModuleMalloc(sizeof(VALUE_TYPE) * m_capacity));

			if (m_pData)
			{
				std::copy(m_pData, m_pData + m_size, pData);
				if (!m_bInPlace)
				{
					CryModuleFree(m_pData);
				}
			}

			m_pData = pData;
			m_bInPlace = false;
		}
	}

	inline VALUE_TYPE& push_back(const VALUE_TYPE& value)
	{
		reserve(m_size + 1);
		VALUE_TYPE* pValue = new(m_pData + m_size)VALUE_TYPE(value);
		++m_size;
		return *pValue;
	}

	inline void push_back(uint32 size, const VALUE_TYPE* pData)
	{
		reserve(m_size + size);
		for (VALUE_TYPE* pPos = m_pData + m_size, * pEnd = pPos + size; pPos != pEnd; ++pPos)
		{
			new(pPos)VALUE_TYPE(*pData);
			++pData;
		}
		m_size += size;
	}

	template<typename ... PARAM_TYPES> inline VALUE_TYPE& emplace_back(PARAM_TYPES&& ... params)
	{
		reserve(m_size + 1);
		VALUE_TYPE* pValue = new(m_pData + m_size)VALUE_TYPE(std::forward<PARAM_TYPES>(params) ...);
		++m_size;
		return *pValue;
	}

	inline void pop_back()
	{
		SCHEMATYC_CORE_ASSERT(m_size > 0);
		--m_size;
		m_pData[m_size].~VALUE_TYPE();
	}

	inline void clear()
	{
		for (VALUE_TYPE* pPos = m_pData, * pEnd = pPos + m_size; pPos != pEnd; ++pPos)
		{
			pPos->~VALUE_TYPE();
		}
		m_size = 0;
	}

	inline uint32 capacity() const
	{
		return m_capacity;
	}

	inline bool empty() const
	{
		return m_size > 0;
	}

	inline uint32 size() const
	{
		return m_size;
	}

	inline VALUE_TYPE* begin()
	{
		return m_pData;
	}

	inline const VALUE_TYPE* begin() const
	{
		return m_pData;
	}

	inline VALUE_TYPE* end()
	{
		return m_pData + m_size;
	}

	inline const VALUE_TYPE* end() const
	{
		return m_pData + m_size;
	}

	inline CHybridArray& operator=(const CHybridArray& rhs)
	{
		clear();
		push_back(rhs.size(), rhs.begin());
		return *this;
	}

	inline VALUE_TYPE& operator[](uint32 idx)
	{
		SCHEMATYC_CORE_ASSERT(idx < m_size);
		return m_pData[idx];
	}

	inline const VALUE_TYPE& operator[](uint32 idx) const
	{
		SCHEMATYC_CORE_ASSERT(idx < m_size);
		return m_pData[idx];
	}

private:

	uint32      m_capacity = 0;
	uint32      m_size = 0;
	VALUE_TYPE* m_pData = nullptr;
	bool        m_bInPlace = false;
};

} // Schematyc
