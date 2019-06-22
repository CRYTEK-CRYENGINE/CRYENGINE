#include "StdAfx.h"
#include "ClothComponent.h"

#include <Cry3DEngine/IRenderNode.h>
#include <Cry3DEngine/ISurfaceType.h>

namespace Cry
{
namespace DefaultComponents
{

void CClothComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CClothComponent::AttachToEntity, "{170A7B3C-1BAB-4303-BD8E-503BA79DB62C}"_cry_guid, "AttachToEntity");
	pFunction->SetDescription("Adds a constraint, tying this component's physical entity to the specified entity");
	pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
	pFunction->BindInput(1, 'enti', "Target Entity", "Specifies the entity cloth should be attached to", Schematyc::ExplicitEntityId());
	pFunction->BindInput(2, 'part', "Entity Part Id", "Id of the part to be attached to (-1 to use main/default)", -1);
	componentScope.Register(pFunction);
}

CClothComponent::~CClothComponent()
{
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_NONE;
	m_pEntity->Physicalize(physParams);
}

void CClothComponent::Initialize()
{
	Physicalize();
}

void CClothComponent::Physicalize()
{
	int slot = GetOrMakeEntitySlotId();
	if (!m_pStatObj || m_pStatObj->IsDefaultObject() || strcmp(m_pStatObj->GetFilePath(), m_filePath.value))
	{
		m_pStatObj = gEnv->p3DEngine->LoadStatObj(m_filePath.value, "cloth");
		if (!m_pStatObj || m_pStatObj->IsDefaultObject())
			return;
		GetEntity()->SetStatObj(m_pStatObj, slot | ENTITY_SLOT_ACTUAL, false);
	}

	if (!m_pEntity->GetSlotMaterial(slot) != m_materialPath.value.IsEmpty() || 
			!m_materialPath.value.IsEmpty() && strcmp(m_pEntity->GetSlotMaterial(slot)->GetName(), m_materialPath.value.Left(m_materialPath.value.length() - 4)))
		m_pEntity->SetSlotMaterial(slot, m_materialPath.value.IsEmpty() ? nullptr : gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialPath.value, false));

	if (GetEntity()->GetParent() && GetEntity()->GetParent()->GetPhysics())
		m_entAttachTo = GetEntity()->GetParent()->GetId();

	SEntityPhysicalizeParams params;
	params.type = PE_SOFT;
	IEntity *pentAtt = gEnv->pEntitySystem->GetEntity((EntityId)m_entAttachTo);
	params.pAttachToEntity = pentAtt && pentAtt->GetPhysics() ? pentAtt->GetPhysics() : WORLD_ENTITY;
	params.nAttachToPart = m_attachPart;
	params.mass = m_mass;
	params.density = m_density;
	m_pEntity->Physicalize(params);

	pe_simulation_params sp;
	sp.maxTimeStep = m_simulationParameters.maxTimeStep;
	sp.minEnergy = sqr(m_simulationParameters.sleepSpeed);
	sp.damping = m_simulationParameters.damping;
	m_pEntity->GetPhysicalEntity()->SetParams(&sp);

	pe_params_softbody psb;
	psb.thickness = m_thickness;
	psb.massDecay = m_massDecay;
	psb.friction = m_friction;
	psb.airResistance = m_airResistance;
	psb.wind = m_wind;
	psb.windVariance = m_windVariance;
	psb.pressure = m_pressure;
	psb.densityInside = m_densityInside;
	psb.impulseScale = m_impulseScale;
	psb.explosionScale = m_explosionScale;
	psb.collTypes = (m_collTerrain ? ent_terrain : 0) | (m_collStatic ? ent_static : 0) | (m_collDynamic ? ent_rigid | ent_sleeping_rigid : 0) | (m_collPlayers ? ent_living : 0);
	psb.ks = m_simulationParameters.hardness;
	psb.nMaxIters = m_simulationParameters.maxIters;
	psb.accuracy = m_simulationParameters.accuracy;
	psb.maxSafeStep = m_simulationParameters.maxStretch;
	m_pEntity->GetPhysicalEntity()->SetParams(&psb);
}

void CClothComponent::AttachToEntity(Schematyc::ExplicitEntityId entAttachTo, int idpart) 
{ 
	m_entAttachTo = (EntityId)entAttachTo;
	m_attachPart = idpart;
	Reattach();
}

void CClothComponent::Reattach()
{
	if (IPhysicalEntity *pent = GetEntity()->GetPhysics())
	{
		if (pent->GetiForeignData() != PHYS_FOREIGN_ID_ENTITY)
		{
			pe_params_foreign_data pfd;
			pfd.iForeignData = PHYS_FOREIGN_ID_USER + (int)m_entAttachTo;
			pfd.iForeignFlags = m_attachPart;
			pent->SetParams(&pfd);
		}
		else
		{
			pe_action_attach_points aap;
			aap.pEntity = gEnv->pPhysicalWorld->GetPhysicalEntityById((int)m_entAttachTo);
			if ((aap.partid = m_attachPart) < 0)
				MARK_UNUSED aap.partid;
			MARK_UNUSED aap.nPoints;
			GetEntity()->GetPhysics()->Action(&aap);
		}
	}
}

void CClothComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_RESET:
			if (!m_pStatObj || !(m_pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
				break;
			m_pStatObj = nullptr;
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			Physicalize();
			break;
		case ENTITY_EVENT_START_GAME:
			{
				if (GetEntity()->GetParent() && GetEntity()->GetParent()->GetPhysics())
					m_entAttachTo = GetEntity()->GetParent()->GetId();
				if ((EntityId)m_entAttachTo != INVALID_ENTITYID)
					Reattach();
				pe_action_awake pa;
				pa.bAwake = m_airResistance > 0;
				m_pEntity->GetPhysicalEntity()->Action(&pa);
			}
			break;
		case ENTITY_EVENT_DETACH_THIS:
			m_entAttachTo = INVALID_ENTITYID;
			Reattach();
			break;
	}
}

Cry::Entity::EventFlags CClothComponent::GetEventMask() const
{
	Cry::Entity::EventFlags bitFlags = ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED | ENTITY_EVENT_START_GAME | ENTITY_EVENT_RESET | ENTITY_EVENT_DETACH_THIS;
	return bitFlags;
}

}
}
