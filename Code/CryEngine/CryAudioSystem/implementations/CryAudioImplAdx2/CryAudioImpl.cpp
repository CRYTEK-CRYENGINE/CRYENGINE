// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "CVars.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include <Logger.h>
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

#if CRY_PLATFORM_DURANGO
	#include <apu.h>
	#include <shapexmacontext.h>
#endif // CRY_PLATFORM_DURANGO

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
// Define global objects.
CCVars g_cvars;
//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplAdx2 : public CryAudio::IImplModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(CryAudio::IImplModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioImplAdx2, "EngineModule_AudioImpl", "F9CF2408-0217-4048-9369-EE322EFDD7D4"_cry_guid);

	CEngineModule_CryAudioImplAdx2();

	//////////////////////////////////////////////////////////////////////////
	virtual char const* GetName()  const override     { return "CryAudioImplAdx2"; }
	virtual char const* GetCategory()  const override { return "CryAudio"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		gEnv->pAudioSystem->AddRequestListener(&CEngineModule_CryAudioImplAdx2::OnEvent, nullptr, ESystemEvents::ImplSet);
		SRequestUserData const data(ERequestFlags::ExecuteBlocking | ERequestFlags::CallbackOnExternalOrCallingThread);

		MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::Adx2::CImpl");
		gEnv->pAudioSystem->SetImpl(new CImpl, data);
		gEnv->pAudioSystem->RemoveRequestListener(&CEngineModule_CryAudioImplAdx2::OnEvent, nullptr);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		if (m_bSuccess)
		{
			Cry::Audio::Log(ELogType::Always, "CryAudioImplAdx2 loaded");
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "CryAudioImplAdx2 failed to load");
		}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

		return m_bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnEvent(SRequestInfo const* const pRequestInfo)
	{
		m_bSuccess = pRequestInfo->requestResult == ERequestResult::Success;
	}

	static bool m_bSuccess;
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplAdx2)
bool CEngineModule_CryAudioImplAdx2::m_bSuccess = false;

CEngineModule_CryAudioImplAdx2::CEngineModule_CryAudioImplAdx2()
{
	g_cvars.RegisterVariables();
}

#if CRY_COMPILER_MSVC && CRY_COMPILER_VERSION >= 1900
// We need this to link the CRI library when using compiler VC14 or newer.
auto* g_ignore = static_cast<int (*)(char*, size_t, const char*, va_list)>(&vsprintf_s);
#endif
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio

#include <CryCore/CrtDebugStats.h>
