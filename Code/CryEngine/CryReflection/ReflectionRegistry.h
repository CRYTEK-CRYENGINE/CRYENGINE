// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryReflection/IReflection.h>
#include <CryExtension/CryGUID.h>

#include <unordered_map>
#include <vector>

#include "ReflectedTypeDesc.h"

namespace Cry {
namespace Reflection {

class CReflection : public IReflection
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(Cry::Reflection::IReflection)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CReflection, "EngineModule_Reflection", "{4E615AA3-5E2D-4E34-883F-4119C9286FB5}"_cry_guid);

public:
	virtual ~CReflection();

	static CReflection& GetInstance();

	// Cry::IDefaultModule
	virtual const char* GetName() const override     { return "CryReflection"; }
	virtual const char* GetCategory() const override { return "Plugin"; }
	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~Cry::IDefaultModule

	// IReflectionRegistry
	virtual CReflectedTypeDesc*  Register(const CryTypeDesc& typeDesc, const CryGUID& guid, ReflectTypeFunction pReflectFunc, const SSourceFileInfo& srcPos) override;

	virtual TypeIndex::ValueType GetTypeCount() const override;
	virtual const ITypeDesc*     FindTypeByIndex(TypeIndex index) const override;
	virtual const ITypeDesc*     FindTypeByGuid(CryGUID guid) const override;
	virtual const ITypeDesc*     FindTypeById(CryTypeId typeId) const override;

	virtual size_t               GetSystemRegistriesCount() const override;
	virtual ISystemTypeRegistry* FindSystemRegistryByIndex(size_t index) const override;
	virtual ISystemTypeRegistry* FindSystemRegistryByGuid(const CryGUID& guid) const override;
	virtual ISystemTypeRegistry* FindSystemRegistryById(CryTypeId typeId) const override;

	virtual void                 RegisterSystemRegistry(ISystemTypeRegistry* pSystemRegistry) override;
	// ~IReflectionRegistry

private:
	CReflection();
	static CReflection* s_pInstance;

private:
	std::vector<CReflectedTypeDesc>                     m_typesByIndex;
	std::unordered_map<CryGUID, TypeIndex>              m_typeIndicesByGuid;
	std::unordered_map<CryTypeId::ValueType, TypeIndex> m_typeIndicesByTypeId;

	std::vector<ISystemTypeRegistry*>                   m_customRegistriesByIndex;
};

template<typename TYPE>
struct ReflectedTypeDesc
{
	static inline CReflectedTypeDesc* Of()
	{
		return gEnv->pReflection->FindTypeById(TypeIdOf<TYPE>());
	}
};

} // ~Cry namespace
} // ~Reflection namespace

template<typename TYPE>
using CryReflectedTypeDesc = Cry::Reflection::ReflectedTypeDesc<TYPE>;
