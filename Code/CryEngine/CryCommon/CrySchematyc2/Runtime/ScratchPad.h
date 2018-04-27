// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Separate POD and non-POD data?
// #SchematycTODO : Does this class really need to live in public interface?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/IAny.h"

namespace Schematyc2
{
	class CScratchPad
	{
	public:

		inline CScratchPad()
			: m_pData(nullptr)
			, m_size(0)
			, m_capacity(0)
		{}

		inline CScratchPad(const CScratchPad& rhs)
			: m_size(0)
			, m_capacity(0)
			, m_pData(nullptr)
		{
			Reserve(rhs.m_size);
			Clone(m_pData, rhs.m_pData, rhs.m_size);
			m_size = rhs.m_size;
		}

		inline ~CScratchPad()
		{
			Release(m_pData, m_size);
			if(m_pData)
			{
				free(m_pData);
			}
		}

		inline void Reserve(uint32 capacity)
		{
			if(capacity > m_capacity)
			{
				static const uint32 s_minCapacity  = 2;
				static const uint32 s_growthFactor = 2;
				m_capacity = std::max(std::max(capacity, m_capacity * s_growthFactor), s_minCapacity);
				void* pData = malloc(m_capacity);
				Move(pData, m_pData, m_size);
				if(m_pData)
				{
					free(m_pData);
				}
				m_pData = pData;
			}
		}

		inline uint32 PushBack(const IAny& value)
		{
			const uint32 pos = m_size;
			const uint32 size = pos + value.GetSize();
			if(size > m_capacity)
			{
				Reserve(size);
			}
			value.Clone(static_cast<uint8*>(m_pData) + pos);
			m_size = size;
			return pos;
		}

		inline void ShrinkToFit()
		{
			// #SchematycTODO : Check difference between size and capacity against a threshold.
			void* pData = malloc(m_size);
			Move(pData, m_pData, m_size);
			free(m_pData);
			m_capacity = m_size;
			m_pData    = pData;
		}

		inline void Clear()
		{
			Release(m_pData, m_size);
			if(m_pData)
			{
				free(m_pData);
			}
			m_size     = 0;
			m_capacity = 0;
			m_pData    = nullptr;
		}

		inline CScratchPad& operator = (const CScratchPad& rhs)
		{
			Clear();
			Reserve(rhs.m_size);
			Clone(m_pData, rhs.m_pData, rhs.m_size);
			m_size = rhs.m_size;
			return *this;
		}

		inline IAny* operator [] (uint32 pos)
		{
			// #SchematycTODO : Verify position?
			return reinterpret_cast<IAny*>(static_cast<uint8*>(m_pData) + pos);
		}

		inline const IAny* operator [] (uint32 pos) const
		{
			// #SchematycTODO : Verify position?
			return reinterpret_cast<const IAny*>(static_cast<uint8*>(m_pData) + pos);
		}

	private:

		inline void Clone(void* pDst, const void* pSrc, uint32 size) const
		{
			for(uint32 pos = 0; pos < size; )
			{
				const IAny* pSrcValue = reinterpret_cast<const IAny*>(static_cast<const uint8*>(pSrc) + pos);
				pSrcValue->Clone(static_cast<uint8*>(pDst) + pos);
				pos += pSrcValue->GetSize();
			}
		}

		inline void Release(void* pData, uint32 size) const
		{
			for(uint32 pos = 0; pos < size; )
			{
				IAny* pValue = reinterpret_cast<IAny*>(static_cast<uint8*>(pData) + pos);
				pos += pValue->GetSize();
				pValue->~IAny();
			}
		}

		inline void Move(void* pDst, void* pSrc, uint32 size) const
		{
			Clone(pDst, pSrc, size);
			Release(pSrc, size);
		}

	private:

		uint32 m_size;
		uint32 m_capacity;
		void*  m_pData;
	};
}
