// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "ReflectedTypeDesc.h"

#include "ReflectedFunctionDesc.h"
#include "ReflectedVariableDesc.h"

namespace Cry {
namespace Reflection {

CReflectedTypeDesc::CReflectedTypeDesc()
	: m_category(ETypeCategory::Unset)
{
}

CReflectedTypeDesc::CReflectedTypeDesc(const CryGUID& guid, const CryTypeDesc& typeDesc)
	: m_typeDesc(typeDesc)
	, m_guid(guid)
{
	m_category = ETypeCategory::Unset;
	if (typeDesc.IsAbstract())
	{
		m_category = ETypeCategory::Abstract;
	}
	else if (typeDesc.IsClass())
	{
		m_category = ETypeCategory::Class;
		m_classMembers.baseTypesByIndex = std::vector<CryTypeId>();
		m_classMembers.functionsByIndex = std::vector<CReflectedFunctionDesc*>();
		m_classMembers.variablesByIndex = std::vector<CReflectedVariableDesc*>();
	}
	else if (typeDesc.IsEnum())
	{
		m_category = ETypeCategory::Enum;
		m_enumMembers.valuesByIndex = std::vector<CEnumValueDesc*>();
	}
	else if (typeDesc.IsFundamental())
	{
		m_category = ETypeCategory::Fundamental;
	}
	else if (typeDesc.IsUnion())
	{
		m_category = ETypeCategory::Union;
	}
}

CReflectedTypeDesc::CReflectedTypeDesc(CReflectedTypeDesc&& rhs)
	: m_typeDesc(rhs.m_typeDesc)
	, m_guid(rhs.m_guid)
	, m_label(rhs.m_label)
	, m_description(rhs.m_description)
	, m_sourcePos(rhs.m_sourcePos)
	, m_category(rhs.m_category)
	, m_index(rhs.m_index)
{
	switch (m_category)
	{
	case ETypeCategory::Class:
		{
			m_classMembers.baseTypesByIndex.swap(rhs.m_classMembers.baseTypesByIndex);
			m_classMembers.functionsByIndex.swap(rhs.m_classMembers.functionsByIndex);
			m_classMembers.variablesByIndex.swap(rhs.m_classMembers.variablesByIndex);
		}
		break;
	case ETypeCategory::Enum:
		{
			m_enumMembers.valuesByIndex.swap(rhs.m_enumMembers.valuesByIndex);
		}
		break;
	default:
		break;
	}

	m_conversionsByTypeId.swap(rhs.m_conversionsByTypeId);
}

bool CReflectedTypeDesc::AddBaseType(CryTypeId typeId, const SSourceFileInfo& srcPos)
{
	CRY_ASSERT_MESSAGE(IsClass(), "Base can only be added to class types.");
	if (IsClass())
	{
		auto compare = [typeId](CryTypeId id) -> bool
		{
			return (id == typeId);
		};

		CRY_ASSERT_MESSAGE(std::find_if(m_classMembers.baseTypesByIndex.begin(), m_classMembers.baseTypesByIndex.end(), compare) == m_classMembers.baseTypesByIndex.end(), "Type already registered as base.");
		m_classMembers.baseTypesByIndex.emplace_back(typeId);
		return true;
	}
	return false;
}

const ITypeDesc* CReflectedTypeDesc::GetBaseTypeByIndex(size_t index) const
{
	if (IsClass() && index < m_classMembers.baseTypesByIndex.size())
	{
		return GetReflectionRegistry().FindTypeById(m_classMembers.baseTypesByIndex[index]);
	}
	return nullptr;
}

IFunctionDesc* CReflectedTypeDesc::AddFunction(const CMemberFunction& memberFunc, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcPos)
{
	CRY_ASSERT_MESSAGE(IsClass(), "Functions can only be added to class types.");
	if (IsClass())
	{
		auto compare = [&guid](const CReflectedFunctionDesc* pDesc) -> bool
		{
			return (pDesc->GetGuid() == guid);
		};

		const bool isGuidUnused = (std::find_if(m_classMembers.functionsByIndex.begin(), m_classMembers.functionsByIndex.end(), compare) == m_classMembers.functionsByIndex.end());
		CRY_ASSERT_MESSAGE(isGuidUnused, "Function guid already used.");
		if (isGuidUnused)
		{
			CReflectedFunctionDesc* pFunctionDesc = new CReflectedFunctionDesc(this, memberFunc.GetReturnTypeId(), memberFunc.GetFunction(), memberFunc.GetParams(), szLabel, guid);
			m_classMembers.functionsByIndex.emplace_back(pFunctionDesc);
			return pFunctionDesc;
		}
	}
	return nullptr;
}

const IFunctionDesc* CReflectedTypeDesc::GetFunctionByIndex(size_t index) const
{
	if (IsClass() && index < m_classMembers.functionsByIndex.size())
	{
		return m_classMembers.functionsByIndex[index];
	}
	return nullptr;
}

IVariableDesc* CReflectedTypeDesc::AddVariable(CryTypeId typeId, ptrdiff_t offset, const char* szLabel, const CryGUID& guid, const SSourceFileInfo& srcPos)
{
	CRY_ASSERT_MESSAGE(IsClass(), "Variables can only be added to class types.");
	if (IsClass())
	{
		// TODO: Decide if we should always check.
		auto compare = [offset](const CReflectedVariableDesc* pDesc) -> bool
		{
			return (pDesc->GetOffset() == offset);
		};
		CRY_ASSERT_MESSAGE(std::find_if(m_classMembers.variablesByIndex.begin(), m_classMembers.variablesByIndex.end(), compare) == m_classMembers.variablesByIndex.end(), "Variable already registered.");

		CReflectedVariableDesc* pVariableDesc = new CReflectedVariableDesc(offset, szLabel, *this, typeId, guid, srcPos);

		m_classMembers.variablesByIndex.emplace_back(pVariableDesc);
		return m_classMembers.variablesByIndex.back();
	}
	return nullptr;
}

const IVariableDesc* CReflectedTypeDesc::GetVariableByIndex(size_t index) const
{
	if (IsClass() && index < m_classMembers.variablesByIndex.size())
	{
		return m_classMembers.variablesByIndex[index];
	}
	return nullptr;
}

IEnumValueDesc* CReflectedTypeDesc::AddEnumValue(const char* szLabel, size_t value, const char* szDescription)
{
	CRY_ASSERT_MESSAGE(IsEnum(), "Enum values can only be added to enum types.");
	if (IsEnum())
	{
		// TODO: Decide if we should always check.
		CEnumValueDesc enumNameValue(szLabel, value, "");
		auto compare = [&enumNameValue](const CEnumValueDesc* pDesc) -> bool
		{
			return (*pDesc == enumNameValue);
		};
		CRY_ASSERT_MESSAGE(std::find_if(m_enumMembers.valuesByIndex.begin(), m_enumMembers.valuesByIndex.end(), compare) == m_enumMembers.valuesByIndex.end(), "Name '%s' with value '%d' already exists.", szLabel, value);
		// ~TODO

		CEnumValueDesc* pEnumDesc = new CEnumValueDesc(szLabel, value, szDescription ? szDescription : "");

		m_enumMembers.valuesByIndex.emplace_back(pEnumDesc);
		return pEnumDesc;
	}
	return nullptr;
}

const IEnumValueDesc* CReflectedTypeDesc::GetEnumValueByIndex(size_t index) const
{
	if (IsEnum() && index < m_enumMembers.valuesByIndex.size())
	{
		return m_enumMembers.valuesByIndex[index];
	}
	return nullptr;
}

bool CReflectedTypeDesc::AddConversionOperator(CryTypeId typeId, Type::CConversionOperator conversionOperator)
{
	const bool isNotRegistered = (m_conversionsByTypeId.find(typeId) == m_conversionsByTypeId.end());
	CRY_ASSERT_MESSAGE(isNotRegistered, "Conversion already registered.");
	if (isNotRegistered)
	{
		m_conversionsByTypeId.emplace(typeId, conversionOperator);
		return true;
	}
	return false;
}

Type::CConversionOperator CReflectedTypeDesc::GetConversionOperator(CryTypeId typeId)
{
	auto result = m_conversionsByTypeId.find(typeId);
	if (result != m_conversionsByTypeId.end())
	{
		return result->second;
	}
	return Type::CConversionOperator();
}

} // ~Reflection namespace
} // ~Cry namespace
