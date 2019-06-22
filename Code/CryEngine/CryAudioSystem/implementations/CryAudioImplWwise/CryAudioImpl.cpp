// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

#if CRY_PLATFORM_DURANGO
	#include <apu.h>
	#include <shapexmacontext.h>
#endif // CRY_PLATFORM_DURANGO

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
// Define global objects.
CCVars g_cvars;

#if defined(CRY_AUDIO_IMPL_WWISE_PROVIDE_SECONDARY_POOL)
MemoryPoolReferenced g_audioImplMemoryPoolSecondary;
#endif // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplWwise : public CryAudio::IImplModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(CryAudio::IImplModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioImplWwise, "EngineModule_AudioImpl", "b4971e5d-d024-42c5-b34a-9ac0b4abfffd"_cry_guid)

	CEngineModule_CryAudioImplWwise();

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override     { return "CryAudioImplWwise"; }
	virtual const char* GetCategory() const override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
#if defined(CRY_AUDIO_IMPL_WWISE_PROVIDE_SECONDARY_POOL)
		size_t secondarySize = 0;
		void* pSecondaryMemory = nullptr;

	#if CRY_PLATFORM_DURANGO
		secondarySize = g_cvars.m_secondaryMemoryPoolSize << 10;

		APU_ADDRESS temp;
		HRESULT const result = ApuAlloc(&pSecondaryMemory, &temp, secondarySize, SHAPE_XMA_INPUT_BUFFER_ALIGNMENT);
		CRY_ASSERT(result == S_OK);
	#endif  // CRY_PLATFORM_DURANGO

		g_audioImplMemoryPoolSecondary.InitMem(secondarySize, (uint8*)pSecondaryMemory);
#endif    // PROVIDE_AUDIO_IMPL_SECONDARY_POOL

		gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplWwise::OnAudioEvent, nullptr, ESystemEvents::ImplSet);
		SRequestUserData const data(ERequestFlags::ExecuteBlocking | ERequestFlags::CallbackOnExternalOrCallingThread);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Wwise::CImpl");
		gEnv->pAudioSystem->SetImpl(new CImpl, data);
		gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplWwise::OnAudioEvent, nullptr);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
		if (m_bSuccess)
		{
			Cry::Audio::Log(ELogType::Always, "CryAudioImplWwise loaded");
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "CryAudioImplWwise failed to load");
		}
#endif    // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnAudioEvent(SRequestInfo const* const pAudioRequestInfo)
	{
		m_bSuccess = pAudioRequestInfo->requestResult == ERequestResult::Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplWwise)
bool CEngineModule_CryAudioImplWwise::m_bSuccess = false;

CEngineModule_CryAudioImplWwise::CEngineModule_CryAudioImplWwise()
{
	g_cvars.RegisterVariables();
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
#include <CryCore/CrtDebugStats.h>
