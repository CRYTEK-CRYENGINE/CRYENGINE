// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	#include <Logger.h>
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
// Define global objects.
CCVars g_cvars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplPortAudio : public IImplModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(IImplModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioImplPortAudio, "EngineModule_AudioImpl", "c0c05842-ff77-4a61-96de-43a684515dc8"_cry_guid);

	CEngineModule_CryAudioImplPortAudio();

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()  const override    { return "CryAudioImplPortAudio"; }
	virtual char const* GetCategory() const override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplPortAudio::OnEvent, nullptr, ESystemEvents::ImplSet);
		SRequestUserData const data(ERequestFlags::ExecuteBlocking | ERequestFlags::CallbackOnExternalOrCallingThread);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CImpl");
		gEnv->pAudioSystem->SetImpl(new CImpl, data);
		gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplPortAudio::OnEvent, nullptr);

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		if (m_bSuccess)
		{
			Cry::Audio::Log(ELogType::Always, "CryAudioImplPortAudio loaded");
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "CryAudioImplPortAudio failed to load");
		}
#endif        // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnEvent(SRequestInfo const* const pRequestInfo)
	{
		m_bSuccess = pRequestInfo->requestResult == ERequestResult::Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplPortAudio)
bool CEngineModule_CryAudioImplPortAudio::m_bSuccess = false;

CEngineModule_CryAudioImplPortAudio::CEngineModule_CryAudioImplPortAudio()
{
	g_cvars.RegisterVariables();
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
#include <CryCore/CrtDebugStats.h>
