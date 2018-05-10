// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryMemory/BucketAllocatorImpl.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#include "ResponseSystem.h"
#include "VariableCollection.h"

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryDynamicResponseSystem : public DRS::IDynamicResponseSystemEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(DRS::IDynamicResponseSystemEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryDynamicResponseSystem, "EngineModule_CryDynamicResponseSystem", "d1ed34dd-a44c-4c17-959a-46df79af5db3"_cry_guid)

	virtual ~CEngineModule_CryDynamicResponseSystem() override
	{
		SAFE_DELETE(gEnv->pDynamicResponseSystem);
	}

	virtual const char* GetName() const override { return "CryDynamicResponseSystem"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		CryDRS::CResponseSystem* pResponseSystem = new CryDRS::CResponseSystem();
		env.pDynamicResponseSystem = pResponseSystem;

		pResponseSystem->CreateVariableCollection(CryDRS::CVariableCollection::s_globalCollectionName);

		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryDynamicResponseSystem)

#include <CryCore/CrtDebugStats.h>
