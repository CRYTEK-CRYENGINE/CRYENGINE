// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioImplCVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

using namespace CryAudio::Impl::SDL_mixer;

// Define global objects.
CSoundAllocator g_audioImplMemoryPool;
CAudioLogger g_audioImplLogger;
CAudioImplCVars CryAudio::Impl::SDL_mixer::g_audioImplCVars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplSDLMixer : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioImplSDLMixer, "CryAudioImplSDLMixer", 0x8030c0d1905b4031, 0xa3785a8b53125f3f)

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()     { return "CryAudioImplSDLMixer"; }
	virtual char const* GetCategory() { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		// Initialize memory pools.
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SDL Mixer Audio Implementation Memory Pool Primary");
		size_t const poolSize = g_audioImplCVars.m_primaryMemoryPoolSize << 10;
		uint8* const pPoolMemory = new uint8[poolSize];
		g_audioImplMemoryPool.InitMem(poolSize, pPoolMemory, "SDL Mixer Implementation Audio Pool");

		POOL_NEW_CREATE(CAudioImpl, pImpl);

		if (pImpl != nullptr)
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplSDLMixer loaded");

			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;

			SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> requestData(pImpl);
			request.pData = &requestData;

			gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnAudioEvent, nullptr, eAudioRequestType_AudioManagerRequest, eAudioManagerRequestType_SetAudioImpl);
			env.pAudioSystem->PushRequest(request);
			gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplSDLMixer::OnAudioEvent, nullptr);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplSDLMixer failed to load");
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
	g_audioImplCVars.RegisterVariables();
}

CEngineModule_CryAudioImplSDLMixer::~CEngineModule_CryAudioImplSDLMixer()
{
}

#include <CryCore/CrtDebugStats.h>
