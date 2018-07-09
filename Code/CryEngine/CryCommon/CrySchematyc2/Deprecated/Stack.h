// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Figure out how to deal with dynamic containers!!!
// #SchematycTODO : Ability to set stack frames or create sub-stacks so that when entering a new scope we can ensure correct number of params are pushed/popped?
// #SchematycTODO : Option to store debug information and validate serialization?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Deprecated/Variant.h"

namespace Schematyc2
{
	class CStack
	{
	public:

		inline CStack(const size_t capacity = 0)
			: m_pData(nullptr)
			, m_pFreeFunction(nullptr)
			, m_capacity(0)
			, m_size(0)
		{
			Reserve(capacity);
		}

		inline ~CStack()
		{
			for(CVariant* pValue = m_pData, *pEnd = pValue + m_size; pValue != pEnd; ++ pValue)
			{
				pValue->~CVariant();
			}
			if(m_pFreeFunction)
			{
				(*m_pFreeFunction)(m_pData);
			}
		}

		inline void Reserve(size_t capacity)
		{
			static const size_t minCapacity = 32; // Moved from static const member to fix weird PS4 compilation issue.
			static const size_t growthFactor = 2; // Moved from static const member to fix weird PS4 compilation issue.
			if(capacity > m_capacity)
			{
				capacity = std::max(std::max(capacity, m_capacity * growthFactor), minCapacity);
				CVariant* pData = static_cast<CVariant*>(CryModuleMalloc(sizeof(CVariant) * capacity));
				Move(pData, m_pData, m_pData + m_size);
				if(m_pFreeFunction)
				{
					(*m_pFreeFunction)(m_pData);
				}
				m_pData         = pData;
				m_pFreeFunction = CryModuleFree;
				m_capacity      = capacity;
			}
		}

		inline void Push(const CVariant& value)
		{
			Reserve(m_size + 1);
			new (m_pData + m_size) CVariant(value);
			m_size ++;
		}

		inline void Pop()
		{
			CRY_ASSERT(m_size);
			-- m_size;
			m_pData[m_size].~CVariant();
		}

		inline CVariant& Top()
		{
			CRY_ASSERT(m_size);
			return m_pData[m_size - 1];
		}

		inline const CVariant& Top() const
		{
			CRY_ASSERT(m_size);
			return m_pData[m_size - 1];
		}

		inline bool Empty() const
		{
			return !m_size;
		}

		inline size_t Size() const
		{
			return m_size;
		}

		inline operator TVariantArray() // Temporary, just until we can replace old CVariantStack class.
		{
			return TVariantArray(m_pData, m_size);
		}

		inline operator TVariantConstArray() const // Temporary, just until we can replace old CVariantStack class.
		{
			return TVariantConstArray(m_pData, m_size);
		}

		inline CVariant& operator [] (const size_t pos) 
		{
			CRY_ASSERT(pos < m_size);
			return m_pData[pos];
		}

		inline const CVariant& operator [] (const size_t pos) const
		{
			CRY_ASSERT(pos < m_size);
			return m_pData[pos];
		}

	protected:

		typedef void (*FreeFunctionPtr)(void*);

		inline CStack(void* const pData, const FreeFunctionPtr pFreeFunction, const size_t capacity)
			: m_pData(static_cast<CVariant*>(pData))
			, m_pFreeFunction(pFreeFunction)
			, m_capacity(capacity)
			, m_size(0)
		{}

	private:

		inline void Move(CVariant* pDst, const CVariant* pSrc, const CVariant* pEnd) const
		{
			for( ; pSrc != pEnd; ++ pDst, ++ pSrc)
			{
				*pDst = *pSrc;
				pSrc->~CVariant();
			}
		}

		CVariant*       m_pData;
		FreeFunctionPtr m_pFreeFunction;
		size_t          m_capacity;
		size_t          m_size;
	};

	class CInPlaceStack : public CStack
	{
	public:

		inline CInPlaceStack()
			: CStack(m_data, nullptr, CAPACITY)
		{}

	private:

		static const size_t CAPACITY = 16;

		uint8 m_data[sizeof(CVariant) * CAPACITY];
	};

	template <size_t CAPACITY> class CCustomInPlaceStack : public CStack
	{
	public:

		inline CCustomInPlaceStack()
			: CStack(m_data, nullptr, CAPACITY)
		{}

	private:

		uint8 m_data[sizeof(CVariant) * CAPACITY];
	};
}
