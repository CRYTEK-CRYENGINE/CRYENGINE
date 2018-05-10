// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/BaseEnv_Prerequisites.h"

#include "BaseEnv/BaseEnv_AutoRegistrar.h"
#include "BaseEnv/Components/BaseEnv_EntityLightComponent.h"
#include "BaseEnv/Utils/BaseEnv_EntityActionBase.h"

namespace SchematycBaseEnv
{
	class CEntityLightSwitchAction : public CEntityActionBase
	{
	private:

		struct SProperties
		{
			SProperties()
				: bOn(true)
			{}

			void Serialize(Serialization::IArchive& archive)
			{
				archive(bOn, "on", "On");
			}

			bool bOn;
		};

	public:

		CEntityLightSwitchAction()
			: m_pProperties(nullptr)
			, m_pLightComponent(nullptr)
			, m_bPrevOn(false)
		{}

		// IAction

		virtual bool Init(const Schematyc2::SActionParams& params) override
		{
			if(!CEntityActionBase::Init(params))
			{
				return false;
			}
			m_pProperties     = params.properties.ToPtr<SProperties>();
			m_pLightComponent = static_cast<CEntityLightComponent*>(params.pComponent);
			return true;
		}

		virtual void Start() override
		{
			m_bPrevOn = m_pLightComponent->Switch(m_pProperties->bOn);
		}

		virtual void Stop() override
		{
			m_pLightComponent->Switch(m_bPrevOn);
		}

		virtual void Shutdown() override {}

		// ~IAction

		static void Register()
		{
			Schematyc2::IActionFactoryPtr pActionFactory = SCHEMATYC2_MAKE_COMPONENT_ACTION_FACTORY_SHARED(CEntityLightSwitchAction, SProperties, CEntityLightSwitchAction::s_actionGUID, CEntityLightComponent::s_componentGUID);
			pActionFactory->SetName("Switch");
			pActionFactory->SetAuthor("Crytek");
			pActionFactory->SetDescription("Switch light on/off");
			gEnv->pSchematyc2->GetEnvRegistry().RegisterActionFactory(pActionFactory);
		}

	public:

		static const Schematyc2::SGUID s_actionGUID;

	private:

		const SProperties*     m_pProperties;
		CEntityLightComponent* m_pLightComponent;
		bool                   m_bPrevOn;
	};

	const Schematyc2::SGUID CEntityLightSwitchAction::s_actionGUID = "09c39bb0-4fb6-4ad3-b0ba-ca1fc87c7d9d";
}

SCHEMATYC2_GAME_ENV_AUTO_REGISTER(&SchematycBaseEnv::CEntityLightSwitchAction::Register)
