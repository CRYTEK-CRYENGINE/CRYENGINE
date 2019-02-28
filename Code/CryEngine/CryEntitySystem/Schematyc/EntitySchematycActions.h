// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : These actions are for test purposes only and should either be removed or polished up and moved to separate files.

#include <CryRenderer/IRenderAuxGeom.h>

#include  <CrySchematyc/Utils/SharedString.h>
#include  <CrySchematyc/Services/ITimerSystem.h>
#include  <CrySchematyc/Services/IUpdateScheduler.h>
#include  <CrySchematyc/Action.h>
#include  <CrySchematyc/Env/Elements/EnvAction.h>
#include  <CrySchematyc/Env/Elements/EnvSignal.h>

#include  <CrySchematyc/Env/IEnvRegistrar.h>

namespace Schematyc
{

bool Serialize(Serialization::IArchive& archive, STimerDuration& value, const char* szName, const char* szLabel) // #SchematycTODO : Move to ITimerSystem.h?
{
	// #SchematycTODO : Validate!!!
	const ETimerUnits prevUnits = value.units;
	archive(value.units, "units", "Units");
	switch (value.units)
	{
	case ETimerUnits::Frames:
		{
			if (value.units != prevUnits)
			{
				value = STimerDuration(uint32(1));
			}
			archive(value.frames, "frames", "Duration");
			break;
		}
	case ETimerUnits::Seconds:
		{
			if (value.units != prevUnits)
			{
				value = STimerDuration(1.0f);
			}
			archive(value.seconds, "seconds", "Duration");
			break;
		}
	case ETimerUnits::Random:
		{
			if (value.units != prevUnits)
			{
				value = STimerDuration(1.0f, 1.0f);
			}
			archive(value.range.min, "min", "Minimum");
			archive(value.range.max, "max", "Maximum");
			break;
		}
	}
	return true;
}

class CEntityTimerAction final : public CAction
{
public:

	struct STickSignal
	{
		static void ReflectType(CTypeDesc<STickSignal>& desc)
		{
			desc.SetGUID("57f19e5f-23be-40a5-aff9-98042f6a63f8"_cry_guid);
			desc.SetLabel("Tick");
		}
	};

public:

	// CAction

	virtual bool Init() override
	{
		return true;
	}

	virtual void Start() override
	{
		STimerParams timerParams(m_duration);
		if (m_bAutoStart)
		{
			timerParams.flags.Add(ETimerFlags::AutoStart);
		}
		if (m_bRepeat)
		{
			timerParams.flags.Add(ETimerFlags::Repeat);
		}

		m_timerId = gEnv->pSchematyc->GetTimerSystem().CreateTimer(timerParams, SCHEMATYC_MEMBER_DELEGATE(&CEntityTimerAction::OnTimer, *this));
	}

	virtual void Stop() override
	{
		if (m_timerId != TimerId::Invalid)
		{
			gEnv->pSchematyc->GetTimerSystem().DestroyTimer(m_timerId);
			m_timerId = TimerId::Invalid;
		}
	}

	virtual void Shutdown() override {}

	// ~CAction

	static void ReflectType(CTypeDesc<CEntityTimerAction>& desc)
	{
		desc.SetGUID("6937eddc-f25c-44dc-a759-501d2e5da0df"_cry_guid);
		desc.SetIcon("icons:schematyc/entity_timer_action.png");
		desc.AddMember(&CEntityTimerAction::m_duration, 'dur', "duration", "Duration", "Timer duration", STimerDuration(0.0f));
		desc.AddMember(&CEntityTimerAction::m_bAutoStart, 'auto', "bAutoStart", "AutoStart", "Start timer automatically", true);
		desc.AddMember(&CEntityTimerAction::m_bRepeat, 'rep', "bRepeat", "Repeat", "Repeat timer", false);
	}

	static void Register(IEnvRegistrar& registrar)
	{
		CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			CEnvRegistrationScope actionScope = scope.Register(SCHEMATYC_MAKE_ENV_ACTION(CEntityTimerAction));
			{
				actionScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(STickSignal));
			}
		}
	}

private:

	void OnTimer()
	{
		CAction::OutputSignal(STickSignal());
		if (!m_bRepeat)
		{
			CAction::GetObject().StopAction(*this);
		}
	}

private:

	STimerDuration m_duration = STimerDuration(0.0f);
	bool           m_bAutoStart = true;
	bool           m_bRepeat = false;

	TimerId        m_timerId = TimerId::Invalid;
};

class CEntityDebugTextAction final : public CAction
{
public:

	// CAction

	virtual bool Init() override
	{
		return true;
	}

	virtual void Start() override
	{
		gEnv->pSchematyc->GetUpdateScheduler().Connect(SUpdateParams(SCHEMATYC_MEMBER_DELEGATE(&CEntityDebugTextAction::Update, *this), m_connectionScope));
	}

	virtual void Stop() override
	{
		m_connectionScope.Release();
	}

	virtual void Shutdown() override {}

	// ~CAction

	static void ReflectType(CTypeDesc<CEntityDebugTextAction>& desc)
	{
		desc.SetGUID("8ea4441b-e080-4cca-8b3e-973e017404d3"_cry_guid);
		desc.SetIcon("icons:schematyc/entity_debug_text_action.png");
		desc.AddMember(&CEntityDebugTextAction::m_text, 'txt', "text", "Text", "Text to display", CSharedString());
		desc.AddMember(&CEntityDebugTextAction::m_pos, 'pos', "pos", "Pos", "Text position", Vec2(ZERO));
		desc.AddMember(&CEntityDebugTextAction::m_color, 'col', "color", "Color", "Text color", Col_White);
	}

	static void Register(IEnvRegistrar& registrar)
	{
		CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			scope.Register(SCHEMATYC_MAKE_ENV_ACTION(CEntityDebugTextAction));
		}
	}

private:

	void Update(const SUpdateContext& updateContext)
	{
		IRenderAuxText::Draw2dLabel(m_pos.x, m_pos.y, 2.0f, m_color, false, "%s", m_text.c_str());
	}

private:

	CSharedString    m_text;
	Vec2             m_pos = Vec2(ZERO);
	ColorF           m_color = Col_White;

	CConnectionScope m_connectionScope;
};

} // Schematyc
