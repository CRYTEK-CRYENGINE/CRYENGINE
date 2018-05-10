// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryAudio/IListener.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
class CListenerComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual void   OnShutDown() override;
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual void   OnTransformChanged() override;
	// ~IEntityComponent

public:

	CListenerComponent() = default;

	inline static void ReflectType(Schematyc::CTypeDesc<CListenerComponent>& desc)
	{
		desc.SetGUID("BAE91D5C-8CFB-40B2-8397-F5A9EEDB9AC4"_cry_guid);
		desc.SetEditorCategory("Audio");
		desc.SetLabel("Listener");
		desc.SetDescription("Audio Listener from which transformation the audio is recorded.");
		desc.SetIcon("icons:Audio/component_audio_listener.ico");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });
	}

	inline void SetActive(bool const bValue)
	{ 
		m_bActive = bValue;

		if (!m_bActive)
		{
			gEnv->pEntitySystem->GetAreaManager()->ExitAllAreas(GetEntityId());
		}
	}

private:

	CryAudio::IListener*            m_pIListener = nullptr;
	CryAudio::CObjectTransformation m_previousTransformation;
	bool                            m_bActive = true;
};
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
