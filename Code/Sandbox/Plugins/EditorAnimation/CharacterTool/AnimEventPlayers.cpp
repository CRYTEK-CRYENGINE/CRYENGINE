// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimEvent.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimEventPlayer.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IListener.h>
#include <CryAudio/IObject.h>
#include <CryAction/IMaterialEffects.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>
#include "EffectPlayer.h"

#include <CrySerialization/Decorators/ResourcesAudio.h>
#include <CrySerialization/Decorators/Resources.h>

namespace CharacterTool {
using Serialization::IArchive;

void SerializeParameterAudioTrigger(string& parameter, IArchive& ar)
{
	ar(Serialization::AudioTrigger(parameter), "parameter", "^");
	ar.doc("Audio triggers are defined in ATL Controls Browser.");
}

void SerializeParameterString(string& parameter, IArchive& ar) { ar(parameter, "parameter", "^"); }
void SerializeParameterEffect(string& parameter, IArchive& ar) { ar(Serialization::ParticlePickerLegacy(parameter), "parameter", "^"); }
void SerializeParameterNone(string& parameter, IArchive& ar)   { ar(parameter, "parameter", 0); }

void SerializeParameterAnimFXEvent(string& parameter, IArchive& ar)
{
	Serialization::StringList eventList;
	IAnimFXEvents* pAnimFXEvents = gEnv->pMaterialEffects->GetAnimFXEvents();

	if (!pAnimFXEvents)
	{
		return;
	}

	int resultEventTypeUid = -1;
	int resultEventSubTypeUid = -1;

	uint32 eventTypeUid(0u);
	uint32 eventSubTypeUid(0u);
	sscanf_s(parameter.c_str(), "%u %u", &eventTypeUid, &eventSubTypeUid);

	const int eventsCount = pAnimFXEvents->GetAnimFXEventsCount();
	eventList.reserve(eventsCount + 1);

	eventList.push_back("<unassigned>");

	SAnimFXEventInfo eventType;
	for (int eventIndex = 0; eventIndex < eventsCount; ++eventIndex)
	{
		pAnimFXEvents->GetAnimFXEventInfo(eventIndex, eventType);
		eventList.push_back(eventType.szName);
		if (eventTypeUid == eventType.uniqueId)
		{
			resultEventTypeUid = eventIndex + 1; // + 1 because we already pushed ("<unassigned>")
		}
	}

	Serialization::StringListValue eventTypeChoice(eventList, resultEventTypeUid == -1 ? 0 : resultEventTypeUid);
	ar(eventTypeChoice, "parameter", "^");

	for (int eventIndex = 0; eventIndex < eventsCount; ++eventIndex)
	{
		pAnimFXEvents->GetAnimFXEventInfo(eventIndex, eventType);

		if (!strcmpi(eventTypeChoice.c_str(), eventType.szName))
		{
			resultEventTypeUid = eventType.uniqueId;

			Serialization::StringList eventParamList;

			int eventParamCount = pAnimFXEvents->GetAnimFXEventParamCount(eventIndex);
			eventParamList.reserve(eventsCount + 1);

			eventParamList.push_back("<unassigned>");

			if (eventParamCount)
			{
				SAnimFXEventInfo eventParam;
				for (int eventParamIndex = 0; eventParamIndex < eventParamCount; eventParamIndex++)
				{
					pAnimFXEvents->GetAnimFXEventParam(eventIndex, eventParamIndex, eventParam);
					eventParamList.push_back(eventParam.szName);

					if (eventSubTypeUid == eventParam.uniqueId)
					{
						resultEventSubTypeUid = eventParamIndex + 1; // + 1 because we already pushed ("<unassigned>")
					}
				}

				Serialization::StringListValue eventSubTypeChoice(eventParamList, resultEventSubTypeUid == -1 ? 0 : resultEventSubTypeUid);
				ar(eventSubTypeChoice, "subParameter", "^");

				for (int eventParamIndex = 0; eventParamIndex < eventParamCount; ++eventParamIndex)
				{
					pAnimFXEvents->GetAnimFXEventParam(eventIndex, eventParamIndex, eventParam);
					if (!strcmpi(eventSubTypeChoice.c_str(), eventParam.szName))
					{
						resultEventSubTypeUid = eventParam.uniqueId;
					}
				}
			}
			break; // if (!strcmpi(eventTypeChoice.c_str(), eventType.name))
		}
	}

	parameter.Format("%d %d", resultEventTypeUid == -1 ? 0 : resultEventTypeUid, resultEventSubTypeUid == -1 ? 0 : resultEventSubTypeUid);
}

static void SpliterParameterValue(string* switchName, string* switchValue, const char* parameter)
{
	const char* sep = strchr(parameter, '=');
	if (!sep)
	{
		*switchName = parameter;
		*switchValue = string();
	}
	else
	{
		*switchName = string(parameter, sep);
		*switchValue = sep + 1;
	}
}

void SerializeParameterAudioSwitch(string& parameter, IArchive& ar)
{
	string switchName;
	string switchState;
	SpliterParameterValue(&switchName, &switchState, parameter.c_str());
	ar(Serialization::AudioSwitch(switchName), "switchName", "^");
	ar(Serialization::AudioSwitchState(switchState), "switchState", "^");
	if (ar.isInput())
		parameter = switchName + "=" + switchState;
}

void SerializeParameterAudioRTPC(string& parameter, IArchive& ar)
{
	string name;
	string valueString;
	SpliterParameterValue(&name, &valueString, parameter.c_str());
	float value = (float)atof(valueString.c_str());
	ar(Serialization::AudioRTPC(name), "rtpcName", "^");
	ar(value, "rtpcValue", "^");

	char newValue[32];
	cry_sprintf(newValue, "%f", value);
	if (ar.isInput())
		parameter = name + "=" + newValue;
}

typedef void (* TSerializeParameterFunc)(string&, IArchive&);

static SCustomAnimEventType g_atlEvents[] = {
	{ "audio_trigger", ANIM_EVENT_USES_BONE, "Plays audio trigger using ATL"                                    },
	{ "audio_switch",  0,                    "Sets audio switch using ATL"                                      },
	{ "audio_rtpc",    0,                    "Sets RTPC value using ATL"                                        },
	{ "sound",         ANIM_EVENT_USES_BONE, "Play audio trigger using ATL. Obsolete in favor of audio_trigger" },
};

TSerializeParameterFunc g_atlEventFunctions[sizeof(g_atlEvents) / sizeof(g_atlEvents[0])] = {
	&SerializeParameterAudioTrigger,
	&SerializeParameterAudioSwitch,
	&SerializeParameterAudioRTPC,
	&SerializeParameterAudioTrigger
};

static Vec3 GetBonePosition(ICharacterInstance* character, const QuatTS& physicalLocation, const char* boneName)
{
	int jointId = -1;
	if (boneName != '\0')
		jointId = character->GetIDefaultSkeleton().GetJointIDByName(boneName);
	if (jointId != -1)
		return (physicalLocation * character->GetISkeletonPose()->GetAbsJointByID(jointId)).t;
	else
		return character->GetAABB().GetCenter();
}

class AnimEventPlayer_AudioTranslationLayer : public IAnimEventPlayer
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimEventPlayer)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayer_AudioTranslationLayer, "AnimEventPlayer_AudioTranslationLayer", "a9fefa2d-fe04-dec4-a616-9d6e3ac635b0"_cry_guid)

	AnimEventPlayer_AudioTranslationLayer();
	virtual ~AnimEventPlayer_AudioTranslationLayer();

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override
	{
		int count = GetCustomTypeCount();
		if (customTypeIndex < 0 || customTypeIndex >= count)
			return 0;
		return &g_atlEvents[customTypeIndex];
	}

	void Initialize() override
	{
		CryAudio::SCreateObjectData const objectData("Character Tool", CryAudio::EOcclusionType::Ignore);
		m_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);
		SetPredefinedSwitches();
	}

	void Reset() override
	{
		if (!m_pIAudioObject)
			return;
		SetPredefinedSwitches();
	}

	void SetPredefinedSwitches()
	{
		if (!m_pIAudioObject)
			return;
		size_t num = m_predefinedSwitches.size();
		for (size_t i = 0; i < num; ++i)
		{
			SetSwitch(m_predefinedSwitches[i].name.c_str(), m_predefinedSwitches[i].state.c_str());
		}
	}

	int GetCustomTypeCount() const override
	{
		return sizeof(g_atlEvents) / sizeof(g_atlEvents[0]);
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		ar.doc("Allows to play triggers and control switches and RPC through Audio Translation Layer.");

		ar(m_predefinedSwitches, "predefinedSwitches", "Predefined Switches");
		ar.doc("Defines a list of switches that are set before audio triggers are played.\n"
		       "Can be used to test switches that are normally set by the game code.\n"
		       "Such switches could be specific to the character, environment or story.");
	}

	const char* SerializeCustomParameter(const char* parameterValue, Serialization::IArchive& ar, int customTypeIndex) override
	{
		if (customTypeIndex < 0 || customTypeIndex >= GetCustomTypeCount())
			return "";
		m_parameter = parameterValue;
		g_atlEventFunctions[customTypeIndex](m_parameter, ar);
		return m_parameter.c_str();
	}

	void Update(ICharacterInstance* character, const QuatT& playerLocation, const Matrix34& cameraMatrix) override
	{
		m_playerLocation = playerLocation;

		if (!m_audioEnabled)
			return;

		if (m_pIAudioListener != nullptr)
		{
			Matrix34 cameraWithOffset = cameraMatrix;
			cameraWithOffset.SetTranslation(cameraMatrix.GetTranslation());
			m_pIAudioListener->SetTransformation(cameraWithOffset);
		}

		if (m_pIAudioObject != nullptr)
		{
			Vec3 const& pos = GetBonePosition(character, m_playerLocation, m_boneToAttachTo.c_str());
			m_pIAudioObject->SetTransformation(pos);
		}
	}

	void EnableAudio(bool enableAudio) override
	{
		if (enableAudio && m_pIAudioListener == nullptr)
		{
			m_pIAudioListener = gEnv->pAudioSystem->CreateListener();
		}
		else if (m_pIAudioListener != nullptr)
		{
			gEnv->pAudioSystem->ReleaseListener(m_pIAudioListener);
			m_pIAudioListener = nullptr;
		}

		m_audioEnabled = enableAudio;
	}

	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override
	{
		if (!m_pIAudioObject)
			return false;
		const char* name = event.m_EventName ? event.m_EventName : "";
		if (stricmp(name, "sound") == 0 || stricmp(name, "sound_tp") == 0)
		{
			Vec3 const& pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);
			PlayTrigger(event.m_CustomParameter, pos);
			m_boneToAttachTo = event.m_BonePathName;
			return true;
		}
		// If the event is an effect event, spawn the event.
		else if (stricmp(name, "audio_trigger") == 0)
		{
			Vec3 const& pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);
			PlayTrigger(event.m_CustomParameter, pos);
			m_boneToAttachTo = event.m_BonePathName;
			return true;
		}
		else if (stricmp(name, "audio_rtpc") == 0)
		{
			string rtpcName;
			string rtpcValue;
			SpliterParameterValue(&rtpcName, &rtpcValue, event.m_CustomParameter);

			SetRTPC(rtpcName.c_str(), float(atof(rtpcValue.c_str())));
			return true;
		}
		else if (stricmp(name, "audio_switch") == 0)
		{
			string switchName;
			string switchValue;
			SpliterParameterValue(&switchName, &switchValue, event.m_CustomParameter);
			SetSwitch(switchName.c_str(), switchValue.c_str());
			return true;
		}
		return false;
	}

	void SetRTPC(const char* name, float value)
	{
		CryAudio::ControlId const parameterId = CryAudio::StringToId(name);
		m_pIAudioObject->SetParameter(parameterId, value);
	}

	void SetSwitch(const char* name, const char* state)
	{
		CryAudio::ControlId const switchId = CryAudio::StringToId(name);
		CryAudio::SwitchStateId const stateId = CryAudio::StringToId(state);
		m_pIAudioObject->SetSwitchState(switchId, stateId);
	}

	void PlayTrigger(const char* trigger, Vec3 const& pos)
	{
		CryAudio::ControlId const triggerId = CryAudio::StringToId(trigger);
		m_pIAudioObject->SetTransformation(pos);
		m_pIAudioObject->ExecuteTrigger(triggerId);
	}

