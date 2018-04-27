// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "SystemTypeRegistry.h"

#include <CrySystem/ISystem.h>

namespace Cry {
namespace Reflection {

template<typename REGISTRY_INTERFACE_TYPE>
bool CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE >::UseType(const ITypeDesc& typeDesc)
{
	const TypeIndex index = typeDesc.GetIndex();

	const ITypeDesc* pTypeDesc = GetReflectionRegistry().FindTypeByIndex(index);
	CRY_ASSERT_MESSAGE(pTypeDesc, "Type '%s' not found in global registry.", typeDesc.GetRawName());
	if (pTypeDesc)
	{
		m_typesByIndex.emplace_back(index);
		m_typeIndicesByGuid.emplace(pTypeDesc->GetGuid(), index);
		m_typeIndicesByTypeId.emplace(pTypeDesc->GetTypeId().GetValue(), index);
		return true;
	}

	return false;
}

template<typename REGISTRY_INTERFACE_TYPE>
TypeIndex::ValueType CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE >::GetTypeCount() const
{
	return m_typesByIndex.size();
}

template<typename REGISTRY_INTERFACE_TYPE>
const ITypeDesc* CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE >::FindTypeByIndex(TypeIndex index) const
{
	if (index < m_typesByIndex.size())
	{
		return GetReflectionRegistry().FindTypeByIndex(m_typesByIndex[index]);
	}
	return nullptr;
}

template<typename REGISTRY_INTERFACE_TYPE>
const ITypeDesc* CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE >::FindTypeByGuid(const CryGUID& guid) const
{
	auto result = m_typeIndicesByGuid.find(guid);
	if (result != m_typeIndicesByGuid.end())
	{
		const size_t index = result->second;
		CRY_ASSERT_MESSAGE(index < m_typesByIndex.size(), "Type Registry is corrupted.");
		if (index < m_typesByIndex.size())
		{
			return GetReflectionRegistry().FindTypeByIndex(index);
		}
	}
	return nullptr;
}

template<typename REGISTRY_INTERFACE_TYPE>
const ITypeDesc* CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE >::FindTypeById(CryTypeId typeId) const
{
	auto result = m_typeIndicesByTypeId.find(typeId);
	if (result != m_typeIndicesByTypeId.end())
	{
		const size_t index = result->second;
		CRY_ASSERT_MESSAGE(index < m_typesByIndex.size(), "Type Registry is corrupted.");
		if (index < m_typesByIndex.size())
		{
			return GetReflectionRegistry().FindTypeByIndex(index);
		}
	}
	return nullptr;
}

} // ~Reflection namespace
} // ~Cry namespace
