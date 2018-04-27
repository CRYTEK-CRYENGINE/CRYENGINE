// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#ifdef INCLUDE_SCALEFORM_SDK
	#include "ScaleformHelper.h"

// Engine module wrapper for Scaleform Helper.
// Note: This can either be loaded from inside CrySystem (if compiled in directly) or loaded from a DLL (if pre-compiled version is shipped).
// The only responsibility of the module is to set gEnv->pScaleformHelper, through which all CRYENGINE specific Scaleform interaction runs.
	#include <CrySystem/IEngineModule.h>
	#include <CryExtension/ClassWeaver.h>

class CEngineModule_ScaleformHelper : public IScaleformHelperEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IScaleformHelperEngineModule)
	CRYINTERFACE_END()
	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_ScaleformHelper, "EngineModule_ScaleformHelper", "3d38f12a-521d-43cf-ca18-fd1fa7ea5020"_cry_guid)

	virtual ~CEngineModule_ScaleformHelper() {}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override { return "CryScaleformHelper"; };

	#if CRY_IS_SCALEFORM_HELPER
	virtual const char* GetCategory() const override { return "CryExtensions"; };
	#else
	virtual const char* GetCategory() const override { return "CryEngine"; };
	#endif

	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		if (env.pRenderer)
		{
			env.pScaleformHelper = new CScaleformHelper();	
		}

		return true;
	}
};
CRYREGISTER_SINGLETON_CLASS(CEngineModule_ScaleformHelper)
#endif

// If we are a stand-alone module, this will implement the module entry-points
#ifdef CRY_IS_SCALEFORM_HELPER
	#include <CryCore/Platform/platform_impl.inl>
#endif
