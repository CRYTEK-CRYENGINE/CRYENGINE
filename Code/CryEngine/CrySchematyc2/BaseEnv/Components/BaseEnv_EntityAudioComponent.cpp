// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SchematycLegacy/Entity/Components/EntityAudioComponent.h"

#include "SchematycLegacy/AutoRegistrar.h"

#include "SchematycLegacy/Entity/Components/EntityVisualComponent.h"

#include <ICryAnimation.h>

#include <Serialization/IArchive.h>
#include <Serialization/Enum.h>
#include <Serialization/Math.h>
#include <Serialization/STL.h>

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc, AudioProxyAttachType, EValue, "Attachtment Type")
	SERIALIZATION_ENUM(Schematyc::AudioProxyAttachType::FixedOffset, "FixedOffset", "Fixed offset")
	SERIALIZATION_ENUM(Schematyc::AudioProxyAttachType::CharacterJoint, "CharacterJoint", "Character joint")
SERIALIZATION_ENUM_END()

namespace Schematyc
{
	namespace Entity
	{
		const SGUID CAudioComponent::COMPONENT_GUID = "63a207f7-7d54-4282-b070-9af2bb00dd1f";
		const SGUID CAudioComponent::SET_SWITCH_STATE_FUNCTION_GUID = "fa3b3639-2d53-476e-872f-af818b2e077a";
		const SGUID CAudioComponent::SET_RTPC_FUNCTION_GUID = "a7e69590-ec46-49ed-9055-b6370072e721";

		//////////////////////////////////////////////////////////////////////////
		struct SRevertedSortPred
		{
			bool operator()(const uint32 left, const uint32 right) const
			{
				return right < left;
			}
		};

