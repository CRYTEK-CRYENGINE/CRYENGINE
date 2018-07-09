// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/DynArray.h>
#include <CrySerialization/Forward.h>

namespace Schematyc
{

// Dynamic array class based on DynArray that implements copy on write.
template<typename TYPE> class CArray
{
private:

	typedef DynArray<TYPE, uint32> Array;
	typedef std::shared_ptr<Array> ArrayPtr;

public:

	inline CArray() {}

	inline CArray(const CArray& rhs)
		: m_pArray(rhs.m_pArray)
	{}

	inline uint32 Size() const
	{
		return m_pArray ? m_pArray->size() : 0;
	}

	inline void PushBack(const TYPE& value)
	{
		CopyOnWrite();
		m_pArray->push_back(value);
	}

	inline void EmplaceBack(TYPE&& value)
	{
		CopyOnWrite();
		m_pArray->emplace_back(std::forward<TYPE>(value));
	}

	inline void Insert(uint32 pos, TYPE&& value)
	{
		CopyOnWrite();
		m_pArray->insert(m_pArray->begin() + pos, std::forward<TYPE>(value));
	}

	inline void PopBack()
	{
		CRY_ASSERT(m_pArray);
		CopyOnWrite();
		m_pArray->pop_back();
	}

	inline TYPE& At(uint32 pos)
	{
		CRY_ASSERT(m_pArray);
		return (*m_pArray)[pos];
	}

	inline const TYPE& At(uint32 pos) const
	{
		CRY_ASSERT(m_pArray);
		return (*m_pArray)[pos];
	}

	friend inline bool Serialize(Serialization::IArchive& archive, CArray& value, const char* szName, const char* szLabel)
	{
		if (archive.isInput())
		{
			value.CopyOnWrite();
		}
		else if (value.m_pArray == nullptr)
		{
			Array emptyArray;
			return archive(emptyArray, szName, szLabel);
		}

		return archive(*value.m_pArray, szName, szLabel);
	}

	bool operator==(const CArray& other) const
	{
		if (Size() == other.Size())
		{
			for (uint32 i = 0, e = Size(); i < e; ++i)
			{
				if (At(i) != other.At(i))
					return false;
			}
			return true;
		}
		return false;
	}

private:

	inline void CopyOnWrite()
	{
		if (!m_pArray)
		{
			m_pArray = std::make_shared<Array>();
		}
		else if (!m_pArray.unique())
		{
			m_pArray = std::make_shared<Array>(*m_pArray);
		}
	}

private:

	ArrayPtr m_pArray;
};

} // Schematyc
