#include "StdAfx.h"
#include "PlaneConstraint.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CPlaneConstraintComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPlaneConstraintComponent::ConstrainToEntity, "{761D04E2-672B-4EAD-885E-B7B056001DFA}"_cry_guid, "ConstrainToEntity");
				pFunction->SetDescription("Adds a constraint, tying this component's physical entity to the specified entity");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'enti', "Target Entity", "Defines the entity we want to be constrained to, or the point itself if id is 0", Schematyc::ExplicitEntityId());
				pFunction->BindInput(2, 'igno', "Ignore Collisions With", "Whether or not to ignore collisions between this entity and the target", false);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPlaneConstraintComponent::ConstrainToPoint, "{2A1E5BF3-C98F-40B3-86C6-FF21CB35F4A9}"_cry_guid, "ConstrainToPoint");
				pFunction->SetDescription("Adds a constraint, tying this component's physical entity to the point");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPlaneConstraintComponent::Remove, "{F3E7141F-508F-479D-AD72-2BD838C09905}"_cry_guid, "Remove");
				pFunction->SetDescription("Removes the constraint");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
		}

		void CPlaneConstraintComponent::Initialize()
		{
			Reset();
		}

		void CPlaneConstraintComponent::OnShutDown()
		{
			Remove();
		}

		void CPlaneConstraintComponent::Reset()
		{
			if (m_bActive)
			{
				ConstrainToPoint();
			}
			else
			{
				Remove();
			}
		}

		void CPlaneConstraintComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_START_GAME)
			{
				Reset();
			}
			else if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				m_pEntity->UpdateComponentEventMask(this);

				m_axis = m_axis.GetNormalized();

				Reset();
			}
		}

		uint64 CPlaneConstraintComponent::GetEventMask() const
		{
			uint64 bitFlags = m_bActive ? ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) : 0;
			bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

			return bitFlags;
		}

#ifndef RELEASE
		void CPlaneConstraintComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
		{
			if (context.bSelected)
			{
				Quat rot = Quat::CreateRotationV0V1(Vec3(0, 0, 1), m_axis).GetInverted() * Quat(m_pEntity->GetSlotLocalTM(GetEntitySlotId(), false)).GetInverted() * m_pEntity->GetRotation().GetInverted();

				Vec3 pos1 = Vec3(-1, 1, 0) * rot + m_pEntity->GetWorldPos();
				Vec3 pos2 = Vec3(1, 1, 0) * rot + m_pEntity->GetWorldPos();
				Vec3 pos3 = Vec3(1, -1, 0) * rot + m_pEntity->GetWorldPos();
				Vec3 pos4 = Vec3(-1, -1, 0) * rot + m_pEntity->GetWorldPos();

				gEnv->pAuxGeomRenderer->DrawQuad(pos4, context.debugDrawInfo.color, pos3, context.debugDrawInfo.color, pos2, context.debugDrawInfo.color, pos1, context.debugDrawInfo.color);
				gEnv->pAuxGeomRenderer->DrawQuad(pos1, context.debugDrawInfo.color, pos2, context.debugDrawInfo.color, pos3, context.debugDrawInfo.color, pos4, context.debugDrawInfo.color);
			}
		}
#endif
	}
}