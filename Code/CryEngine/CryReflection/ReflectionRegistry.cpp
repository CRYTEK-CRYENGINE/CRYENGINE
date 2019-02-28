// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ReflectionRegistry.h"

#include <CryReflection/SystemTypeRegistry.h>

#include <CryCore/Platform/platform_impl.inl>

#include "ReflectedTypeDesc.h"
#include "ReflectedFunctionDesc.h"
#include "ReflectedVariableDesc.h"

namespace Cry {
namespace Reflection {

CRYREGISTER_SINGLETON_CLASS(CReflection);

CReflection* CReflection::s_pInstance = nullptr;

CReflection::CReflection()
{
	m_typesByIndex.reserve(100);
}

CReflection::~CReflection()
{

}

CReflection& CReflection::GetInstance()
{
	CRY_ASSERT_MESSAGE(s_pInstance, "Reflection not yet initialized.");
	return *s_pInstance;
}

bool CReflection::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	s_pInstance = this;
	env.pReflection = this;

	return true;
}

CReflectedTypeDesc* CReflection::Register(const CryTypeDesc& typeDesc, const CryGUID& guid, ReflectTypeFunction pReflectFunc, const SSourceFileInfo& srcPos)
{
	CRY_ASSERT_MESSAGE(pReflectFunc, "Callback for ReflectTypeFunction must be non-null!");
	if (pReflectFunc == nullptr)
	{
		return nullptr;
	}

	CRY_ASSERT_MESSAGE(typeDesc.IsArray() == false, "Only non-array types are allowed to be registered.");
	if (typeDesc.IsArray())
	{
		return nullptr;
	}

	auto resultByGuid = m_typeIndicesByGuid.find(guid);
	if (resultByGuid != m_typeIndicesByGuid.end())
	{
		const CReflectedTypeDesc& rtDesc = m_typesByIndex[resultByGuid->second];
		const SSourceFileInfo srcPos = rtDesc.m_sourcePos;

		CRY_ASSERT_MESSAGE(resultByGuid != m_typeIndicesByGuid.end(),
		                   "Type registration rejected. Guid %s already used for type '%s', registered in file '%s', line '%d', function '%s'.",
		                   guid.ToString().c_str(),
		                   rtDesc.GetLabel(),
		                   srcPos.GetFile(),
		                   srcPos.GetLine(),
		                   srcPos.GetFunction());

		return nullptr;
	}

	auto resultByTypeId = m_typeIndicesByTypeId.find(typeDesc.GetTypeId().GetValue());
	if (resultByTypeId != m_typeIndicesByTypeId.end())
	{
		const CReflectedTypeDesc& rtDesc = m_typesByIndex[resultByGuid->second];
		const SSourceFileInfo srcPos = rtDesc.m_sourcePos;

		CRY_ASSERT_MESSAGE(resultByTypeId != m_typeIndicesByTypeId.end(),
		                   "Type registration rejected. Type '%s' is already registered with label '%s' in file '%s', line '%d', function '%s'.",
		                   rtDesc.GetRawName(),
		                   rtDesc.GetLabel(),
		                   srcPos.GetFile(),
		                   srcPos.GetLine(),
		                   srcPos.GetFunction());

		return nullptr;
	}

	const size_t index = m_typesByIndex.size();
	CRY_ASSERT_MESSAGE(index + 1 < std::numeric_limits<TypeIndex::ValueType>::max(), "Index of type exceeded max of TypeIndex.");
	if (index + 1 < std::numeric_limits<TypeIndex::ValueType>::max())
	{
		m_typesByIndex.emplace_back(guid, typeDesc);

		CReflectedTypeDesc& typeDesc = m_typesByIndex.back();
		typeDesc.m_sourcePos = srcPos;
		typeDesc.m_index = static_cast<TypeIndex::ValueType>(index);

		m_typeIndicesByGuid.emplace(typeDesc.GetGuid(), index);
		m_typeIndicesByTypeId.emplace(typeDesc.GetTypeId(), index);

		pReflectFunc(typeDesc);

		return &typeDesc;
	}

	return nullptr;
}

TypeIndex::ValueType CReflection::GetTypeCount() const
{
	return m_typesByIndex.size();
}

const ITypeDesc* CReflection::FindTypeByIndex(TypeIndex index) const
{
	if (index.IsValid() && index < m_typesByIndex.size())
	{
		return &m_typesByIndex[index];
	}
	return nullptr;
}

const ITypeDesc* CReflection::FindTypeByGuid(CryGUID guid) const
{
	auto result = m_typeIndicesByGuid.find(guid);
	if (result != m_typeIndicesByGuid.end())
	{
		const size_t index = result->second;
		CRY_ASSERT_MESSAGE(index < m_typesByIndex.size(), "Type Registry is corrupted.");
		if (index < m_typesByIndex.size())
		{
			return &m_typesByIndex[index];
		}
	}
	return nullptr;
}

const ITypeDesc* CReflection::FindTypeById(CryTypeId typeId) const
{
	auto result = m_typeIndicesByTypeId.find(typeId);
	if (result != m_typeIndicesByTypeId.end())
	{
		const size_t index = result->second;
		CRY_ASSERT_MESSAGE(index < m_typesByIndex.size(), "Type Registry is corrupted.");
		if (index < m_typesByIndex.size())
		{
			return &m_typesByIndex[index];
		}
	}
	return nullptr;
}

void CReflection::RegisterSystemRegistry(ISystemTypeRegistry* pSystemRegistry)
{
	auto condition = [pSystemRegistry](ISystemTypeRegistry* pRHS)
	{
		return pSystemRegistry->GetTypeId() == pRHS->GetTypeId();
	};

	auto result = std::find_if(m_customRegistriesByIndex.begin(), m_customRegistriesByIndex.end(), condition);
	CRY_ASSERT_MESSAGE(result == m_customRegistriesByIndex.end(), "Custom type registry '%s' is already registered", pSystemRegistry->GetLabel());
	if (result == m_customRegistriesByIndex.end())
	{
		m_customRegistriesByIndex.emplace_back(pSystemRegistry);
	}
}

size_t CReflection::GetSystemRegistriesCount() const
{
	return m_customRegistriesByIndex.size();
}

ISystemTypeRegistry* CReflection::FindSystemRegistryByIndex(size_t index) const
{
	if (index < m_customRegistriesByIndex.size())
	{
		return m_customRegistriesByIndex[index];
	}
	return nullptr;
}

ISystemTypeRegistry* CReflection::FindSystemRegistryByGuid(const CryGUID& guid) const
{
	for (ISystemTypeRegistry* pRegistry : m_customRegistriesByIndex)
	{
		if (pRegistry->GetGuid() == guid)
		{
			return pRegistry;
		}
	}
	return nullptr;
}

ISystemTypeRegistry* CReflection::FindSystemRegistryById(CryTypeId typeId) const
{
	for (ISystemTypeRegistry* pRegistry : m_customRegistriesByIndex)
	{
		if (pRegistry->GetTypeId() == typeId)
		{
			return pRegistry;
		}
	}
	return nullptr;
}

} // ~Reflection namespace
} // ~Cry namespace
