// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryLobby.h"
// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

class CEngineModule_CryLobby : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryLobby, "EngineModule_CryLobby", 0x2c5cc5ec41f7451c, 0xa785857ca7731c28)

	virtual const char* GetName() override { return "CryLobby"; }
	virtual const char* GetCategory() override { return "CryEngine"; }

	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		CCryLobby* pLobby = new CCryLobby;

		if (pLobby)
		{
			env.pLobby = pLobby;

			return true;
		}

		return false;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryLobby)

CEngineModule_CryLobby::CEngineModule_CryLobby()
{
}

CEngineModule_CryLobby::~CEngineModule_CryLobby()
{
}

#include <CryCore/CrtDebugStats.h>
