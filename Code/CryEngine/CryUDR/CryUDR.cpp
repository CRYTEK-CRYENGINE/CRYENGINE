// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>
#include "UDRSystem.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{
		class CEngineModule_CryUDR final : public IUDREngineModule
		{
			CRYINTERFACE_BEGIN()
				CRYINTERFACE_ADD(IDefaultModule)
				CRYINTERFACE_ADD(IUDREngineModule)
			CRYINTERFACE_END()

			CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryUDR, "EngineModule_CryUDR", "52eb3317-1abe-412a-9173-5d8d8277c3e4"_cry_guid)

			// Cry::IDefaultModule
			virtual const char* GetName() const override     { return "CryUDR"; }
			virtual const char* GetCategory() const override { return "CryEngine"; }
			virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
			{
				env.pUDR = &CUDRSystem::GetInstance();
				return true;
			}
			// ~Cry::IDefaultModule
		};
	}
}
CRYREGISTER_SINGLETON_CLASS(Cry::UDR::CEngineModule_CryUDR)

#include <CryCore/CrtDebugStats.h>
