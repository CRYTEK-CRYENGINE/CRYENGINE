// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <Logger.h>
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
// Define global objects.
CCVars g_cvars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplFmod : public CryAudio::IImplModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(CryAudio::IImplModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioImplFmod, "EngineModule_AudioImpl", "aa6a039a-0ce5-bbab-33e0-aad69f3136f4"_cry_guid);

	CEngineModule_CryAudioImplFmod();

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()  const override     { return "CryAudioImplFmod"; }
	virtual char const* GetCategory()  const override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplFmod::OnEvent, nullptr, ESystemEvents::ImplSet);
		SRequestUserData const data(ERequestFlags::ExecuteBlocking | ERequestFlags::CallbackOnExternalOrCallingThread);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Fmod::CImpl");
		gEnv->pAudioSystem->SetImpl(new CImpl, data);
		gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplFmod::OnEvent, nullptr);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		if (m_bSuccess)
		{
			Cry::Audio::Log(ELogType::Always, "CryAudioImplFmod loaded");
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "CryAudioImplFmod failed to load");
		}
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnEvent(SRequestInfo const* const pRequestInfo)
	{
		m_bSuccess = pRequestInfo->requestResult == ERequestResult::Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplFmod)
bool CEngineModule_CryAudioImplFmod::m_bSuccess = false;

CEngineModule_CryAudioImplFmod::CEngineModule_CryAudioImplFmod()
{
	g_cvars.RegisterVariables();
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
#include <CryCore/CrtDebugStats.h>
