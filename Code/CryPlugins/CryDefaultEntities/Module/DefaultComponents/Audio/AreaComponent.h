// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	enum class ETriggeringObjects
	{
		None,
		AudioListeners,
		Entities,
	};

	static void ReflectType(Schematyc::CTypeDesc<CAreaComponent>& desc);
	void        SetTriggeringObjects(ETriggeringObjects const triggeringObjects);
	void        SetActive(bool const bValue);

protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual void   OnShutDown() override {}
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	// Properties exposed to UI
	float m_fadeDistance = 0.0f;

private:

	enum class EState
	{
		None,
		Near,
		Inside,
	};

	EState             m_state = EState::None;
	EState             m_previousState = EState::None;
	ETriggeringObjects m_triggeringObjects = ETriggeringObjects::None;
	float              m_fadeValue = 0.0f;
	bool               m_bActive = true;
};
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
