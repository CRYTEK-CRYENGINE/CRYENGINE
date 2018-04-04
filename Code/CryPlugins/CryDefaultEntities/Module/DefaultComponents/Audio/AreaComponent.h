// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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
static constexpr float FadeValueEpsilon = 0.025f;

class CAreaComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual void   OnShutDown() override;
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(SEntityEvent& event) override;
	// ~IEntityComponent

public:

	CAreaComponent() = default;

	struct SNewFadeValueSignal
	{
		SNewFadeValueSignal() = default;
		SNewFadeValueSignal(float const value_)
			: value(value_)
		{}

		float value = 0.0f;
	};

	struct SOnFarToNearSignal
	{
		SOnFarToNearSignal() = default;
	};

	struct SOnNearToInsideSignal
	{
		SOnNearToInsideSignal() = default;
	};

	struct SOnInsideToNearSignal
	{
		SOnInsideToNearSignal() = default;
	};

	struct SOnNearToFarSignal
	{
		SOnNearToFarSignal() = default;
	};

	static void ReflectType(Schematyc::CTypeDesc<CAreaComponent>& desc)
	{
		desc.SetGUID("E57B86BA-9F4D-4EDA-94D2-1E20E4CAE94F"_cry_guid);
		desc.SetEditorCategory("Audio");
		desc.SetLabel("Area");
		desc.SetDescription("Area component which delivers positional data in regards to entities interacting with the area this component is attached to.");
		desc.SetIcon("icons:Audio/component_area.ico");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

		desc.AddMember(&CAreaComponent::m_fadeDistance, 'fade', "fadeDistance", "FadeDistance", "The distance over which is faded.", 0.0f);
	}

	void SetActive(bool const bValue);

private:

	enum class EState
	{
		None,
		Near,
		Inside,
	};

	EState m_state = EState::None;
	EState m_previousState = EState::None;
	bool   m_bActive = true;
	float  m_fadeDistance = 0.0f;
	float  m_fadeValue = 0.0f;
};
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
