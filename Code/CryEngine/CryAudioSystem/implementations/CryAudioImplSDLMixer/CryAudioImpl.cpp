// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
// Define global objects.
CCVars g_cvars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplSDLMixer : public IImplModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(IImplModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioImplSDLMixer, "EngineModule_AudioImpl", "8030c0d1-905b-4031-a378-5a8b53125f3f"_cry_guid)

	CEngineModule_CryAudioImplSDLMixer();

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName() const override     { return "CryAudioImplSDLMixer"; }
	virtual char const* GetCategory() const override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnEvent, nullptr, ESystemEvents::ImplSet);
		SRequestUserData const data(ERequestFlags::ExecuteBlocking | ERequestFlags::CallbackOnExternalOrCallingThread);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::SDL_mixer::CImpl");
		gEnv->pAudioSystem->SetImpl(new CImpl, data);
		gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnEvent, nullptr);

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		if (m_bSuccess)
		{
			Cry::Audio::Log(ELogType::Always, "CryAudioImplSDLMixer loaded");
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "CryAudioImplSDLMixer failed to load");
		}
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnEvent(SRequestInfo const* const pRequestInfo)
	{
		m_bSuccess = pRequestInfo->requestResult == ERequestResult::Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplSDLMixer)
bool CEngineModule_CryAudioImplSDLMixer::m_bSuccess = false;

CEngineModule_CryAudioImplSDLMixer::CEngineModule_CryAudioImplSDLMixer()
{
	g_cvars.RegisterVariables();
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
#include <CryCore/CrtDebugStats.h>
