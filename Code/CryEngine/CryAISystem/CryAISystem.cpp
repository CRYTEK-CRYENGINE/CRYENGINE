// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryAISystem.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"
#include "CryAISystem.h"
#include <CryCore/Platform/platform_impl.inl>
#include "CAISystem.h"
#include "AILog.h"
#include <CrySystem/ISystem.h>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

CAISystem* g_pAISystem;

/*
   //////////////////////////////////////////////////////////////////////////
   // Pointer to Global ISystem.
   static ISystem* gISystem = 0;
   ISystem* GetISystem()
   {
   return gISystem;
   }
 */

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAISystem : public IAIEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IAIEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAISystem, "EngineModule_CryAISystem", "6b8e79a7-8400-4f44-97db-7614428ad251"_cry_guid)

	virtual ~CEngineModule_CryAISystem()
	{
		CryUnregisterFlowNodes();
		SAFE_RELEASE(gEnv->pAISystem);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName()  const override { return "CryAISystem"; };
	virtual const char* GetCategory()  const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		AIInitLog(pSystem);

		g_pAISystem = new CAISystem(pSystem);
		env.pAISystem = g_pAISystem;

		CryRegisterFlowNodes();

		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAISystem)

//////////////////////////////////////////////////////////////////////////
#include <CryCore/CrtDebugStats.h>

#ifndef _LIB
	#include <CryCore/Common_TypeInfo.h>
#endif

#include <CryCore/TypeInfo_impl.h>
#include "AutoTypeStructs_info.h"
