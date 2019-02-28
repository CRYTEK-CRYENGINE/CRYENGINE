#include "StdAfx.h"
#include "CharacterControllerComponent.h"

namespace Cry
{
namespace DefaultComponents
{
struct SCollisionSignal
{
	Schematyc::ExplicitEntityId otherEntityId;
	Schematyc::SurfaceTypeName surfaceType;
};

void CCharacterControllerComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::IsOnGround, "{C41A9117-0292-4FF6-978A-F433345841B3}"_cry_guid, "IsOnGround");
		pFunction->SetDescription("Checks whether or not the character is on ground");
		pFunction->BindOutput(0, 'ongr', "OnGround");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::GetGroundNormal, "{72858886-D2D8-4FDD-A8C3-30A6AA85E7FD}"_cry_guid, "GetGroundNormal");
		pFunction->SetDescription("Gets the latest ground normal, returns last ground normal if in air");
		pFunction->BindOutput(0, 'norm', "Normal");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::IsWalking, "{80796905-AC9E-4C0F-A1F1-4CBE0B0807C3}"_cry_guid, "IsWalking");
		pFunction->SetDescription("Checks whether or not the player is moving on ground");
		pFunction->BindOutput(0, 'walk', "Walking");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::AddVelocity, "{B9D4C79B-1BDF-4A5C-969D-3456036C9CD4}"_cry_guid, "AddVelocity");
		pFunction->SetDescription("Adds velocity to that of the character");
		pFunction->BindInput(1, 'vel', "Velocity");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::SetVelocity, "{32A91404-8E63-4D59-8330-ADF5F2AB71DE}"_cry_guid, "SetVelocity");
		pFunction->SetDescription("Overrides the character's velocity");
		pFunction->BindInput(1, 'vel', "Velocity");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::ChangeVelocity, "{B2D208A7-6F80-4580-8AD5-BDD6859872E7}"_cry_guid, "ChangeVelocity");
		pFunction->SetDescription("Changes the character velocity using specified mode");
		pFunction->BindInput(1, 'vel', "Velocity");
		pFunction->BindInput(2, 'mode', "Mode");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::GetMoveDirection, "{7D21BDFC-FE14-4020-A685-6F7917AC759C}"_cry_guid, "GetMovementDirection");
		pFunction->SetDescription("Gets the direction the character is moving in");
		pFunction->BindOutput(0, 'dir', "Direction");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCharacterControllerComponent::GetVelocity, "{BF80D631-05D3-4571-A51D-E0AF6A252F91}"_cry_guid, "GetVelocity");
		pFunction->SetDescription("Gets the character's velocity");
		pFunction->BindOutput(0, 'vel', "Velocity");
		componentScope.Register(pFunction);
	}
	// Signals
	{
		auto pSignal = SCHEMATYC_MAKE_ENV_SIGNAL(SCollisionSignal);
		pSignal->SetDescription("Sent when the entity collided with another object");
		componentScope.Register(pSignal);
	}
}

static void ReflectType(Schematyc::CTypeDesc<SCollisionSignal>& desc)
{
	desc.SetGUID("{24C421C5-3E60-42C2-B0D0-6BA3DFC7CF7C}"_cry_guid);
	desc.SetLabel("On Collision");
	desc.AddMember(&SCollisionSignal::otherEntityId, 'ent', "OtherEntityId", "OtherEntityId", "Other Colliding Entity Id", Schematyc::ExplicitEntityId());
	desc.AddMember(&SCollisionSignal::surfaceType, 'srf', "SurfaceType", "SurfaceType", "Material Surface Type at the collision point", "");
}

CCharacterControllerComponent::~CCharacterControllerComponent()
{
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_NONE;
	m_pEntity->Physicalize(physParams);
}

void CCharacterControllerComponent::Initialize()
{
	Physicalize();

	if (m_bNetworked)
	{
		m_pEntity->GetNetEntity()->BindToNetwork();
	}
}

void CCharacterControllerComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_UPDATE)
	{
		SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];

		IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics();
		CRY_ASSERT_MESSAGE(pPhysicalEntity != nullptr, "Physical entity removed without call to IEntity::UpdateComponentEventMask!");

		// Update stats
		pe_status_living livingStatus;
		if (pPhysicalEntity->GetStatus(&livingStatus) != 0)
		{
			m_bOnGround = !livingStatus.bFlying;

			// Store the ground normal in case it is needed
			// Note that users have to check if we're on ground before using, is considered invalid in air.
			m_groundNormal = livingStatus.groundSlope;
		}

		// Get the player's velocity from physics
		pe_status_dynamics playerDynamics;
		if (pPhysicalEntity->GetStatus(&playerDynamics) != 0)
		{
			m_velocity = playerDynamics.v;
		}
	}
	else if (event.event == ENTITY_EVENT_COLLISION)
	{
		// Collision info can be retrieved using the event pointer
		EventPhysCollision* physCollision = reinterpret_cast<EventPhysCollision*>(event.nParam[0]);

		const char* surfaceTypeName = "";
		EntityId otherEntityId = INVALID_ENTITYID;

		ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		if (ISurfaceType* pSurfaceType = pSurfaceTypeManager->GetSurfaceType(physCollision->idmat[1]))
		{
			surfaceTypeName = pSurfaceType->GetName();
		}

		if (IEntity* pOtherEntity = gEnv->pEntitySystem->GetEntityFromPhysics(physCollision->pEntity[1]))
		{
			otherEntityId = pOtherEntity->GetId();
		}

		// Send OnCollision signal
		if (Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject())
		{
			pSchematycObject->ProcessSignal(SCollisionSignal{ Schematyc::ExplicitEntityId(otherEntityId), Schematyc::SurfaceTypeName(surfaceTypeName) }, GetGUID());
		}
	}
	else if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		// Start validating inputs
#ifndef RELEASE
		// Slide value will have no effect if larger than the fall angle, since we'll fall instead
		if (m_movement.m_minSlideAngle > m_movement.m_minFallAngle)
		{
			m_movement.m_minSlideAngle = m_movement.m_minFallAngle;
		}
#endif

		m_pEntity->UpdateComponentEventMask(this);

		Physicalize();
	}
}

uint64 CCharacterControllerComponent::GetEventMask() const
{
	uint64 eventMask = ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

	// Only update when we have a physical entity
	if (m_pEntity->GetPhysicalEntity() != nullptr)
	{
		eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
	}

	if (m_physics.m_bSendCollisionSignal)
	{
		eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);
	}

	return eventMask;
}

#ifndef RELEASE
void CCharacterControllerComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
		{
			pe_params_part partParams;

			// The living entity main part (cylinder / capsule) is always at index 0
			partParams.ipart = 0;
			if (pPhysicalEntity->GetParams(&partParams))
			{
				Matrix34 entityTransform = m_pEntity->GetWorldTM();

				geom_world_data geomWorldData;
				geomWorldData.R = Matrix33(Quat(entityTransform) * partParams.q);
				geomWorldData.scale = entityTransform.GetUniformScale() * partParams.scale;
				geomWorldData.offset = entityTransform.GetTranslation() + entityTransform.TransformVector(partParams.pos);

				gEnv->pSystem->GetIPhysRenderer()->DrawGeometry(partParams.pPhysGeom->pGeom, &geomWorldData, -1, 0, ZERO, context.debugDrawInfo.color);
			}
		}
	}
}
#endif
}
}
