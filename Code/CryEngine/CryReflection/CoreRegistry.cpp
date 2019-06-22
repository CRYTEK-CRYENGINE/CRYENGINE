// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CoreRegistry.h"
#include "TypeDesc.h"
#include "FunctionDesc.h"
#include "VariableDesc.h"

// TODO: This is a workaround for our CRY_IS_MONOLITHIC_BUILD which doesn't handle static variable very well.
//       For now we disable double registration asserts for those builds, because they do not (should not) influence working ability.
#if defined(CRY_IS_MONOLITHIC_BUILD)
#define CRY_DISABLE_REFLECTION_REGISTRATION_ASSERTS 1
#endif
// ~TODO

namespace Cry {
namespace Reflection {

CCoreRegistry::CCoreRegistry()
{
	m_typesByIndex.reserve(s_typeSlots);
}

ITypeDesc* CCoreRegistry::RegisterType(const Type::CTypeDesc& typeDesc, CGuid guid, ReflectTypeFunction pReflectFunc, const SSourceFileInfo& srcPos)
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
#if !defined (CRY_DISABLE_REFLECTION_REGISTRATION_ASSERTS)
#if defined(USE_CRY_ASSERT)
		const CTypeDesc& foundTypeDesc = m_typesByIndex[resultByGuid->second];
		const SSourceFileInfo& srcPos = foundTypeDesc.m_sourcePos;
#endif

		CRY_ASSERT_MESSAGE(resultByGuid == m_typeIndicesByGuid.end(),
		                   "Type registration rejected. Guid %s already used for type '%s', registered in file '%s', line '%d', function '%s'.",
		                   guid.ToDebugString(),
		                   foundTypeDesc.GetLabel(),
		                   srcPos.GetFile(),
		                   srcPos.GetLine(),
		                   srcPos.GetFunction());
#endif
		return nullptr;
	}

	auto resultByTypeId = m_typeIndicesByTypeId.find(typeDesc.GetTypeId().GetValue());
	if (resultByTypeId != m_typeIndicesByTypeId.end())
	{
#if !defined (CRY_DISABLE_REFLECTION_REGISTRATION_ASSERTS)
#if defined(USE_CRY_ASSERT)
		const CTypeDesc& foundTypeDesc = m_typesByIndex[resultByTypeId->second];
		const SSourceFileInfo& srcPos = foundTypeDesc.m_sourcePos;
#endif

		CRY_ASSERT_MESSAGE(resultByTypeId == m_typeIndicesByTypeId.end(),
		                   "Type registration rejected. Type '%s' is already registered with label '%s' {%s} in file '%s', line '%d', function '%s'.",
		                   foundTypeDesc.GetFullQualifiedName(),
		                   foundTypeDesc.GetLabel(),
		                   foundTypeDesc.GetGuid().ToDebugString(),
		                   srcPos.GetFile(),
		                   srcPos.GetLine(),
		                   srcPos.GetFunction());
#endif
		return nullptr;
	}

	const size_t index = m_typesByIndex.size();
	CRY_ASSERT_MESSAGE(index + 1 < TypeIndex::MaxValue, "Index of type exceeded max of TypeIndex.");
	if (index + 1 < TypeIndex::MaxValue)
	{
		m_typesByIndex.emplace_back(guid, typeDesc);

		CTypeDesc& reflectedTypeDesc = m_typesByIndex.back();
		reflectedTypeDesc.m_sourcePos = srcPos;
		reflectedTypeDesc.m_index = static_cast<TypeIndex::ValueType>(index);

		m_typeIndicesByGuid.emplace(reflectedTypeDesc.GetGuid(), index);
		m_typeIndicesByTypeId.emplace(reflectedTypeDesc.GetTypeId(), index);

		pReflectFunc(reflectedTypeDesc);

		for (CDescExtension* pExtension : reflectedTypeDesc.m_extensions)
		{
			pExtension->OnRegister(reflectedTypeDesc);
		}

		return &reflectedTypeDesc;
	}

	return nullptr;
}

TypeIndex::ValueType CCoreRegistry::GetTypeCount() const
{
	return static_cast<TypeIndex::ValueType>(m_typesByIndex.size());
}

const ITypeDesc* CCoreRegistry::GetTypeByIndex(TypeIndex index) const
{
	if (index.IsValid() && index < m_typesByIndex.size())
	{
		return &m_typesByIndex[index];
	}
	return nullptr;
}

const ITypeDesc* CCoreRegistry::GetTypeByGuid(CGuid guid) const
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

const ITypeDesc* CCoreRegistry::GetTypeById(CTypeId typeId) const
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

