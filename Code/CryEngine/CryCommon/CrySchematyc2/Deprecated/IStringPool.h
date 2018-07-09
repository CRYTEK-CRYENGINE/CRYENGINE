// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/IFramework.h"

namespace Schematyc2
{
	class CPoolStringData;

	// String pool interface.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	struct IStringPool
	{
		virtual ~IStringPool() {}

		virtual CPoolStringData* Insert(const char* szString) = 0;
		virtual void Erase(CPoolStringData* pData) = 0;
	};

	// String pool data.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CPoolStringData
	{
	public:

		inline CPoolStringData(size_t length, uint32 crc32, int32 refCount, const char* szString)
			: m_length(length)
			, m_crc32(crc32)
			, m_refCount(refCount)
		{
			memcpy(m_string, szString, length + 1);
		}

		inline size_t Length() const
		{
			return m_length;
		}

		inline bool Empty() const
		{
			return !m_length;
		}

		inline uint32 CRC32() const
		{
			return m_crc32;
		}

		inline const char* c_str() const
		{
			return m_string;
		}

		inline void AddRef()
		{
			++ m_refCount;
		}

		inline void RemoveRef()
		{
			-- m_refCount;
			if(m_refCount <= 0)
			{
				gEnv->pSchematyc2->GetStringPool().Erase(this);
			}
		}

	private:

		size_t m_length;
		uint32 m_crc32;
		int32  m_refCount;
		char   m_string[1];
	};

	// Pool string.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CPoolString
	{
	public:

		inline CPoolString()
			: m_pData(nullptr)
		{}

		inline CPoolString(const char* szString)
			: m_pData(Insert(szString))
		{}

		inline CPoolString(const CPoolString& rhs)
			: m_pData(AddRef(rhs.m_pData))
		{}
		
		inline ~CPoolString()
		{
			RemoveRef(m_pData);
		}

		inline size_t Length() const
		{
			return m_pData ? m_pData->Length() : 0;
		}

		inline bool Empty() const
		{
			return m_pData ? m_pData->Empty() : true;
		}

		inline uint32 CRC32() const
		{
			return m_pData ? m_pData->CRC32() : 0;
		}

		inline const char* c_str() const
		{
			return m_pData ? m_pData->c_str() : "";
		}

		inline CPoolString& operator = (const char* szString)
		{
			RemoveRef(m_pData);
			m_pData = Insert(szString);
			return *this;
		}

		inline CPoolString& operator = (const CPoolString& rhs)
		{
			RemoveRef(m_pData);
			m_pData = AddRef(rhs.m_pData);
			return *this;
		}

		inline bool operator == (const CPoolString& rhs) const
		{
			return m_pData == rhs.m_pData;
		}

		inline bool operator != (const CPoolString& rhs) const
		{
			return m_pData != rhs.m_pData;
		}

	private:

		inline CPoolStringData* Insert(const char* szString) const
		{
			return szString && szString[0] ? gEnv->pSchematyc2->GetStringPool().Insert(szString) : nullptr;
		}

		inline CPoolStringData* AddRef(CPoolStringData* pData) const
		{
			if(pData)
			{
				pData->AddRef();
			}
			return pData;
		}

		inline void RemoveRef(CPoolStringData* pData) const
		{
			if(pData)
			{
				pData->RemoveRef();
			}
		}

		CPoolStringData* m_pData;
	};

	inline bool Serialize(Serialization::IArchive& archive, CPoolString& value, const char* szName, const char* szLabel)
	{
		if(archive.isInput())
		{
			string temp;
			archive(temp, szName, szLabel);
			value = temp.c_str();
		}
		else if(archive.isOutput())
		{
			string temp = value.c_str();
			archive(temp, szName, szLabel);
		}
		return true;
	}
}
