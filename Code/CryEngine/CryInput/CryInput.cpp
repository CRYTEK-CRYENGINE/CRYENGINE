// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#include "BaseInput.h"

#include "Synergy/SynergyContext.h"
#include "Synergy/SynergyKeyboard.h"
#include "Synergy/SynergyMouse.h"

#if CRY_PLATFORM_WINDOWS
	#ifndef _LIB
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
	#endif
#endif

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryInput : public IInputEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IInputEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryInput, "EngineModule_CryInput", "3cc05160-71bb-44f6-ae52-5949f30277f9"_cry_guid)

	virtual ~CEngineModule_CryInput() {}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override { return "CryInput"; };
	virtual const char* GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		IInput* pInput = 0;

		//Specific input systems only make sense in 'normal' mode when renderer is on
		if (gEnv->pRenderer)
		{
#if defined(USE_DXINPUT)
			pInput = new CDXInput(pSystem, (HWND) initParams.hWnd);
#elif defined(USE_DURANGOINPUT)
			pInput = new CDurangoInput(pSystem);
#elif defined(USE_LINUXINPUT)
			pInput = new CLinuxInput(pSystem);
#elif defined(USE_ORBIS_INPUT)
			pInput = new COrbisInput(pSystem);
#else
			pInput = new CBaseInput();
#endif
		}
		else
		{
			pInput = new CBaseInput();
		}

		if (!pInput->Init())
		{
			delete pInput;
			return false;
		}

#ifdef USE_SYNERGY_INPUT
		const char* pServer = g_pInputCVars->i_synergyServer->GetString();
		if (pServer && pServer[0] != '\0')
		{
			_smart_ptr<CSynergyContext> pContext = new CSynergyContext(g_pInputCVars->i_synergyScreenName->GetString(), pServer);

			if (!gEnv->pThreadManager->SpawnThread(pContext.get(), "Synergy"))
			{
				CryFatalError("Error spawning \"Synergy\" thread.");
			}

			pInput->AddInputDevice(new CSynergyKeyboard(*pInput, pContext));
			pInput->AddInputDevice(new CSynergyMouse(*pInput, pContext));
		}
#endif

		env.pInput = pInput;

		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryInput)

#include <CryCore/CrtDebugStats.h>