private:
	struct PredefinedSwitch
	{
		string name;
		string state;

		void Serialize(Serialization::IArchive& ar)
		{
			ar(Serialization::AudioSwitch(name), "name", "^");
			ar(Serialization::AudioSwitchState(state), "state", "^");
		}
	};

	std::vector<PredefinedSwitch> m_predefinedSwitches;

	bool                          m_audioEnabled;
	CryAudio::IObject*            m_pIAudioObject;
	CryAudio::IListener*          m_pIAudioListener;
	string                        m_parameter;
	string                        m_boneToAttachTo;

	QuatT                         m_playerLocation;
};

AnimEventPlayer_AudioTranslationLayer::AnimEventPlayer_AudioTranslationLayer()
	: m_audioEnabled(false)
	, m_pIAudioObject(nullptr)
	, m_pIAudioListener(nullptr)
{
}

AnimEventPlayer_AudioTranslationLayer::~AnimEventPlayer_AudioTranslationLayer()
{
	if (m_pIAudioObject != nullptr)
	{
		gEnv->pAudioSystem->ReleaseObject(m_pIAudioObject);
		m_pIAudioObject = nullptr;
	}

	if (m_pIAudioListener != nullptr)
	{
		gEnv->pAudioSystem->ReleaseListener(m_pIAudioListener);
		m_pIAudioListener = nullptr;
	}
}

CRYREGISTER_CLASS(AnimEventPlayer_AudioTranslationLayer)

// ---------------------------------------------------------------------------

static SCustomAnimEventType g_mfxEvents[] = {
	{ "foley",        ANIM_EVENT_USES_BONE, "Foley are used for sounds that are specific to character and material that is being contacted." },
	{ "footstep",     ANIM_EVENT_USES_BONE, "Footstep are specific to character and surface being stepped on."                               },
	{ "groundEffect", ANIM_EVENT_USES_BONE },
};

TSerializeParameterFunc g_mfxEventFuncitons[sizeof(g_mfxEvents) / sizeof(g_mfxEvents[0])] = {
	&SerializeParameterString,
	&SerializeParameterString,
	&SerializeParameterString,
};

class AnimEventPlayerMaterialEffects : public IAnimEventPlayer
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimEventPlayer)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayerMaterialEffects, "AnimEventPlayer_MaterialEffects", "a9fffa2d-ae34-d6c4-a696-9d6e3ac732b0"_cry_guid)

	AnimEventPlayerMaterialEffects();
	virtual ~AnimEventPlayerMaterialEffects() {}

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override
	{
		int count = GetCustomTypeCount();
		if (customTypeIndex < 0 || customTypeIndex >= count)
			return 0;
		return &g_mfxEvents[customTypeIndex];
	}

	int GetCustomTypeCount() const override
	{
		return sizeof(g_mfxEvents) / sizeof(g_mfxEvents[0]);
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		ar.doc("Material Effects allow to define sound and particle response to interaction with certain surface types.");

		ar(m_foleyLibrary, "foleyLibrary", "Foley Library");
		ar.doc("Foley libraries are located in Libs/MaterialEffects/FXLibs");
		ar(m_footstepLibrary, "footstepLibrary", "Footstep Library");
		ar.doc("Foostep libraries are located in Libs/MaterialEffects/FXLibs");
		ar(m_defaultFootstepEffect, "footstepEffectName", "Default Footstep Effect");
		ar.doc("Foostep effect that is used when no effect name specified in AnimEvent.");
	}

	const char* SerializeCustomParameter(const char* parameterValue, Serialization::IArchive& ar, int customTypeIndex) override
	{
		if (customTypeIndex < 0 || customTypeIndex >= GetCustomTypeCount())
			return "";
		m_parameter = parameterValue;
		g_mfxEventFuncitons[customTypeIndex](m_parameter, ar);
		return m_parameter.c_str();
	}

	void Update(ICharacterInstance* character, const QuatT& playerLocation, const Matrix34& cameraMatrix) override
	{
		m_playerLocation = playerLocation;
	}

	void EnableAudio(bool enableAudio) override
	{
		m_audioEnabled = enableAudio;
	}

	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override
	{
		const char* name = event.m_EventName ? event.m_EventName : "";
		if (stricmp(name, "footstep") == 0)
		{
			SMFXRunTimeEffectParams params;
			params.pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);
			params.angle = 0;

			IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
			TMFXEffectId effectId = InvalidEffectId;

			if (pMaterialEffects)
			{
				const char* effectName = (event.m_CustomParameter && event.m_CustomParameter[0]) ? event.m_CustomParameter : m_defaultFootstepEffect.c_str();
				effectId = pMaterialEffects->GetEffectIdByName(m_footstepLibrary.c_str(), effectName);
			}

			if (effectId != InvalidEffectId)
				pMaterialEffects->ExecuteEffect(effectId, params);
			else
				gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find material for footstep sounds");
			return true;
		}
		else if (stricmp(name, "groundEffect") == 0)
		{
			// setup sound params
			SMFXRunTimeEffectParams params;
			params.pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);
			params.angle = 0;

			IMaterialEffects* materialEffects = gEnv->pMaterialEffects;
			TMFXEffectId effectId = InvalidEffectId;
			if (materialEffects)
				effectId = materialEffects->GetEffectIdByName(event.m_CustomParameter, "default");

			if (effectId != 0)
				materialEffects->ExecuteEffect(effectId, params);
			else
				gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find material for groundEffect anim event");
			return true;
		}
		else if (stricmp(name, "foley") == 0)
		{
			// setup sound params
			SMFXRunTimeEffectParams params;
			params.angle = 0;
			params.pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);

			IMaterialEffects* materialEffects = gEnv->pMaterialEffects;
			TMFXEffectId effectId = InvalidEffectId;

			// replace with "foley"
			if (materialEffects)
			{
				effectId = materialEffects->GetEffectIdByName(m_foleyLibrary.c_str(),
				                                              event.m_CustomParameter[0] ? event.m_CustomParameter : "default");

				if (effectId != InvalidEffectId)
					materialEffects->ExecuteEffect(effectId, params);
				else
					gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find effect entry for foley sounds");
			}
			return true;
		}
		return false;
	}

private:
	bool   m_audioEnabled;
	string m_parameter;
	QuatT  m_playerLocation;
	string m_foleyLibrary;
	string m_footstepLibrary;
	string m_defaultFootstepEffect;
};

AnimEventPlayerMaterialEffects::AnimEventPlayerMaterialEffects()
	: m_foleyLibrary("foley_player")
	, m_footstepLibrary("footsteps")
	, m_defaultFootstepEffect("concrete")
{
}

CRYREGISTER_CLASS(AnimEventPlayerMaterialEffects)

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
class AnimEventPlayerAnimFXEvents : public IAnimEventPlayer
{
	struct SAnimFXSource
	{
		string m_soundFXLib;
		string m_enviroment;

		SAnimFXSource()
			: m_soundFXLib("")
			, m_enviroment("")
		{}

		SAnimFXSource(const string& fxLib, const string& enviroment)
			: m_soundFXLib(fxLib)
			, m_enviroment(enviroment)
		{}

		void Serialize(Serialization::IArchive& ar)
		{
			Serialization::StringList soundFXLibsList;

			if (IAnimFXEvents* pAnimFXEvents = gEnv->pMaterialEffects->GetAnimFXEvents())
			{
				uint32 index = 0;
				for (uint32 i = 0; i < pAnimFXEvents->GetLibrariesCount(); ++i)
				{
					const char* soundLibraryName = pAnimFXEvents->GetLibraryName(i);
					soundFXLibsList.push_back(soundLibraryName);
				}

				int foundIdx = soundFXLibsList.find(m_soundFXLib.c_str());
				index = foundIdx == Serialization::StringList::npos ? 0 : uint32(foundIdx);

				Serialization::StringListValue eventListChoice(soundFXLibsList, index);

				ar(eventListChoice, "animFxLib", "^");

				if (ISurfaceTypeEnumerator* pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator())
				{
					Serialization::StringList envList;
					for (ISurfaceType* pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
					{
						stl::push_back_unique(envList, pSurfaceType->GetType());
					}

					foundIdx = envList.find(m_enviroment.c_str());
					index = foundIdx == Serialization::StringList::npos ? 0 : uint32(foundIdx);

					Serialization::StringListValue envListChoice(envList, index);
					ar(envListChoice, "enviroment", "Enviroment");
					m_enviroment = envListChoice.c_str();
				}

				m_soundFXLib = eventListChoice.c_str();
			}
		}
	};

	typedef std::vector<SCustomAnimEventType>    TCustomEventTypes;
	typedef std::vector<TSerializeParameterFunc> TSerializeParamFuncs;
	typedef std::vector<SAnimFXSource>           TAnimFxSources;

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimEventPlayer)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayerAnimFXEvents, "AnimEventPlayer_AnimFXEvents", "9688f293-04f3-400c-886c-2d31c5199481"_cry_guid)

	AnimEventPlayerAnimFXEvents();
	virtual ~AnimEventPlayerAnimFXEvents() {}

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override
	{
		int count = GetCustomTypeCount();
		if (customTypeIndex < 0 || customTypeIndex >= count)
			return 0;
		return &m_eventTypes[customTypeIndex];
	}

	int GetCustomTypeCount() const override
	{
		return m_eventTypes.size();
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		ar(m_animFxSources, "animFxSources", gEnv->pMaterialEffects->GetAnimFXEvents() ? "Libraries" : "!Libraries");
		ar.doc("AnimFX libraries provided by the game project.");
	}

	const char* SerializeCustomParameter(const char* parameterValue, Serialization::IArchive& ar, int customTypeIndex) override
	{
		if (customTypeIndex < 0 || customTypeIndex >= GetCustomTypeCount())
			return "";
		m_parameter = parameterValue;
		m_eventSerializeFuncs[customTypeIndex](m_parameter, ar);
		return m_parameter.c_str();
	}

	void Update(ICharacterInstance* character, const QuatT& playerLocation, const Matrix34& cameraMatrix) override
	{
		m_playerLocation = playerLocation;
	}

	void EnableAudio(bool enableAudio) override
	{
		m_audioEnabled = enableAudio;
	}

	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override
	{
		// Only handle anim_fx events here
		if (strcmpi(event.m_EventName, "anim_fx"))
		{
			return false;
		}

		SMFXRunTimeEffectParams params;
		params.pos = GetBonePosition(character, m_playerLocation, event.m_BonePathName);
		params.angle = 0;

		int eventUid(0);
		int eventNameUid(0);
		sscanf_s(event.m_CustomParameter, "%d %d", &eventUid, &eventNameUid);

		TMFXEffectId effectId = InvalidEffectId;

		if (IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects)
		{
			SAnimFXEventInfo eventType;
			stack_string mfxLibrary;
			stack_string subType("default");

			if (IAnimFXEvents* pAnimFXEvents = pMaterialEffects->GetAnimFXEvents())
			{
				pAnimFXEvents->GetAnimFXEventInfoById(uint32(eventUid), eventType);
				bool effectFound(false);

				for (const SAnimFXSource& animFxSource : m_animFxSources)
				{
					mfxLibrary.Format("%s/%d/%d", animFxSource.m_soundFXLib.c_str(), eventUid, eventNameUid);
					subType = animFxSource.m_enviroment.c_str();

					// Check for enviroment effect
					effectId = pMaterialEffects->GetEffectIdByName(mfxLibrary.c_str(), subType.c_str());

					// Search for regular effect
					if (effectId == InvalidEffectId)
					{
						mfxLibrary.Format("%s/%d/0", animFxSource.m_soundFXLib.c_str(), eventUid);
						subType = pAnimFXEvents->GetAnimFXEventParamName(eventUid, eventNameUid);

						effectId = pMaterialEffects->GetEffectIdByName(mfxLibrary.c_str(), subType.c_str());
					}

					effectFound = (effectId != InvalidEffectId) || effectFound;
					if (effectId != InvalidEffectId)
					{
						pMaterialEffects->ExecuteEffect(effectId, params);
					}
				}

				if (!effectFound)
				{
					for (const SAnimFXSource& animFxSource : m_animFxSources)
					{
						const char* szFullEventName = pAnimFXEvents->GetAnimFXEventParamName(eventUid, eventNameUid);
						if (!eventType.szName)
						{
							gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find sound event (UID %d/%d) in environment (%s) and definition in (%s).", eventUid, eventNameUid, animFxSource.m_enviroment.c_str(), animFxSource.m_soundFXLib.c_str());
						}
						else if (!szFullEventName)
						{
							gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find sound event (%s/ UID %d) in environment (%s) and definition in (%s).", eventType.szName, eventNameUid, animFxSource.m_enviroment.c_str(), animFxSource.m_soundFXLib.c_str());
						}
						else
						{
							gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find sound event (%s/%s) in environment (%s) and definition in (%s).", eventType.szName, szFullEventName, animFxSource.m_enviroment.c_str(), animFxSource.m_soundFXLib.c_str());
						}
					}
				}
			}
			return true;
		}

		return false;
	}

private:
	bool                 m_audioEnabled;
	TAnimFxSources       m_animFxSources;
	string               m_parameter;
	QuatT                m_playerLocation;
	TCustomEventTypes    m_eventTypes;
	TSerializeParamFuncs m_eventSerializeFuncs;
};

AnimEventPlayerAnimFXEvents::AnimEventPlayerAnimFXEvents()
{
	m_eventTypes.push_back({ "anim_fx", ANIM_EVENT_USES_BONE, "Animation fx's are defined by designers (on the game side) and are executed through the Material FX System." });
	m_eventSerializeFuncs.push_back(&SerializeParameterAnimFXEvent);
}

CRYREGISTER_CLASS(AnimEventPlayerAnimFXEvents)

// ---------------------------------------------------------------------------

static SCustomAnimEventType g_particleEvents[] = {
	{ "effect", ANIM_EVENT_USES_BONE | ANIM_EVENT_USES_OFFSET_AND_DIRECTION, "Spawns a particle effect." }
};

TSerializeParameterFunc g_particleEventFuncitons[sizeof(g_particleEvents) / sizeof(g_particleEvents[0])] = {
	&SerializeParameterEffect,
};

class AnimEventPlayer_Particles : public IAnimEventPlayer
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimEventPlayer)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayer_Particles, "AnimEventPlayer_Particles", "a9fef72d-f304-dec4-a916-2a6e4ac635b0"_cry_guid)

	AnimEventPlayer_Particles();
	virtual ~AnimEventPlayer_Particles() {}

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override
	{
		int count = GetCustomTypeCount();
		if (customTypeIndex < 0 || customTypeIndex >= count)
			return 0;
		return &g_particleEvents[customTypeIndex];
	}

	void Initialize() override
	{
		m_effectPlayer.reset(new EffectPlayer());
	}

	int GetCustomTypeCount() const override
	{
		return sizeof(g_particleEvents) / sizeof(g_particleEvents[0]);
	}

	void Serialize(Serialization::IArchive& ar) override
	{
		ar.doc("Spawns a particle effect for each \"effect\" AnimEvent.");

		ar(m_enableRendering, "enableRendering", "Enable Rendering");
		ar.doc("Enables rendering of particle effects in the preview viewport.");
	}

	const char* SerializeCustomParameter(const char* parameterValue, Serialization::IArchive& ar, int customTypeIndex) override
	{
		if (customTypeIndex < 0 || customTypeIndex >= GetCustomTypeCount())
			return "";
		m_parameter = parameterValue;
		g_particleEventFuncitons[customTypeIndex](m_parameter, ar);
		return m_parameter.c_str();
	}

	void Reset() override
	{
		if (m_effectPlayer)
			m_effectPlayer->KillAllEffects();
	}

	void Render(const QuatT& playerPosition, SRendParams& params, const SRenderingPassInfo& passInfo) override
	{
		if (m_effectPlayer && m_enableRendering)
		{
			m_effectPlayer->Render(params, passInfo);
		}
	}

	void Update(ICharacterInstance* character, const QuatT& playerLocation, const Matrix34& cameraMatrix) override
	{
		if (m_effectPlayer)
		{
			m_effectPlayer->SetSkeleton(character->GetISkeletonAnim(), character->GetISkeletonPose(), &character->GetIDefaultSkeleton());
			m_effectPlayer->Update(playerLocation);
		}
	}

	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override
	{
		if (stricmp(event.m_EventName, "effect") == 0)
		{
			if (m_effectPlayer)
			{
				m_effectPlayer->SetSkeleton(character->GetISkeletonAnim(), character->GetISkeletonPose(), &character->GetIDefaultSkeleton());
				m_effectPlayer->SpawnEffect(event.m_CustomParameter, event.m_BonePathName, event.m_vOffset, event.m_vDir);
			}
			return true;
		}
		return false;
	}

