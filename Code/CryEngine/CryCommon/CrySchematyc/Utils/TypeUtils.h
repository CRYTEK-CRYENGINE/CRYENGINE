// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <type_traits>

#include <CryExtension/TypeList.h>
#include <CrySerialization/IArchive.h>
#include <CryString/StringUtils.h>

#include "CrySchematyc/Utils/PreprocessorUtils.h"

namespace Schematyc
{
namespace HasOperator
{

template<typename TYPE, typename = void>
struct SEquals : std::false_type {};

template<typename TYPE>
struct SEquals<
  TYPE,
  decltype(void(std::declval<const TYPE&>() == std::declval<const TYPE&>()))
  > : std::true_type {};

} // HasOperator

// Test to determine whether types in first list can be converted to respective types in second.
////////////////////////////////////////////////////////////////////////////////////////////////////

template<class LHS, class RHS> struct STypeListConversion;

template<> struct STypeListConversion<TL::NullType, TL::NullType>
{
	static const bool value = true;
};

template<class HEAD, class TAIL> struct STypeListConversion<TL::Typelist<HEAD, TAIL>, TL::NullType>
{
	static const bool value = false;
};

template<class HEAD, class TAIL> struct STypeListConversion<TL::NullType, TL::Typelist<HEAD, TAIL>>
{
	static const bool value = false;
};

template<class LHS_HEAD, class LHS_TAIL, class RHS_HEAD, class RHS_TAIL> struct STypeListConversion<TL::Typelist<LHS_HEAD, LHS_TAIL>, TL::Typelist<RHS_HEAD, RHS_TAIL>>
{
	static const bool value = std::is_convertible<LHS_HEAD, RHS_HEAD>::value && STypeListConversion<LHS_TAIL, RHS_TAIL>::value;
};

// Get function name.
////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> const char* GetFunctionName();

namespace Private
{
template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> struct SExtractFunctionName
{
	inline string operator()() const
	{
		string functionName;
		const char* szFunctionName = SCHEMATYC_FUNCTION_NAME;
		const char* szPrefix = "Schematyc::Private::SExtractFunctionName<";
		const char* szStartOfFunctionSignature = strstr(szFunctionName, szPrefix);
		if (szStartOfFunctionSignature)
		{
			static const char* szSuffix = ">::operator";
			const char* szEndOfFunctionName = strstr(szStartOfFunctionSignature + strlen(szPrefix), szSuffix);
			if (szEndOfFunctionName)
			{
				int32 scope = 0;
				while (true)
				{
					--szEndOfFunctionName;
					if (*szEndOfFunctionName == ')')
					{
						++scope;
					}
					else if (*szEndOfFunctionName == '(')
					{
						--scope;
						if (scope == 0)
						{
							break;
						}
					}
				}
				const char* szStartOfFunctionName = szEndOfFunctionName - 1;
				while (*(szStartOfFunctionName - 1) != ' ')
				{
					--szStartOfFunctionName;
				}
				functionName.assign(szStartOfFunctionName, 0, static_cast<string::size_type>(szEndOfFunctionName - szStartOfFunctionName));
			}
		}
		CRY_ASSERT_MESSAGE(!functionName.empty(), "Failed to extract function name!");
		return functionName;
	}
};

} // Private

template<typename FUNCTION_PTR_TYPE, FUNCTION_PTR_TYPE FUNCTION_PTR> inline const char* GetFunctionName()
{
	static const string functionName = Private::SExtractFunctionName<FUNCTION_PTR_TYPE, FUNCTION_PTR>()();
	return functionName.c_str();
}

// Get type name.
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Private
{
template<typename TYPE> struct SExtractTypeName
{
	inline string operator()() const
	{
		string typeName;
		const char* szFunctionName = SCHEMATYC_FUNCTION_NAME;
		const char* szPrefix = "Schematyc::Private::SExtractTypeName<";
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
				typeName.assign(szStartOfTypeName, 0, static_cast<string::size_type>(szEndOfTypeName - szStartOfTypeName));
			}
		}
		CRY_ASSERT_MESSAGE(!typeName.empty(), "Failed to extract type name!");
		return typeName;
	}
};

} // Private
} // Schematyc
