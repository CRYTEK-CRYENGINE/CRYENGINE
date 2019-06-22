// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryAudio/IListener.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntityComponent.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
class CDefaultListenerComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void                    Initialize() override;
	virtual void                    OnShutDown() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void                    ProcessEvent(const SEntityEvent& event) override;
	virtual void                    OnTransformChanged() override;
	// ~IEntityComponent

public:

	CDefaultListenerComponent() = default;

	inline static void ReflectType(Schematyc::CTypeDesc<CDefaultListenerComponent>& desc)
	{
		desc.SetGUID("56B7CEF6-1725-440D-9794-8C31819BB0D2"_cry_guid);
		desc.SetEditorCategory("Audio");
		desc.SetLabel("DefaultListener");
		desc.SetDescription("Audio Default Listener from which transformation the audio is recorded.");
		desc.SetIcon("icons:Audio/component_audio_listener.ico");
		desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector, IEntityComponent::EFlags::HiddenFromUser });
	}

	inline void SetActive(bool const bValue)
	{
		m_isActive = bValue;

		if (!m_isActive)
		{
			gEnv->pEntitySystem->GetAreaManager()->ExitAllAreas(GetEntityId());
		}
	}

private:

	CryAudio::IListener*      m_pIListener = nullptr;
	CryAudio::CTransformation m_previousTransformation;
	bool                      m_isActive = true;
};
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
