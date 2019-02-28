#include "StdAfx.h"
#include "PointConstraint.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CPointConstraintComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPointConstraintComponent::ConstrainToEntity, "{7310C27B-1B70-4274-80EE-2DBF46085DC8}"_cry_guid, "ConstrainToEntity");
				pFunction->SetDescription("Adds a constraint, tying this component's physical entity to the specified entity");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'enti', "Target Entity", "Defines the entity we want to be constrained to, or the point itself if id is 0", Schematyc::ExplicitEntityId());
				pFunction->BindInput(2, 'igno', "Ignore Collisions With", "Whether or not to ignore collisions between this entity and the target", false);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPointConstraintComponent::ConstrainToPoint, "{97A9A9D5-6821-4914-87FB-0990534E8409}"_cry_guid, "ConstrainToPoint");
				pFunction->SetDescription("Adds a constraint, tying this component's physical entity to the point");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CPointConstraintComponent::Remove, "{DD75FE1D-7147-4609-974E-EE6F051ADCC1}"_cry_guid, "Remove");
				pFunction->SetDescription("Removes the constraint");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
		}

		void CPointConstraintComponent::Initialize()
		{
			Reset();
		}

		void CPointConstraintComponent::OnShutDown()
		{
			Remove();
		}

		void CPointConstraintComponent::Reset()
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

		void CPointConstraintComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_START_GAME)
			{
				Reset();
			}
			else if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				m_pEntity->UpdateComponentEventMask(this);

				Reset();
			}
		}

		uint64 CPointConstraintComponent::GetEventMask() const
		{
			uint64 bitFlags = m_bActive ? ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) : 0;
			bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

			return bitFlags;
		}

#ifndef RELEASE
		void CPointConstraintComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
		{
			if (context.bSelected)
			{
				Vec3 axis = Vec3(m_axis.value) * m_pEntity->GetRotation().GetInverted();

				Vec3 pos1 = m_pEntity->GetSlotWorldTM(GetEntitySlotId()).GetTranslation();
				pos1.x += 0.5f * axis.x;
				pos1.y += 0.5f * axis.y;
				pos1.z += 0.5f * axis.z;

				Vec3 pos2 = m_pEntity->GetSlotWorldTM(GetEntitySlotId()).GetTranslation();
				pos2.x += -0.5f * axis.x;
				pos2.y += -0.5f * axis.y;
				pos2.z += -0.5f * axis.z;

				gEnv->pAuxGeomRenderer->DrawLine(pos1, context.debugDrawInfo.color, pos2, context.debugDrawInfo.color, 2.0f);

				gEnv->pAuxGeomRenderer->DrawSphere(m_pEntity->GetSlotWorldTM(GetEntitySlotId()).GetTranslation(), 0.1f, context.debugDrawInfo.color, true);
			}
		}
#endif
	}
}