// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IReflection.h"
#include "CryExtension/CryGUID.h"

#include <unordered_map>
#include <vector>

namespace Cry {
namespace Reflection {

template<typename REGISTRY_INTERFACE_TYPE = ISystemTypeRegistry>
class CSystemTypeRegistry : public REGISTRY_INTERFACE_TYPE
{
public:
	CSystemTypeRegistry(const char* szLabel, const CryTypeDesc& typeDesc, const CryGUID& guid);

	virtual ~CSystemTypeRegistry() {}

	// ICustomTypeRegistry
	virtual bool                 UseType(const ITypeDesc& typeDesc) override;

	virtual const CryGUID&       GetGuid() const override final     { return m_guid; }
	virtual const char*          GetLabel() const override final    { return m_label.c_str(); }
	virtual const CryTypeDesc&   GetTypeDesc() const override final { return m_typeDesc; }
	virtual CryTypeId            GetTypeId() const override final   { return m_typeDesc.GetTypeId(); }

	virtual TypeIndex::ValueType GetTypeCount() const override;
	virtual const ITypeDesc*     FindTypeByIndex(TypeIndex index) const override;
	virtual const ITypeDesc*     FindTypeByGuid(const CryGUID& guid) const override;
	virtual const ITypeDesc*     FindTypeById(CryTypeId typeId) const override;
	// ~ICustomTypeRegistry

protected:
	std::vector<TypeIndex>                              m_typesByIndex;
	std::unordered_map<CryGUID, TypeIndex>              m_typeIndicesByGuid;
	std::unordered_map<CryTypeId::ValueType, TypeIndex> m_typeIndicesByTypeId;

private:
	const string      m_label;
	const CryGUID     m_guid;
	const CryTypeDesc m_typeDesc;
};

template<typename REGISTRY_INTERFACE_TYPE>
CSystemTypeRegistry<REGISTRY_INTERFACE_TYPE>::CSystemTypeRegistry(const char* szLabel, const CryTypeDesc& typeDesc, const CryGUID& guid)
	: m_label(szLabel)
	, m_typeDesc(typeDesc)
	, m_guid(guid)
{
	GetReflectionRegistry().RegisterSystemRegistry(this);
}

} // ~Reflection namespace
} // ~Cry namespace