IFunctionDesc* CCoreRegistry::RegisterFunction(const Reflection::CFunction& function, CGuid guid, const char* szLabel, ReflectFunctionFunction pReflectFunc, const SSourceFileInfo& srcPos)
{
	CRY_ASSERT_MESSAGE(pReflectFunc, "Callback for ReflectFunctionFunction must be non-null!");
	if (pReflectFunc == nullptr)
	{
		return nullptr;
	}

	const IFunctionDesc* pFunctionDesc = GetFunctionByGuid(guid);
	if (pFunctionDesc)
	{
#if !defined (CRY_DISABLE_REFLECTION_REGISTRATION_ASSERTS)
#if defined(USE_CRY_ASSERT)
		const SSourceFileInfo& srcPos = pFunctionDesc->GetSourceInfo();
#endif

		CRY_ASSERT_MESSAGE(!pFunctionDesc,
			"Function registration rejected. Guid '{%s}' already used for function '%s', registered in file '%s', line '%d', function '%s'.",
			guid.ToDebugString(),
			pFunctionDesc->GetFullQualifiedName(),
			srcPos.GetFile(),
			srcPos.GetLine(),
			srcPos.GetFunction());
#endif
		return nullptr;
	}

	// TODO: For now we use label as function name. Maybe this needs to be changed later.
	pFunctionDesc = GetFunctionByName(szLabel);
	// ~TODO
	if (pFunctionDesc)
	{
#if !defined (CRY_DISABLE_REFLECTION_REGISTRATION_ASSERTS)
#if defined(USE_CRY_ASSERT)
		const SSourceFileInfo& srcPos = pFunctionDesc->GetSourceInfo();
#endif

		CRY_ASSERT_MESSAGE(!pFunctionDesc,
			"Function registration rejected. Function '%s' is already registered with label '%s' '{%s}' in file '%s', line '%d', function '%s'.",
			pFunctionDesc->GetFullQualifiedName(),
			pFunctionDesc->GetLabel(),
			pFunctionDesc->GetGuid().ToDebugString(),
			srcPos.GetFile(),
			srcPos.GetLine(),
			srcPos.GetFunction());
#endif
		return nullptr;
	}

	const size_t index = m_functionsByIndex.size();
	CRY_ASSERT_MESSAGE(index + 1 < FunctionIndex::MaxValue, "Index of function exceeded max of FunctionIndex.");
	if (index + 1 < FunctionIndex::MaxValue)
	{
		CFunctionDesc* pReflectedFunctionDesc = new CFunctionDesc(
			function.GetReturnTypeId(), function.GetFunctionDelegate(), function.GetParams(), function.GetFullQualifiedName(), szLabel, guid);
		pReflectedFunctionDesc->m_index = static_cast<FunctionIndex::ValueType>(index);
		pReflectedFunctionDesc->m_sourcePos = srcPos;

		m_functionsByIndex.emplace_back(pReflectedFunctionDesc);
		m_functionsByGuid.emplace_back(std::pair<CGuid, FunctionIndex>(pReflectedFunctionDesc->GetGuid(), index));
		// TODO: For now we use label as function name. Maybe this needs to be changed later.
		m_functionsByName.emplace_back(std::pair<uint32, FunctionIndex>(CCrc32::Compute(szLabel), index));
		// ~TODO
		pReflectFunc(*pReflectedFunctionDesc);

		return pReflectedFunctionDesc;
	}

	return nullptr;
}

TypeIndex::ValueType CCoreRegistry::GetFunctionCount() const
{
	return static_cast<TypeIndex::ValueType>(m_functionsByIndex.size());
}

const IFunctionDesc* CCoreRegistry::GetFunctionByIndex(TypeIndex index) const
{
	if (index.IsValid() && index < m_functionsByIndex.size())
	{
		return m_functionsByIndex[index];
	}
	return nullptr;
}

const IFunctionDesc* CCoreRegistry::GetFunctionByGuid(CGuid guid) const
{
	for (const auto& curPair : m_functionsByGuid)
	{
		if (curPair.first == guid)
		{
			return m_functionsByIndex[curPair.second];
		}
	}
	return nullptr;
}

const IFunctionDesc* CCoreRegistry::GetFunctionByName(const char* szName)
{
	if (szName)
	{
		const uint32_t nameHash = CCrc32::Compute(szName);
		for (const auto& curPair : m_functionsByName)
		{
			if (curPair.first == nameHash)
			{
				return m_functionsByIndex[curPair.second];
			}
		}
	}
	CRY_ASSERT_MESSAGE(szName, "Can't search for a function without a name.");
	return nullptr;
}

void CCoreRegistry::RegisterRegistry(IRegistry* pRegistry)
{
	auto condition = [pRegistry](IRegistry* pRHS)
	{
		return pRegistry->GetTypeId() == pRHS->GetTypeId();
	};

	auto result = std::find_if(m_customRegistriesByIndex.begin(), m_customRegistriesByIndex.end(), condition);
	CRY_ASSERT_MESSAGE(result == m_customRegistriesByIndex.end(), "Custom type registry '%s' is already registered", pRegistry->GetLabel());
	if (result == m_customRegistriesByIndex.end())
	{
		m_customRegistriesByIndex.emplace_back(pRegistry);
	}
}

size_t CCoreRegistry::GetRegistriesCount() const
{
	return m_customRegistriesByIndex.size();
}

IRegistry* CCoreRegistry::GetRegistryByIndex(size_t index) const
{
	if (index < m_customRegistriesByIndex.size())
	{
		return m_customRegistriesByIndex[index];
	}
	return nullptr;
}

IRegistry* CCoreRegistry::GetRegistryByGuid(CGuid guid) const
{
	for (IRegistry* pRegistry : m_customRegistriesByIndex)
	{
		if (pRegistry->GetGuid() == guid)
		{
			return pRegistry;
		}
	}
	return nullptr;
}

IRegistry* CCoreRegistry::GetRegistryById(CTypeId typeId) const
{
	for (IRegistry* pRegistry : m_customRegistriesByIndex)
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
