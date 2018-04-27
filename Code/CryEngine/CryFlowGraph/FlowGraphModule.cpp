// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryDB.cpp : Defines the exported functions for the DLL application.
//

#include "StdAfx.h"

// Included only once here
#include <CryCore//Platform/platform_impl.inl>

#include "FlowSystem/FlowSystem.h"
#include "FlowSystem/Modules/ModuleManager.h"
#include "GameTokens/GameTokenSystem.h"



// Cry::IDefaultModule -----------------------------------

class CEngineModule_FlowGraph : public IFlowSystemEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(IFlowSystemEngineModule)
		CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_FlowGraph, "EngineModule_FlowGraph", "8d22d250-cbf2-4dba-adcc-a656c06752d7"_cry_guid)

	CEngineModule_FlowGraph();
	~CEngineModule_FlowGraph();

	//////////////////////////////////////////////////////////////////////////
	virtual const char *GetName() const override { return "CryFlowGraph"; };
	virtual const char *GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize( SSystemGlobalEnvironment &env,const SSystemInitParams &initParams ) override
	{
		ISystem* pSystem = env.pSystem;
		
		CFlowSystem* pFlowSystem = new CFlowSystem();
		env.pFlowSystem = pFlowSystem;
		pFlowSystem->PreInit();
		pFlowSystem->Init();

		CryRegisterFlowNodes();
		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_FlowGraph)

CEngineModule_FlowGraph::CEngineModule_FlowGraph()
{
}

CEngineModule_FlowGraph::~CEngineModule_FlowGraph()
{
	if (gEnv->pFlowSystem)
	{
		gEnv->pFlowSystem->Release();
		gEnv->pFlowSystem = nullptr;
	}
}

