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

		CPointConstraintComponent::~CPointConstraintComponent()
		{
			Remove();
		}

		void CPointConstraintComponent::Initialize()
		{
			Reset();
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

		void CPointConstraintComponent::ProcessEvent(SEntityEvent& event)
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
			uint64 bitFlags = m_bActive ? BIT64(ENTITY_EVENT_START_GAME) : 0;
			bitFlags |= BIT64(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

			return bitFlags;
		}
	}
}