		//////////////////////////////////////////////////////////////////////////
		bool CAudioComponent::Init(const SComponentParams& params)
		{
			if(CComponentBase::Init(params))
			{
				params.properties.Get(m_properties);
				Initialize();
				return true;
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::Run(ESimulationMode simulationMode)
		{
			if (simulationMode == ESimulationMode::Game)
			{
				for(TAudioSwitchSettings::const_iterator it = m_properties.audioSwitchSettings.begin(); it != m_properties.audioSwitchSettings.end(); ++it)
				{
					const SAudioSwitchSetting& switchSetting = *it;
					SetSwitchState(switchSetting.switchName, switchSetting.switchStateName);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		IAnyPtr CAudioComponent::GetPreviewProperties() const
		{
			ScopedSwitchToGlobalHeap();	// Ensure properties are allocated on global heap.
			static IAnyPtr	pPreviewProperties = MakeAnyShared(SPreviewProperties());
			return pPreviewProperties;
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const IAnyConstPtr& pPreviewProperties) const
		{
			SPreviewProperties	previewProperties;
			if(pPreviewProperties)
			{
				pPreviewProperties->Get(previewProperties);
			}

			if (previewProperties.showAudioProxies)
			{
				IRenderer& renderer = *gEnv->pRenderer;
				IRenderAuxGeom&	renderAuxGeom = *renderer.GetIRenderAuxGeom();
				const ColorF sphereColor    = Col_ForestGreen;
				const float  textColor[4]   = { 1.0f, 1.0f, 1.0f, 0.75f };

				SAuxGeomRenderFlags	prevRenderFlags = renderAuxGeom.GetRenderFlags();
				renderAuxGeom.SetRenderFlags(e_Mode3D | e_FillModeWireframe);

				stack_string displayName = "Audio Proxy: 'Default'";
				for (uint32 i = 0; i < m_audioController.GetProxyControllerCount(); ++i)
				{
					const Matrix34& proxyLocation = m_audioController.GetProxyController(i).GetWorldTM();
					renderAuxGeom.DrawSphere(proxyLocation.GetTranslation(), 0.1f, sphereColor, false);

					if ((i > 0) && (i <= m_properties.auxiliaryProxies.size()))
					{
						displayName.Format("Audio Proxy: '%s'", m_properties.auxiliaryProxies[i-1].name.c_str()); 
					}
					renderer.DrawLabelEx(proxyLocation.GetTranslation(), 1.2f, textColor, true, true, displayName.c_str());
				}

				renderAuxGeom.SetRenderFlags(prevRenderFlags);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::Shutdown()
		{
			m_audioController.StopAll();
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::ExecuteTrigger(const char* szAuxProxyName, const TAudioControlID triggerId, const TAudioControlID stopTriggerId)
		{
			ExecuteTrigger(FindAuxProxyIndexByName(szAuxProxyName), triggerId, stopTriggerId);
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::ExecuteTrigger(const uint32 proxyIndex, const TAudioControlID triggerId, const TAudioControlID stopTriggerId)
		{
			if (proxyIndex < m_audioController.GetProxyControllerCount())
			{
				if(m_audioController.GetProxyController(proxyIndex).Play(GameAudio::STrigger(triggerId), GameAudio::STrigger(stopTriggerId)))
				{
					if (AudioProxyNeedsUpdate(proxyIndex))
					{
						RegisterForUpdate();
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::StopTrigger(TAudioControlID triggerId, const string& auxProxyName)
		{
			const uint32 auxProxyIdx = FindAuxProxyIndexByName(auxProxyName);
			m_audioController.GetProxyController(auxProxyIdx).Stop(GameAudio::STrigger(triggerId));
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::Register()
		{
			IEnvRegistry&	envRegistry = GetSchematycFramework().GetEnvRegistry();

			IComponentFactoryPtr	pComponentFactory = SCHEMATYC_MAKE_COMPONENT_FACTORY_SHARED(CAudioComponent, SProperties, COMPONENT_GUID);
			pComponentFactory->SetName("Audio");
			pComponentFactory->SetNamespace("Entity");
			pComponentFactory->SetAuthor("Paul Slinger");
			pComponentFactory->SetDescription("Entity audio component");
			pComponentFactory->SetFlags(EComponentFlags::Singleton);
			envRegistry.RegisterComponentFactory(pComponentFactory);

			{
				IComponentMemberFunctionPtr	pSetSwitchStateFunction = SCHEMATYC_MAKE_COMPONENT_MEMBER_FUNCTION_SHARED(CAudioComponent::SetSwitchState, SET_SWITCH_STATE_FUNCTION_GUID, COMPONENT_GUID);
				pSetSwitchStateFunction->SetAuthor("Thorsten Knuck");
				pSetSwitchStateFunction->SetDescription("Sets an audio switch state on the auxiliary proxy.");
				pSetSwitchStateFunction->BindInput(0, "Switch", "Name of the switch.", Schematyc::SAudioSwitchName());
				pSetSwitchStateFunction->BindInput(1, "SwitchState", "Name of the state.", Schematyc::SAudioSwitchStateName());
				envRegistry.RegisterComponentMemberFunction(pSetSwitchStateFunction);
			}

			{
				IComponentMemberFunctionPtr	pSetRtpcFunction = SCHEMATYC_MAKE_COMPONENT_MEMBER_FUNCTION_SHARED(CAudioComponent::SetRtpc, SET_RTPC_FUNCTION_GUID, COMPONENT_GUID);
				pSetRtpcFunction->SetAuthor("Thorsten Knuck");
				pSetRtpcFunction->SetDescription("Sets an RTPC (Real Time Parameter Control) value on the auxiliary proxy.");
				pSetRtpcFunction->BindInput(0, "Proxy", "Name of the auxiliary proxy.", "");
				pSetRtpcFunction->BindInput(1, "RtpcName", "Name of the RTPC to set.", Schematyc::SRtpcName());
				pSetRtpcFunction->BindInput(2, "RtpcValue", "Value to set.", 0.0f);
				envRegistry.RegisterComponentMemberFunction(pSetRtpcFunction);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::GenerateAudioProxyStringListForSettings(Serialization::IArchive& archive, Serialization::StringList& stringList, int& iDefaultValue)
		{
			const CAudioComponent::TAuxiliarProxies* pProxies = archive.context<CAudioComponent::TAuxiliarProxies>();
			GenerateStringListFromProxies(pProxies, stringList, iDefaultValue);
		}

		void CAudioComponent::GenerateAudioProxyStringList(Serialization::IArchive& archive, Serialization::StringList& stringList, int& iDefaultValue)
		{
			const CAudioComponent::SProperties*	pAudioComponentProperties = archive.context<const CAudioComponent::SProperties>();
			if(pAudioComponentProperties)
			{
				GenerateStringListFromProxies(&pAudioComponentProperties->auxiliaryProxies, stringList, iDefaultValue);
			}
		}

		void CAudioComponent::GenerateStringListFromProxies(const TAuxiliarProxies* pProxies, Serialization::StringList& stringList, int& iDefaultValue)
		{
			CRY_ASSERT(pProxies);
			if(pProxies)
			{
				const size_t auxiliaryProxyCount = pProxies->size();
				stringList.reserve(auxiliaryProxyCount + 1);
				stringList.push_back("Default");
				for(TAuxiliarProxies::const_iterator it = pProxies->begin(); it != pProxies->end(); ++it)
				{
					const char*	auxiliaryProxyName = (*it).name.c_str();
					if(auxiliaryProxyName[0])
					{
						stringList.push_back(auxiliaryProxyName);
					}
				}
				iDefaultValue = 0;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::Initialize()
		{
			CEntityInterface::BindInterface(GetEntityId());

			const uint32 auxProxyCount = m_properties.auxiliaryProxies.size();
			m_audioController.Init(GetEntity(), auxProxyCount);

			const ICharacterInstance* pCharacterInstance = GetCharacter();

			// Cache mapping between proxies and boneIds
			if (pCharacterInstance)
			{
				const uint32 proxyToJointCount = GetAudioProxiesBoundToCharacterJointCount();
				if (proxyToJointCount > 0)
				{
					if (m_properties.defaultProxyProperties.attachType == AudioProxyAttachType::CharacterJoint)
					{
						const int32 jointId = pCharacterInstance->GetIDefaultSkeleton().GetJointIDByName(m_properties.defaultProxyProperties.characterJoint.c_str());
						if (jointId >= 0)
						{
							m_audioController.BindProxyControllerToCharacterJoint(0, jointId);
						}
					}

					for (uint32 i = 0; i < m_properties.auxiliaryProxies.size(); ++i)
					{
						const SAuxAudioProxy& auxAudioProxy = m_properties.auxiliaryProxies[i];
						if (auxAudioProxy.properties.attachType == AudioProxyAttachType::CharacterJoint)
						{
							const int32 jointId = pCharacterInstance->GetIDefaultSkeleton().GetJointIDByName(auxAudioProxy.properties.characterJoint.c_str());
							if (jointId >= 0)
							{
								m_audioController.BindProxyControllerToCharacterJoint(min(i + 1, m_audioController.GetProxyControllerCount() - 1), jointId);
							}
						}
					}
				}
			}

			// Set initial default proxy location
			Matrix34 location;
			GetLocalOffsetForProxyFromProperties(pCharacterInstance, m_properties.defaultProxyProperties, location);
			m_audioController.GetProxyController(0).SetLocationOffset(location);
			m_audioController.GetProxyController(0).SetObstructionType(m_properties.defaultProxyProperties.obstructionType);

			// Set initial auxiliary proxies location
			uint32 proxyIdx = 1;
			for (TAuxiliarProxies::const_iterator it = m_properties.auxiliaryProxies.begin(), itEnd = m_properties.auxiliaryProxies.end(); 
				   (it != itEnd) && (proxyIdx < m_audioController.GetProxyControllerCount()); ++it, ++proxyIdx)
			{
				GetLocalOffsetForProxyFromProperties(pCharacterInstance, it->properties, location);
				m_audioController.GetProxyController(proxyIdx).SetLocationOffset(location);
				m_audioController.GetProxyController(proxyIdx).SetObstructionType(it->properties.obstructionType);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		uint32 CAudioComponent::GetAudioProxiesBoundToCharacterJointCount() const
		{
			uint32 proxyToJointCount = 0;
			if (m_properties.defaultProxyProperties.attachType == AudioProxyAttachType::CharacterJoint)
			{
				proxyToJointCount++;
			}

			for (TAuxiliarProxies::const_iterator it = m_properties.auxiliaryProxies.begin(), itEnd = m_properties.auxiliaryProxies.end(); (it != itEnd); ++it)
			{
				if(it->properties.attachType == AudioProxyAttachType::CharacterJoint)
					proxyToJointCount++;
			}

			return proxyToJointCount;
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::GetLocalOffsetForProxyFromProperties(const ICharacterInstance* pCharacterInstance, const SAudioProxyProperties& proxyProperties, Matrix34& outLocation)
		{
			outLocation.SetIdentity();

			if (proxyProperties.attachType == AudioProxyAttachType::FixedOffset)
			{
				outLocation.SetTranslation(proxyProperties.offset);
			}
			else if (proxyProperties.attachType == AudioProxyAttachType::CharacterJoint)
			{
				const int32 jointId = pCharacterInstance ? pCharacterInstance->GetIDefaultSkeleton().GetJointIDByName(proxyProperties.characterJoint.c_str()) : -1;
				if (jointId != -1)
				{
					outLocation = Matrix34(pCharacterInstance->GetISkeletonPose()->GetAbsJointByID(jointId));
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::GetLocalOffsetForProxyFromJoint(const ICharacterInstance& characterInstance, const int32 jointId, Matrix34& outLocation)
		{
			outLocation = Matrix34(characterInstance.GetISkeletonPose()->GetAbsJointByID(jointId));
		}

		//////////////////////////////////////////////////////////////////////////
		uint32 CAudioComponent::FindAuxProxyIndexByName(const char* szAuxProxyName) const
		{
			if ((szAuxProxyName == NULL) || (strlen(szAuxProxyName) == 0))
				return 0;

			for(uint32 i = 0; i < m_properties.auxiliaryProxies.size(); ++i)
			{
				if(strcmp(m_properties.auxiliaryProxies[i].name.c_str(), szAuxProxyName) == 0)
					return (i + 1);
			}

			return 0;
		}

		//////////////////////////////////////////////////////////////////////////
		const ICharacterInstance* CAudioComponent::GetCharacter() const
		{
			const CVisualComponent* pVisualComponent = CVisualComponent::QueryInterface(GetEntityId());
			return pVisualComponent ? pVisualComponent->GetGameCharacter().GetCurrentCharacterInstance() : NULL;
		}

		//////////////////////////////////////////////////////////////////////////
		bool CAudioComponent::AudioProxyNeedsUpdate(const uint32 proxyIdx) const
		{
			return m_audioController.GetProxyController(proxyIdx).RequiresLocationUpdate();
		}

		void CAudioComponent::RegisterForUpdate()
		{
			if(!m_signalScopes.update.IsBound())
			{
				GetSchematycFramework().GetUpdateScheduler().Connect(m_signalScopes.update, MAKE_MEMBER_DELEGATE(CAudioComponent::Update, *this), EUpdateFrequency::EveryFourFrames, EUpdatePriority::Default);
			}
		}

		void CAudioComponent::UnregisterForUpdate()
		{
			if (m_signalScopes.update.IsBound())
			{
				GetSchematycFramework().GetUpdateScheduler().Disconnect(m_signalScopes.update);
			}
		}

		void CAudioComponent::Update(const SUpdateContext& updateContext)
		{
			FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

			bool requiresUpdate = false;
			
			const ICharacterInstance* pCharacterInstance = GetCharacter();
			if (pCharacterInstance)
			{
				requiresUpdate = m_audioController.UpdateProxyControllersOnCharacterJoints(*pCharacterInstance->GetISkeletonPose());
			}
			
			// Clean up any dynamic proxy we might not need anymore
			// Note: Indices in array are ordered from high to low, so we can perform correct removal of proxies
			//       Removal might happen over multiple frames to avoid index miss-matches
			for (TDynamicAudioProxies::iterator it = m_dynamicAudioProxies.begin(); it != m_dynamicAudioProxies.end(); )
			{
				if ((*it < m_audioController.GetProxyControllerCount()) && (m_audioController.GetProxyController(*it).RequiresLocationUpdate()))
				{
					break;
				}
				else
				{
					m_audioController.RemoveProxyController(*it);
					TDynamicAudioProxies::iterator nextIt = m_dynamicAudioProxies.erase(it);
					it = nextIt;
				}
			}

			if(!requiresUpdate)
			{
				UnregisterForUpdate();
			}
		}

		void CAudioComponent::SetSwitchState(const Schematyc::SAudioSwitchName& switchName, const Schematyc::SAudioSwitchStateName& switchStateName)
		{
			const GameAudio::SSwitchState switchState(switchName.value.c_str(), switchStateName.value.c_str());
			m_audioController.SetSwitchState(switchState);
		}

		void CAudioComponent::SetRtpc(const char* szAuxProxyName, const Schematyc::SRtpcName& rtpcName, const float rtpcValue)
		{
			SetRtpcForProxy(FindAuxProxyIndexByName(szAuxProxyName), rtpcName.value.c_str(), rtpcValue);
		}

		void CAudioComponent::SetRtpcForProxy(const uint32 proxyIndex, const char* szRtpcName, const float rtpcValue)
		{
			if(proxyIndex < m_audioController.GetProxyControllerCount())
			{
				const GameAudio::SRtpc rtpc(szRtpcName, rtpcValue);
				m_audioController.GetProxyController(proxyIndex).SetRtpc(rtpc);
			}
		}

		uint32 CAudioComponent::GetAudioProxyIndex(const char* szAuxProxyName) const
		{
			return FindAuxProxyIndexByName(szAuxProxyName);
		}

		uint32 CAudioComponent::GetAudioProxyId(const char* szAuxProxyName) const
		{
			return m_audioController.GetProxyController(FindAuxProxyIndexByName(szAuxProxyName)).GetAudioProxyId();
		}

		uint32 CAudioComponent::AddDynamicAudioProxyAtCharacterJoint(const int32 jointId)
		{
			CRY_ASSERT (jointId >= 0);
			
			const uint32 newProxyIndex = m_audioController.AddProxyController();
			m_audioController.GetProxyController(newProxyIndex).SetObstructionType(m_properties.dynamicAudioObjectsObstructionType);
			m_audioController.BindProxyControllerToCharacterJoint(newProxyIndex, jointId);

			m_dynamicAudioProxies.push_back(newProxyIndex);
			std::sort(m_dynamicAudioProxies.begin(), m_dynamicAudioProxies.end(), SRevertedSortPred());
			
			return newProxyIndex;
		}

		void CAudioComponent::AttachAudioController(GameAudio::CEntityControllerPtr pChildController)
		{
			m_audioController.Attach(pChildController);
		}

		bool CAudioComponent::DetachAudioController(GameAudio::CEntityControllerPtr pChildController)
		{
			return m_audioController.Detach(pChildController);
		}

		void CAudioComponent::CloneAudioController(GameAudio::CEntityController& controller) const
		{
			m_audioController.CloneSwitchState(controller);
		}

		//////////////////////////////////////////////////////////////////////////
		void CAudioComponent::SPreviewProperties::Serialize(Serialization::IArchive& archive)
		{
			archive(showAudioProxies, "showAudioProxies", "Show audio proxies");
		}

		void CAudioComponent::SAudioProxyProperties::Serialize(Serialization::IArchive& archive)
		{
			archive(attachType, "attachType", "Attachment Type");
			archive.doc("Attachment type");
			if (attachType == AudioProxyAttachType::FixedOffset)
			{
				archive(offset, "offset", "Offset");
				archive.doc("Relative offset from entity position");
			}
			else if (attachType == AudioProxyAttachType::CharacterJoint)
			{
				archive(characterJoint, "characterJoint", "Joint");
				archive.doc("Attach the audio proxy to a character joint"); 
			}

			archive(obstructionType, "obstructionType", "Obstruction");
			archive.doc("Obstruction type for this audio object");
		}

		void CAudioComponent::SAuxAudioProxy::Serialize(Serialization::IArchive& archive)
		{
			archive(name, "name", "^Name");
			archive(properties, "properties", "Properties");
		}

		void CAudioComponent::SProperties::Serialize(Serialization::IArchive& archive) 
		{
			archive(defaultProxyProperties, "defaultProxyProperties", "Default Audio Proxy Properties");
			archive(auxiliaryProxies, "auxiliaryProxies", "Auxiliary Audio Proxies");
			Serialization::SContext proxiesContext(archive, &auxiliaryProxies);
			archive(audioSwitchSettings, "audioSwitchSettings", "Initial Switch Settings");

			if (archive.openBlock("options", "Other Options"))
			{
				archive(dynamicAudioObjectsObstructionType, "dynamicAudioObjectsObstructionType", "Dynamic AudioObject Obstruction");
				archive.doc("Defines default audio obstruction type for audio objects created dynamically on this entity (e.g. from animation events)"); 
				archive.closeBlock();
			}
		}

		void CAudioComponent::SAudioSwitchSetting::Serialize(Serialization::IArchive& archive) 
		{
			archive(switchName, "switchName", "Switch");
			archive(switchStateName, "stateName", "State");
		}
	}
}

void RegisterSchematycEntityAudioComponent()
{
	Schematyc::Entity::CAudioComponent::Register();
}

// N.B. Ideally we would just call the register function directly, but this results in strange compiler errors on Durango.
SCHEMATYC_AUTO_REGISTER(RegisterSchematycEntityAudioComponent)
