#include "StdAfx.h"
#include "ThrusterComponent.h"

namespace Cry
{
namespace DefaultComponents
{
void CThrusterComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CThrusterComponent::ApplySingleThrust, "{BAFA4C36-CFDC-4309-B5A1-C0DD7F7F6D15}"_cry_guid, "ApplySingleThrust");
		pFunction->SetDescription("Applies one impulse through the thruster");
		pFunction->BindInput(1, 'impl', "Impulse");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CThrusterComponent::EnableConstantThrust, "{987CF5A0-72C9-4501-859A-7EE85B1499E0}"_cry_guid, "EnableConstantThrust");
		pFunction->SetDescription("Enables automatically thrusting every frame");
		pFunction->BindInput(1, 'enab', "Enable");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CThrusterComponent::IsConstantThrustEnabled, "{5FEB48B3-0AFF-44CD-B408-127887D412F2}"_cry_guid, "IsConstantThrustEnabled");
		pFunction->SetDescription("Checks whether automatic thrust is on");
		pFunction->BindOutput(0, 'enab', "Enabled");
		componentScope.Register(pFunction);
	}
}

void CThrusterComponent::ApplySingleThrust(float thrust)
{
	if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
	{
		Matrix34 worldTransform = GetWorldTransformMatrix();

		pe_action_impulse actionImpulse;
		actionImpulse.impulse = worldTransform.GetColumn2() * thrust;
		actionImpulse.point = worldTransform.GetTranslation();
		pPhysicalEntity->Action(&actionImpulse);
	}
}

void CThrusterComponent::Initialize()
{
	m_bConstantThrustActive = m_bEnableConstantThrustByDefault;
}

void CThrusterComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		m_bConstantThrustActive = m_bEnableConstantThrustByDefault;

		m_pEntity->UpdateComponentEventMask(this);
	}
	else if (event.event == ENTITY_EVENT_PREPHYSICSUPDATE)
	{
		ApplySingleThrust(m_constantThrust * event.fParam[0]);
	}
}

uint64 CThrusterComponent::GetEventMask() const
{
	uint64 bitFlags = ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
	if (m_bConstantThrustActive)
	{
		bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE);
	}

	return bitFlags;
}

#ifndef RELEASE
void CThrusterComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		Matrix34 slotTransform = GetWorldTransformMatrix();

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(slotTransform.GetTranslation(), slotTransform.GetColumn2(), 0.1f, 0.1f, context.debugDrawInfo.color);
	}
}
#endif

void CThrusterComponent::EnableConstantThrust(bool bEnable)
{
	m_bConstantThrustActive = bEnable; 

	m_pEntity->UpdateComponentEventMask(this);
}
}
}
