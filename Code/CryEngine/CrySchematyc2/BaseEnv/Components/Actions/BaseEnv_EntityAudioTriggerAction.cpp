/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2014.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Schematyc entity audio trigger action.
-------------------------------------------------------------------------
History:
- 27:10:2014: Created by Paul Slinger
*************************************************************************/

#include "StdAfx.h"

#include "SchematycLegacy/Prerequisites.h"

#include <Serialization/Decorators/Range.h>
#include <Serialization/Decorators/ResourcesAudio.h>

#include "Misc/MathUtils.h"
#include "SchematycLegacy/AutoRegistrar.h"
#include "SchematycLegacy/Entity/EntityActionBase.h"
#include "SchematycLegacy/Entity/EntityUpdateFilters.h"
#include "SchematycLegacy/Entity/Components/EntityAudioComponent.h"

namespace Schematyc
{
	namespace Entity
	{
		class CAudioTriggerAction : public CActionBase
		{
		public:

			inline CAudioTriggerAction()
				: m_pAudioComponent(NULL)
				, m_triggerId(INVALID_AUDIO_CONTROL_ID)
				, m_stopTriggerId(INVALID_AUDIO_CONTROL_ID)
			{}

			// IAction
	
			virtual bool Init(const SActionParams& params) override
			{
				if(CActionBase::Init(params))
				{
					m_pAudioComponent = static_cast<CAudioComponent*>(params.pComponent);
					CRY_ASSERT(m_pAudioComponent);
					if(m_pAudioComponent)
					{
						params.properties.Get(m_properties);
						if (CanExecute() && Replicate())
						{
							gEnv->pAudioSystem->GetAudioTriggerID(m_properties.trigger.c_str(), m_triggerId);
							gEnv->pAudioSystem->GetAudioTriggerID(m_properties.stopTrigger.c_str(), m_stopTriggerId);
						}
						return true;
					}
				}
				return false;
			}

			virtual void Start() override
			{
				if(m_triggerId != INVALID_AUDIO_CONTROL_ID)
				{
					m_pAudioComponent->ExecuteTrigger(m_properties.auxProxyName.c_str(), m_triggerId, m_stopTriggerId);
				}
			}

			virtual void Stop() override
			{
				if(m_triggerId != INVALID_AUDIO_CONTROL_ID)
				{
					if(m_properties.stop == true)
					{
						m_pAudioComponent->StopTrigger(m_triggerId, m_properties.auxProxyName.c_str());
					}
				}
			}

			virtual void Shutdown() override {}

			// ~IAction

			static inline void Register()
			{
				IEnvRegistry&			envRegistry = GetSchematycFramework().GetEnvRegistry();

				IActionFactoryPtr	pActionFactory = SCHEMATYC_MAKE_COMPONENT_ACTION_FACTORY_SHARED(CAudioTriggerAction, SProperties, ACTION_GUID, CAudioComponent::COMPONENT_GUID);
				pActionFactory->SetName("AudioTrigger");
				pActionFactory->SetAuthor("Paul Slinger");
				pActionFactory->SetDescription("Audio trigger");
				pActionFactory->SetFlags(EActionFlags::NetworkReplicateClients);
				envRegistry.RegisterActionFactory(pActionFactory);
			}

			static const SGUID ACTION_GUID;

		private:
			ILINE bool CanExecute() const
			{
				return !gEnv->IsDedicated();
			}

			ILINE bool Replicate() const
			{
				switch(m_properties.relevance)
				{
				case EActionRelevance::Local:
					return CActionBase::GetObject().GetNetworkObject().LocalAuthority();
				case EActionRelevance::All:
				default:
					return true;
				}
			}

		private:

			struct SProperties
			{
				inline SProperties()
					: auxProxyName(DynamicStringListGenerator::FromGlobalFunction<CAudioComponent::GenerateAudioProxyStringList>())
					, stop(false)
					, relevance(EActionRelevance::All)
				{}

				inline void Serialize(Serialization::IArchive& archive)
				{
					archive(Serialization::AudioTrigger(trigger), "trigger", "Trigger");
					archive.doc("Trigger");
					archive(auxProxyName, "auxProxyName", "Proxy");
					archive.doc("Audio proxy to execute the trigger");
					if(archive.openBlock("stopOptions", "Stop"))
					{
						archive(stop, "stop", "^^");
						archive.doc("Stop the trigger when the action ends");
						if (stop || !archive.isEdit())
						{
							archive(Serialization::AudioTrigger(stopTrigger), "stopTrigger", "Optional Stop Trigger");
						}
						archive.closeBlock();
					}
					archive(relevance, "clientRelevance", "Relevance (client owned)");
					archive.doc("Defines to who this action is relevant for client owned entities.");
				}

				CDynamicStringListValue	auxProxyName;
				string									trigger;
				string									stopTrigger;
				EActionRelevance				relevance;
				bool										stop;
			};

			CAudioComponent*  m_pAudioComponent;

			TAudioControlID   m_triggerId;
			TAudioControlID   m_stopTriggerId;
			
			SProperties       m_properties;
		};

		const SGUID CAudioTriggerAction::ACTION_GUID = "99729622-92b0-4e57-a819-fefac3ef0601";
	}
}

void RegisterSchematycEntityAudioTriggerAction()
{
	Schematyc::Entity::CAudioTriggerAction::Register();
}

// N.B. Ideally we would just call the register function directly, but this results in strange compiler errors on Durango.
SCHEMATYC_AUTO_REGISTER(RegisterSchematycEntityAudioTriggerAction)
