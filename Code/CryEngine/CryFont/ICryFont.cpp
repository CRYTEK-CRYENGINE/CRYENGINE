// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#include "CryFont.h"
#if defined(USE_NULLFONT)
	#include "NullFont.h"
#endif

///////////////////////////////////////////////
extern "C" ICryFont * CreateCryFontInterface(ISystem * pSystem)
{
	if (gEnv->IsDedicated())
	{
#if defined(USE_NULLFONT)
		return new CCryNullFont();
#else
		// The NULL font implementation must be present for all platforms
		// supporting running as a pure dedicated server.
		pSystem->GetILog()->LogError("Missing NULL font implementation for dedicated server");
		return NULL;
#endif
	}
	else
	{
#if defined(USE_NULLFONT) && defined(USE_NULLFONT_ALWAYS)
		return new CCryNullFont();
#else
		return new CCryFont(pSystem);
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryFont : public IFontEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IFontEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryFont, "EngineModule_CryFont", "6758643f-4321-4957-9b92-0d898d31f434"_cry_guid)

	virtual ~CEngineModule_CryFont()
	{
		SAFE_RELEASE(gEnv->pCryFont);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override { return "CryFont"; };
	virtual const char* GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;
		env.pCryFont = CreateCryFontInterface(pSystem);
		return env.pCryFont != 0;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryFont)