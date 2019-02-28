// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SchematycLegacy/Prerequisites.h"

#include "SchematycLegacy/Entity/EntityComponentBase.h"
#include "SchematycLegacy/Entity/EntitySignals.h"
#include "Misc/EntityInterface.h"

#include "GameAudio/GameAudioEntityController.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

namespace Schematyc
{
	namespace AudioProxyAttachType
	{
		enum EValue
		{
			FixedOffset = 0,
			CharacterJoint,
		};
	}

	namespace Entity
	{
		class CAudioComponent : public CComponentBase, public CEntityInterface<CAudioComponent>
		{
		private:
			typedef std::vector<uint32> TDynamicAudioProxies;

			struct SPreviewProperties
			{
				SPreviewProperties()
					: showAudioProxies(false)
				{
				}

				void Serialize(Serialization::IArchive& archive);

				bool  showAudioProxies;
			};

		public:
			struct SAudioProxyProperties
			{
				SAudioProxyProperties()
					: attachType(AudioProxyAttachType::FixedOffset)
					, offset(ZERO)
					, obstructionType(GameAudio::eObstructionType_MultiRay)
				{
				}

				void Serialize(Serialization::IArchive& archive);

				AudioProxyAttachType::EValue attachType;
				Vec3    offset;
				string  characterJoint;
				GameAudio::EObstructionType  obstructionType;
			};

			struct SAuxAudioProxy
			{
				SAuxAudioProxy()
				{
				}

				void Serialize(Serialization::IArchive& archive);

				string name;
				SAudioProxyProperties properties;
			};

			typedef std::vector<SAuxAudioProxy> TAuxiliarProxies;

			struct SAudioSwitchSetting
			{
				SAudioSwitchSetting()
				{
				}

				SAudioSwitchSetting(string _switchName, string _switchStateName)
					: switchName(_switchName)
					, switchStateName(_switchStateName)
				{
				}

				Schematyc::SAudioSwitchName switchName;
				Schematyc::SAudioSwitchStateName switchStateName;

				void Serialize(Serialization::IArchive& archive);
			};

			typedef std::vector<SAudioSwitchSetting> TAudioSwitchSettings;

			struct SProperties
			{
				SProperties()
					: dynamicAudioObjectsObstructionType(GameAudio::eObstructionType_MultiRay)
				{

				}

				void Serialize(Serialization::IArchive& archive);

				SAudioProxyProperties defaultProxyProperties;
				TAuxiliarProxies      auxiliaryProxies;
				TAudioSwitchSettings  audioSwitchSettings;

				GameAudio::EObstructionType dynamicAudioObjectsObstructionType;
			};

			// IComponent
			virtual bool Init(const SComponentParams& params) override;
			virtual void Run(ESimulationMode simulationMode) override;
			virtual IAnyPtr GetPreviewProperties() const override;
			virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const IAnyConstPtr& pPreviewProperties) const override;
			virtual void Shutdown() override;
			// ~IComponent

			// Audio playback
			void ExecuteTrigger(const char* szAuxProxyName, const TAudioControlID triggerId, const TAudioControlID stopTriggerId);
			void ExecuteTrigger(const uint32 proxyIndex, const TAudioControlID triggerId, const TAudioControlID stopTriggerId);
			void StopTrigger(const TAudioControlID triggerId, const string& auxProxyName);
			
			void SetSwitchState(const Schematyc::SAudioSwitchName& switchName, const Schematyc::SAudioSwitchStateName& switchStateName);
			void SetRtpc(const char* szAuxProxyName, const Schematyc::SRtpcName& rtpcName, const float rtpcValue);
			void SetRtpcForProxy(const uint32 proxyIndex, const char* szRtpcName, const float rtpcValue);
			
			uint32 GetAudioProxyIndex(const char* szAuxProxyName) const;
			uint32 GetAudioProxyId(const char* szAuxProxyName) const;

			uint32 AddDynamicAudioProxyAtCharacterJoint(const int32 jointId);

			void AttachAudioController(GameAudio::CEntityControllerPtr pChildController);
			bool DetachAudioController(GameAudio::CEntityControllerPtr pChildController);
			void CloneAudioController(GameAudio::CEntityController& controller) const; 

			static void Register();
			static void GenerateAudioProxyStringList(Serialization::IArchive& archive, Serialization::StringList& stringList, int& iDefaultValue);

			static const SGUID COMPONENT_GUID;
			static const SGUID SET_SWITCH_STATE_FUNCTION_GUID;
			static const SGUID SET_RTPC_FUNCTION_GUID;

		private:

			// Misc
			void   Initialize();
			uint32 GetAudioProxiesBoundToCharacterJointCount() const;
			void   GetLocalOffsetForProxyFromProperties(const ICharacterInstance* pCharacterInstance, const SAudioProxyProperties& proxyProperties, Matrix34& outLocation);
			void   GetLocalOffsetForProxyFromJoint(const ICharacterInstance& characterInstance, const int32 jointId, Matrix34& outLocation);
			uint32 FindAuxProxyIndexByName(const char* szAuxProxyName) const;
			const ICharacterInstance* GetCharacter() const;

			bool AudioProxyNeedsUpdate(const uint32 proxyIdx) const;
			void RegisterForUpdate();
			void UnregisterForUpdate();
			void Update(const SUpdateContext& updateContext);

			static void GenerateAudioProxyStringListForSettings(Serialization::IArchive& archive, Serialization::StringList& stringList, int& iDefaultValue);
			static void GenerateStringListFromProxies(const TAuxiliarProxies* pProxies, Serialization::StringList& stringList, int& iDefaultValue);

		private:

			SProperties                  m_properties;
			GameAudio::CEntityController m_audioController;
			TDynamicAudioProxies         m_dynamicAudioProxies;

			struct 
			{
				CUpdateScope                 update;
			} m_signalScopes;
		};
	}
}