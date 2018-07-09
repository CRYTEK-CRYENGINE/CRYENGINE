// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef SCHEMATYC_USE_STD_TYPE_ID
	#define SCHEMATYC_USE_STD_TYPE_ID 0
#endif

#if SCHEMATYC_USE_STD_TYPE_ID
	#include <typeinfo>
	#include <typeindex>
#else
	#include "CrySchematyc/Utils/StringHashWrapper.h"
	#include "CrySchematyc/Utils/TypeUtils.h"
#endif

#include "CrySchematyc/Utils/PreprocessorUtils.h"

namespace Schematyc
{
class CTypeName
{
	template<typename TYPE> friend const CTypeName& GetTypeName();

private:

	struct EmptyType {};

#if SCHEMATYC_USE_STD_TYPE_ID
	typedef std::type_index                                                                   Value;
#else
	typedef CStringHashWrapper<CFastStringHash, CEmptyStringConversion, CRawPtrStringStorage> Value;
#endif

public:

	inline CTypeName()
#if SCHEMATYC_USE_STD_TYPE_ID
		: m_value(typeid(EmptyType))
#endif
	{}

	inline CTypeName(const CTypeName& rhs)
		: m_value(rhs.m_value)
	{}

	inline size_t GetHash() const
	{
#if SCHEMATYC_USE_STD_TYPE_ID
		return m_value.hash_code();
#else
		return static_cast<size_t>(m_value.GetHash().GetValue());
#endif
	}

	inline const char* c_str() const
	{
#if SCHEMATYC_USE_STD_TYPE_ID
		return m_value != typeid(EmptyType) ? m_value.name() : "";
#else
		return m_value.c_str();
#endif
	}

	inline bool IsEmpty() const
	{
#if SCHEMATYC_USE_STD_TYPE_ID
		return m_value == typeid(EmptyType);
#else
		return m_value.IsEmpty();
#endif
	}

	inline CTypeName& operator=(const CTypeName& rhs)
	{
		m_value = rhs.m_value;
		return *this;
	}

	inline bool operator==(const CTypeName& rhs) const
	{
		return m_value == rhs.m_value;
	}

	inline bool operator!=(const CTypeName& rhs) const
	{
		return m_value != rhs.m_value;
	}

	inline bool operator<(const CTypeName& rhs) const
	{
		return m_value < rhs.m_value;
	}

	inline bool operator<=(const CTypeName& rhs) const
	{
		return m_value <= rhs.m_value;
	}

	inline bool operator>(const CTypeName& rhs) const
	{
		return m_value > rhs.m_value;
	}

	inline bool operator>=(const CTypeName& rhs) const
	{
		return m_value >= rhs.m_value;
	}

	explicit inline CTypeName(const Value& value)
		: m_value(value)
	{}

private:

	Value m_value;
};

namespace Private
{
template<typename TYPE> struct STypeNameExtractor
{
	inline const char* operator()(string& storage) const
	{
		static const char* szResult = "";
		if (!szResult[0])
		{
			const char* szFunctionName = SCHEMATYC_FUNCTION_NAME;
			const char* szPrefix = "Schematyc::Private::STypeNameExtractor<";
			const char* szStartOfTypeName = strstr(szFunctionName, szPrefix);
			if (szStartOfTypeName)
			{
				szStartOfTypeName += strlen(szPrefix);
				const char* keyWords[] = { "struct ", "class ", "enum " };
				for (uint32 iKeyWord = 0; iKeyWord < CRY_ARRAY_COUNT(keyWords); ++iKeyWord)
				{
					const char* keyWord = keyWords[iKeyWord];
					const uint32 keyWordLength = strlen(keyWord);
					if (strncmp(szStartOfTypeName, keyWord, keyWordLength) == 0)
					{
						szStartOfTypeName += keyWordLength;
						break;
					}
				}
				static const char* szSffix = ">::operator";
				const char* szEndOfTypeName = strstr(szStartOfTypeName, szSffix);
				if (szEndOfTypeName)
				{
					while (*(szEndOfTypeName - 1) == ' ')
					{
						--szEndOfTypeName;
					}
					storage.assign(szStartOfTypeName, 0, static_cast<string::size_type>(szEndOfTypeName - szStartOfTypeName));
				}
			}
			CRY_ASSERT_MESSAGE(!storage.empty(), "Failed to extract type name!");
		}
		return storage.c_str();
	}
};
} // Private

template<typename TYPE> inline const CTypeName& GetTypeName()
{
	typedef typename std::remove_const<TYPE>::type NonConstType;

#if SCHEMATYC_USE_STD_TYPE_ID
	static const CTypeName typeName(typeid(NonConstType));
#else
	static const string storage = Private::SExtractTypeName<NonConstType>()();
	static const CTypeName typeName(storage.c_str());
#endif
	return typeName;
}
} // Schematyc

// Facilitate use of type id class as key in std::unordered containers.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace std
{
template<> struct hash<Schematyc::CTypeName>
{
	inline uint32 operator()(const Schematyc::CTypeName& rhs) const
	{
		return static_cast<uint32>(rhs.GetHash());
	}
};
} // std
