// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Declare specialized cast functions for shared pointers?

#pragma once

#include "CrySchematyc/Reflection/TypeDesc.h"

namespace Schematyc
{

template<typename TYPE> bool ToString(IString& output, const TYPE& input) // #SchematycTODO : Move/remove?
{
	const STypeOperators& typeOperators = GetTypeDesc<TYPE>().GetOperators();
	if (typeOperators.toString)
	{
		(*typeOperators.toString)(output, &input);
		return true;
	}
	return false;
}

namespace Helpers
{

inline void* DynamicCast(const CCommonTypeDesc& fromTypeDesc, void* pFrom, const CCommonTypeDesc& toTypeDesc)
{
	CRY_ASSERT(pFrom);
	if (fromTypeDesc == toTypeDesc)
	{
		return pFrom;
	}
	else
	{
		const ETypeCategory fromTypeCategory = fromTypeDesc.GetCategory();
		if ((fromTypeCategory == ETypeCategory::Class) || (fromTypeCategory == ETypeCategory::CustomClass))
		{
			for (const CClassBaseDesc& base : static_cast<const CClassDesc&>(fromTypeDesc).GetBases())
			{
				void* pCastValue = DynamicCast(base.GetTypeDesc(), pFrom, toTypeDesc);
				if (pCastValue)
				{
					return static_cast<uint8*>(pCastValue) + base.GetOffset();
				}
			}
		}
	}
	return nullptr;
}

inline const void* DynamicCast(const CCommonTypeDesc& fromTypeDesc, const void* pFrom, const CCommonTypeDesc& toTypeDesc)
{
	CRY_ASSERT(pFrom);
	if (fromTypeDesc == toTypeDesc)
	{
		return pFrom;
	}
	else
	{
		const ETypeCategory fromTypeCategory = fromTypeDesc.GetCategory();
		if ((fromTypeCategory == ETypeCategory::Class) || (fromTypeCategory == ETypeCategory::CustomClass))
		{
			for (const CClassBaseDesc& base : static_cast<const CClassDesc&>(fromTypeDesc).GetBases())
			{
				const void* pCastValue = DynamicCast(base.GetTypeDesc(), pFrom, toTypeDesc);
				if (pCastValue)
				{
					return static_cast<const uint8*>(pCastValue) + base.GetOffset();
				}
			}
		}
	}
	return nullptr;
}

template<typename TO_TYPE> TO_TYPE* DynamicCast(const CCommonTypeDesc& fromTypeDesc, void* pFrom)
{
	return static_cast<TO_TYPE*>(DynamicCast(fromTypeDesc, pFrom, GetTypeDesc<TO_TYPE>()));
}

template<typename TO_TYPE> const TO_TYPE* DynamicCast(const CCommonTypeDesc& fromTypeDesc, const void* pFrom)
{
	return static_cast<const TO_TYPE*>(DynamicCast(fromTypeDesc, pFrom, GetTypeDesc<TO_TYPE>()));
}

} // Helpers

template<typename TO_TYPE, typename FROM_TYPE> TO_TYPE& DynamicCast(FROM_TYPE& from)
{
	void* pResult = Helpers::DynamicCast(GetTypeDesc<FROM_TYPE>(), &from, GetTypeDesc<TO_TYPE>());
	CRY_ASSERT(pResult);
	return *static_cast<TO_TYPE*>(pResult);
}

template<typename TO_TYPE, typename FROM_TYPE> const TO_TYPE* DynamicCast(const FROM_TYPE& from)
{
	const void* pResult = Helpers::DynamicCast(GetTypeDesc<FROM_TYPE>(), &from, GetTypeDesc<TO_TYPE>());
	CRY_ASSERT(pResult);
	return *static_cast<const TO_TYPE*>(pResult);
}

template<typename TO_TYPE, typename FROM_TYPE> TO_TYPE* DynamicCast(FROM_TYPE* pFrom)
{
	return pFrom ? static_cast<TO_TYPE*>(Helpers::DynamicCast(GetTypeDesc<FROM_TYPE>(), pFrom, GetTypeDesc<TO_TYPE>())) : nullptr;
}

template<typename TO_TYPE, typename FROM_TYPE> const TO_TYPE* DynamicCast(const FROM_TYPE* pFrom)
{
	return pFrom ? static_cast<const TO_TYPE*>(Helpers::DynamicCast(GetTypeDesc<FROM_TYPE>(), pFrom, GetTypeDesc<TO_TYPE>())) : nullptr;
}

} // Schematyc
