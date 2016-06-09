// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioImplCVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if CRY_PLATFORM_DURANGO
	#include <apu.h>
	#include <shapexmacontext.h>
#endif // CRY_PLATFORM_DURANGO

using namespace CryAudio::Impl::Fmod;

// Define global objects.
CSoundAllocator g_audioImplMemoryPool;
CAudioLogger g_audioImplLogger;
CAudioImplCVars CryAudio::Impl::Fmod::g_audioImplCVars;

#if defined(PROVIDE_FMOD_IMPL_SECONDARY_POOL)
tMemoryPoolReferenced g_audioImplMemoryPoolSecondary;
#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplFmod : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule);
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioImplFmod, "CryAudioImplFmod", 0xaa6a039a0ce5bbab, 0x33e0aad69f3136f4);

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()     { return "CryAudioImplFmod"; }
	virtual char const* GetCategory() { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		// Initialize memory pools.
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Fmod Audio Implementation Memory Pool Primary");
		size_t const poolSize = g_audioImplCVars.m_primaryMemoryPoolSize << 10;
		uint8* const pPoolMemory = new uint8[poolSize];
		g_audioImplMemoryPool.InitMem(poolSize, pPoolMemory, "Fmod Implementation Audio Pool");

#if defined(PROVIDE_FMOD_IMPL_SECONDARY_POOL)
		size_t secondarySize = 0;
		void* pSecondaryMemory = nullptr;

	#if CRY_PLATFORM_DURANGO
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Fmod Implementation Audio Pool Secondary");
		secondarySize = g_audioImplCVars.m_secondaryMemoryPoolSize << 10;

		APU_ADDRESS temp;
		HRESULT const result = ApuAlloc(&pSecondaryMemory, &temp, secondarySize, SHAPE_XMA_INPUT_BUFFER_ALIGNMENT);
		CRY_ASSERT(result == S_OK);
	#endif // CRY_PLATFORM_DURANGO

		g_audioImplMemoryPoolSecondary.InitMem(secondarySize, (uint8*)pSecondaryMemory);
#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

		POOL_NEW_CREATE(CAudioImpl, pImpl);

		if (pImpl != nullptr)
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplFmod loaded");

			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking | eAudioRequestFlags_SyncCallback;

			SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> requestData(pImpl);
			request.pData = &requestData;

			gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplFmod::OnAudioEvent, nullptr, eAudioRequestType_AudioManagerRequest, eAudioManagerRequestType_SetAudioImpl);
			env.pAudioSystem->PushRequest(request);
			gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplFmod::OnAudioEvent, nullptr);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Always, "CryAudioImplFmod failed to load");
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

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplFmod)
bool CEngineModule_CryAudioImplFmod::m_bSuccess = false;

CEngineModule_CryAudioImplFmod::CEngineModule_CryAudioImplFmod()
{
	g_audioImplCVars.RegisterVariables();
}

CEngineModule_CryAudioImplFmod::~CEngineModule_CryAudioImplFmod()
{
}

#include <CryCore/CrtDebugStats.h>
