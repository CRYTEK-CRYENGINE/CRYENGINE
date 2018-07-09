#include "StdAfx.h"
#include "FogComponent.h" 

namespace Cry
{
	namespace DefaultComponents
	{
		void CFogComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CFogComponent::Enable, "{40F9CF4D-EC81-47B3-B9A5-46F6441FD543}"_cry_guid, "Enable");
				pFunction->SetDescription("Enables or disables the fog component");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
				pFunction->BindInput(1, 'act', "Enable");
				componentScope.Register(pFunction);
			}
		}

		void CFogComponent::Initialize()
		{
			// Call enable which will initialize the component if it's active
			Enable(m_bActive);
		}

		void CFogComponent::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
			case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			{
				Initialize();
			}
			break;
			}
		}

		uint64 CFogComponent::GetEventMask() const
		{
			return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
		}
	}
}