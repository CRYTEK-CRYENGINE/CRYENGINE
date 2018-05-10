// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Deprecated/Variant.h"

namespace Schematyc2
{
	class CVariantStack
	{
	public:

		inline CVariantStack()
			: m_pData(nullptr)
			, m_size(0)
			, m_pos(0)
		{}

		inline CVariantStack(CVariant &data)
			: m_pData(&data)
			, m_size(1)
			, m_pos(0)
		{}

		template <size_t SIZE> inline CVariantStack(CVariant (&pData)[SIZE])
			: m_pData(pData)
			, m_size(SIZE)
			, m_pos(0)
		{}

		inline CVariantStack(std::vector<CVariant>& vector)
			: m_pData(vector.empty() == false ? &vector.front() : nullptr)
			, m_size(vector.empty() == false ? vector.size() : 0)
			, m_pos(0)
		{}

		inline CVariantStack(CVariant* pData, size_t size)
			: m_pData(pData)
			, m_size(pData ? size : 0)
			, m_pos(0)
		{}

		inline CVariantStack(const CVariantStack& rhs)
			: m_pData(rhs.m_pData)
			, m_size(rhs.m_size)
			, m_pos(rhs.m_pos)
		{}

		inline operator TVariantArray()
		{
			return TVariantArray(m_pData, m_pos);
		}

		inline operator TVariantConstArray() const
		{
			return TVariantConstArray(m_pData, m_pos);
		}

		template <typename TYPE> void Push(const TYPE& value)
		{
			CVariantArrayOutputArchive archive(TVariantArray(m_pData + m_pos, m_size - m_pos));
			archive(const_cast<TYPE&>(value), "", "");
			m_pos += archive.GetPos();
		}

		inline void Push(bool value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(int16 value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(uint16 value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(int32 value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(uint32 value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(float value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(const char* value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline void Push(const CVariant& value)
		{
			CRY_ASSERT(m_pos < m_size);
			if(m_pos < m_size)
			{
				m_pData[m_pos] = value;
				++ m_pos;
			}
		}

		inline CVariant Peek() const
		{
			CRY_ASSERT(m_pos <= m_size);
			if((m_pos > 0) && (m_pos <= m_size))
			{
				return m_pData[m_pos - 1];
			}
			else
			{
				return CVariant();
			}
		}

		inline CVariant Pop()
		{
			CRY_ASSERT((m_pos > 0) && (m_pos <= m_size));
			if((m_pos > 0) && (m_pos <= m_size))
			{
				-- m_pos;
				return m_pData[m_pos];
			}
			else
			{
				return CVariant();
			}
		}

		inline void Reset()
		{
			m_pos = 0;
		}

		inline CVariant operator [] (size_t pos) const
		{
			CRY_ASSERT(pos < m_pos);
			if(pos < m_pos)
			{
				return m_pData[pos];
			}
			else
			{
				return CVariant();
			}
		}

	private:

		CVariant* const m_pData;
		const size_t    m_size;
		size_t          m_pos;
	};

	template <size_t SIZE> class CInPlaceVariantStack : public CVariantStack
	{
	public:

		inline CInPlaceVariantStack()
			: CVariantStack(m_data)
		{}

	private:

		CVariant m_data[SIZE];
	};
}
