// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioSystemImpl_sdlmixer.h"
#include "AudioSystemImplCVars_sdlmixer.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

using namespace CryAudio::Impl;

CSoundAllocator g_audioImplMemoryPool_sdlmixer;
CAudioLogger g_audioImplLogger_sdlmixer;
CAudioSystemImplCVars CryAudio::Impl::g_audioImplCVars_sdlmixer;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplSDLMixer : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioImplSDLMixer, "CryAudioImplSDLMixer", 0x8030c0d1905b4031, 0xa3785a8b53125f3f)

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() override { return "CryAudioImplSDLMixer"; }
	virtual const char* GetCategory() override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		// Initialize memory pools.
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SDL Mixer Audio Implementation Memory Pool Primary");
		size_t const nPoolSize = g_audioImplCVars_sdlmixer.m_nPrimaryPoolSize << 10;
		uint8* const pPoolMemory = new uint8[nPoolSize];
		g_audioImplMemoryPool_sdlmixer.InitMem(nPoolSize, pPoolMemory, "SDL Mixer Implementation Audio Pool");

		POOL_NEW_CREATE(CAudioSystemImpl_sdlmixer, pImpl);

		if (pImpl != nullptr)
		{
			g_audioImplLogger_sdlmixer.Log(eAudioLogType_Always, "CryAudioImplSDLMixer loaded");

			SAudioRequest oAudioRequestData;
			oAudioRequestData.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;

			SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> oAMData(pImpl);
			oAudioRequestData.pData = &oAMData;

			gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnAudioEvent, nullptr, eAudioRequestType_AudioManagerRequest, eAudioManagerRequestType_SetAudioImpl);
			env.pAudioSystem->PushRequest(oAudioRequestData);
			gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnAudioEvent, nullptr);
		}
		else
		{
			g_audioImplLogger_sdlmixer.Log(eAudioLogType_Always, "CryAudioImplSDLMixer failed to load");
		}

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnAudioEvent(SAudioRequestInfo const* const pAudioRequestInfo)
	{
		m_bSuccess = pAudioRequestInfo->requestResult == eAudioRequestResult_Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplSDLMixer)
bool CEngineModule_CryAudioImplSDLMixer::m_bSuccess = false;

CEngineModule_CryAudioImplSDLMixer::CEngineModule_CryAudioImplSDLMixer()
{
	g_audioImplCVars_sdlmixer.RegisterVariables();
}

CEngineModule_CryAudioImplSDLMixer::~CEngineModule_CryAudioImplSDLMixer()
{
}

#include <CryCore/CrtDebugStats.h>
