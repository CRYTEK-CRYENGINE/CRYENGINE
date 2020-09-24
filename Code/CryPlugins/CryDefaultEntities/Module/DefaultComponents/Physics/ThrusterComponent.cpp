#include "StdAfx.h"
#include "ThrusterComponent.h"
#include <CryPhysics/physinterface.h>
#include <CryRenderer/IRenderAuxGeom.h>

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
		pPhysicalEntity->Action(&actionImpulse, 1);
	}
}

void CThrusterComponent::Initialize()
{
	m_bConstantThrustActive = m_bEnableConstantThrustByDefault;
	RequestPoststep();
}

void CThrusterComponent::RequestPoststep()
{
	if (m_pEntity->GetPhysicalEntity())
	{
		pe_params_flags pf;
		pf.flagsOR = pef_monitor_poststep;
		m_pEntity->GetPhysicalEntity()->SetParams(&pf);
	}
}

void CThrusterComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			m_bConstantThrustActive = m_bEnableConstantThrustByDefault;
			m_pEntity->UpdateComponentEventMask(this);
		case ENTITY_EVENT_PHYSICAL_TYPE_CHANGED:
			RequestPoststep();
			break;
		case ENTITY_EVENT_PHYS_POSTSTEP:
			ApplySingleThrust(m_constantThrust * event.fParam[0]);
			break;
	}
}

Cry::Entity::EventFlags CThrusterComponent::GetEventMask() const
{
	Cry::Entity::EventFlags bitFlags = ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED | ENTITY_EVENT_PHYSICAL_TYPE_CHANGED;
	if (m_bConstantThrustActive)
	{
		bitFlags |= ENTITY_EVENT_PHYS_POSTSTEP;
	}

	return bitFlags;
}

#ifndef RELEASE
void CThrusterComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext& context) const
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
