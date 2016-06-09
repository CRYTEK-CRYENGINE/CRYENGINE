// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioImplCVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

using namespace CryAudio::Impl::PortAudio;

// Define global objects.
CSoundAllocator g_audioImplMemoryPool;
CAudioLogger g_audioImplLogger;
CAudioImplCVars CryAudio::Impl::PortAudio::g_audioImplCVars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplPortAudio : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule);
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioImplPortAudio, "CryAudioImplPortAudio", 0xaa6a039a0ce5bbab, 0x33e0aad69f3136f4);

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()     { return "CryAudioImplPortAudio"; }
	virtual char const* GetCategory() { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		// Initialize memory pools.
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "PortAudio Implementation Memory Pool Primary");
		size_t const poolSize = g_audioImplCVars.m_primaryMemoryPoolSize << 10;
		uint8* const pPoolMemory = new uint8[poolSize];
		g_audioImplMemoryPool.InitMem(poolSize, pPoolMemory, "PortAudio Implementation Audio Pool");

		POOL_NEW_CREATE(CAudioImpl, pImpl);

		if (pImpl != nullptr)
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplPortAudio loaded");

			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking | eAudioRequestFlags_SyncCallback;

			SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> requestData(pImpl);
			request.pData = &requestData;

			gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplPortAudio::OnAudioEvent, nullptr, eAudioRequestType_AudioManagerRequest, eAudioManagerRequestType_SetAudioImpl);
			env.pAudioSystem->PushRequest(request);
			gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplPortAudio::OnAudioEvent, nullptr);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplPortAudio failed to load");
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

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplPortAudio)
bool CEngineModule_CryAudioImplPortAudio::m_bSuccess = false;

CEngineModule_CryAudioImplPortAudio::CEngineModule_CryAudioImplPortAudio()
{
	g_audioImplCVars.RegisterVariables();
}

CEngineModule_CryAudioImplPortAudio::~CEngineModule_CryAudioImplPortAudio()
{
}

#include <CryCore/CrtDebugStats.h>