private:
	string                        m_parameter;
	std::unique_ptr<EffectPlayer> m_effectPlayer;
	bool                          m_enableRendering;
};

AnimEventPlayer_Particles::AnimEventPlayer_Particles()
	: m_parameter()
	, m_effectPlayer()
	, m_enableRendering(true)
{
}

CRYREGISTER_CLASS(AnimEventPlayer_Particles)

// ---------------------------------------------------------------------------

AnimEventPlayer_CharacterTool::AnimEventPlayer_CharacterTool()
{
	IAnimEventPlayerPtr defaultGamePlayer;
	if (CryCreateClassInstance<IAnimEventPlayer>(cryiidof<IAnimEventPlayerGame>(), defaultGamePlayer))
	{
		m_list.push_back(defaultGamePlayer);
		m_names.push_back(defaultGamePlayer->GetFactory()->GetName());
	}

	IAnimEventPlayerPtr player;
	if (::CryCreateClassInstance<IAnimEventPlayer>(CharacterTool::AnimEventPlayer_AudioTranslationLayer::GetCID(), player))
	{
		m_list.push_back(player);
		m_names.push_back("Audio Translation Layer");
	}
	if (::CryCreateClassInstance<IAnimEventPlayer>(CharacterTool::AnimEventPlayerMaterialEffects::GetCID(), player))
	{
		m_list.push_back(player);
		m_names.push_back("Material Effects");
	}
	if (::CryCreateClassInstance<IAnimEventPlayer>(CharacterTool::AnimEventPlayer_Particles::GetCID(), player))
	{
		m_list.push_back(player);
		m_names.push_back("Particles");
	}
	if (::CryCreateClassInstance<IAnimEventPlayer>(CharacterTool::AnimEventPlayerAnimFXEvents::GetCID(), player))
	{
		m_list.push_back(player);
		m_names.push_back("AnimFX");
	}
}

CRYREGISTER_CLASS(AnimEventPlayer_CharacterTool)

}

