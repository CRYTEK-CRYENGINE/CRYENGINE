// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <limits>

#include <CryCore/Containers/CryArray.h>

class CChunkData
{
public:

	const char* GetData() const
	{
		return m_data.data();
	}

	int GetSize() const
	{
		return m_data.size();
	}

	template<class T>
	void Add(const T& object)
	{
		AddData(&object, sizeof(object));
	}

	void AddData(const void* pSrcData, int srcDataSize)
	{
		if (srcDataSize > 0)
		{
			const auto p = static_cast<const char*>(pSrcData);
			m_data.insert(m_data.end(), p, p + srcDataSize);
			assert(m_data.size() > 0 && m_data.size() <= (std::numeric_limits<int>::max)());
		}
	}

private:

	DynArray<char, int> m_data;
};
