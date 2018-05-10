// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StringPool.h"

#include <CryCore/CryCrc32.h>

namespace Schematyc2
{
//////////////////////////////////////////////////////////////////////////
CStringPool::CStringPool()
{
	m_strings.reserve(512);
}

//////////////////////////////////////////////////////////////////////////
CStringPool::~CStringPool()
{
	for (TStringVector::iterator iString = m_strings.begin(), iEndString = m_strings.end(); iString != iEndString; ++iString)
	{
		free(*iString);
	}
}

//////////////////////////////////////////////////////////////////////////
CPoolStringData* CStringPool::Insert(const char* data)
{
	if (data && data[0])
	{
		const uint32 crc32 = CCrc32::Compute(data);
		TStringVector::iterator iBeginString = m_strings.begin();
		TStringVector::iterator iEndString = m_strings.end();
		TStringVector::iterator iLowerBoundString = std::lower_bound(iBeginString, iEndString, crc32, StringSortPredicate());
		if ((iLowerBoundString == iEndString) || ((*iLowerBoundString)->CRC32() != crc32) || (strcmp((*iLowerBoundString)->c_str(), data) != 0))
		{
			const size_t length = strlen(data);
			CPoolStringData* pNewString = new(malloc(sizeof(CPoolStringData) + length))CPoolStringData(length, crc32, 1, data);
			m_strings.insert(iLowerBoundString, pNewString);
			return pNewString;
		}
		else
		{
			(*iLowerBoundString)->AddRef();
			return *iLowerBoundString;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CStringPool::Erase(CPoolStringData* pData)
{
	if (pData)
	{
		TStringVector::iterator iBeginString = m_strings.begin();
		TStringVector::iterator iEndString = m_strings.end();
		TStringVector::iterator iLowerBoundString = std::lower_bound(iBeginString, iEndString, pData->CRC32(), StringSortPredicate());
		if ((iLowerBoundString != iEndString) && (*iLowerBoundString == pData))
		{
			CPoolStringData* pString = *iLowerBoundString;
			m_strings.erase(iLowerBoundString);
			pString->~CPoolStringData();
			free(pString);
		}
	}
}
}
