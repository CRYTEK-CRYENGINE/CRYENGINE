// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Shared parameters type information.

   -------------------------------------------------------------------------
   History:
   - 15:07:2010: Created by Paul Slinger

*************************************************************************/

#ifndef __SHAREDPARAMSTYPEINFO_H__
#define __SHAREDPARAMSTYPEINFO_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryString/StringUtils.h>

#ifdef _DEBUG
	#define DEBUG_SHARED_PARAMS_TYPE_INFO 1
#else
	#define DEBUG_SHARED_PARAMS_TYPE_INFO 0
#endif //_DEBUG

////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared parameters type information class.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSharedParamsTypeInfo
{
public:

	CSharedParamsTypeInfo(size_t size, const char* pName, const char* pFileName, uint32 line) : m_size(size)
	{
		if (pName)
		{
			const size_t length = strlen(pName);
			size_t pos = 0;

			if (length > sizeof(m_name) - 1)
			{
				pos = length - (sizeof(m_name) - 1);
			}

			cry_strcpy(m_name, pName + pos);
		}
		else
		{
			m_name[0] = '\0';
		}

		if (pFileName)
		{
			const size_t length = strlen(pFileName);
			size_t pos = 0;

			if (length > sizeof(m_fileName) - 1)
			{
				pos = length - (sizeof(m_fileName) - 1);
			}

			cry_strcpy(m_fileName, pFileName + pos);
		}
		else
		{
			m_fileName[0] = '\0';
		}

		m_line = line;

		CryFixedStringT<256> temp;

		temp.Format("%" PRISIZE_T "%s%s%u", size, m_name, m_fileName, m_line);

		m_uniqueId = CryStringUtils::CalculateHash(temp.c_str());
	}

	inline size_t GetSize() const
	{
		return m_size;
	}

	inline const char* GetName() const
	{
		return m_name;
	}

	inline const char* GetFileName() const
	{
		return m_fileName;
	}

	inline uint32 GetLine() const
	{
		return m_line;
	}

	inline uint32 GetUniqueId() const
	{
		return m_uniqueId;
	}

	inline bool operator==(const CSharedParamsTypeInfo& right) const
	{
#if DEBUG_SHARED_PARAMS_TYPE_INFO
		if (m_uniqueId == right.m_uniqueId)
		{
			CRY_ASSERT(m_size == right.m_size);

			CRY_ASSERT(!strcmp(m_name, right.m_name));

			CRY_ASSERT(!strcmp(m_fileName, right.m_fileName));

			CRY_ASSERT(m_line == right.m_line);
		}
#endif //DEBUG_SHARED_PARAMS_TYPE_INFO

		return m_uniqueId == right.m_uniqueId;
	}

	inline bool operator!=(const CSharedParamsTypeInfo& right) const
	{
#if DEBUG_SHARED_PARAMS_TYPE_INFO
		if (m_uniqueId == right.m_uniqueId)
		{
			CRY_ASSERT(m_size == right.m_size);

			CRY_ASSERT(!strcmp(m_name, right.m_name));

			CRY_ASSERT(!strcmp(m_fileName, right.m_fileName));

			CRY_ASSERT(m_line == right.m_line);
		}
#endif //DEBUG_SHARED_PARAMS_TYPE_INFO

		return m_uniqueId != right.m_uniqueId;
	}

private:

	size_t m_size;

	char   m_name[64];
	char   m_fileName[64];

	uint32 m_line;
	uint32 m_uniqueId;
};

#undef DEBUG_SHARED_PARAMS_TYPE_INFO

#endif //__SHAREDPARAMSTYPEINFO_H__
