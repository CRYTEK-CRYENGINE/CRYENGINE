// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PreloadComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CPreloadComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPreloadComponent::Load, "A482E1A3-03C6-47D4-B775-568A4DB60352"_cry_guid, "Load");
		pFunction->SetDescription("Loads the preload request.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPreloadComponent::Unload, "1FF19168-A2EF-46E5-A673-E34C7E46EEFF"_cry_guid, "Unload");
		pFunction->SetDescription("Unloads the preload request.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreloadComponent::OnShutDown()
{
	if (m_bLoaded)
	{
		gEnv->pAudioSystem->UnloadSingleRequest(m_preload.m_id);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreloadComponent::ReflectType(Schematyc::CTypeDesc<CPreloadComponent>& desc)
{
	desc.SetGUID("C098DC62-BB9F-4EC5-80EB-79B99EE29BAB"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Preload");
	desc.SetDescription("Used to load/unload a preload request.");
	desc.SetIcon("icons:Audio/component_preload.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CPreloadComponent::m_preload, 'prel', "preload", "Preload", "The preload request to load/unload.", SPreloadSerializeHelper());
}

//////////////////////////////////////////////////////////////////////////
void CPreloadComponent::Load()
{
	if (m_preload.m_id != CryAudio::InvalidPreloadRequestId)
	{
		m_bLoaded = true;
		gEnv->pAudioSystem->PreloadSingleRequest(m_preload.m_id, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreloadComponent::Unload()
{
	if (m_preload.m_id != CryAudio::InvalidPreloadRequestId)
	{
		m_bLoaded = false;
		gEnv->pAudioSystem->UnloadSingleRequest(m_preload.m_id);
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
