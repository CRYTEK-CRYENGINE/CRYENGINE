// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MFXAudioEffect.h"
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IObject.h>

namespace MaterialEffectsUtils
{
	static constexpr CryAudio::ControlId switchId = CryAudio::StringToId("1stOr3rdP");
	static constexpr CryAudio::SwitchStateId fpStateId = CryAudio::StringToId("1stP");
	static constexpr CryAudio::SwitchStateId tpStateId = CryAudio::StringToId("3rdP");

template<typename AudioObjectType>
void PrepareForAudioTriggerExecution(AudioObjectType* pIAudioObject, const SMFXAudioEffectParams& audioParams, const SMFXRunTimeEffectParams& runtimeParams)
{
	pIAudioObject->SetSwitchState(switchId,	runtimeParams.playSoundFP ? fpStateId : tpStateId);

	for (auto const& switchWrapper : audioParams.triggerSwitches)
	{
		pIAudioObject->SetSwitchState(switchWrapper.GetSwitchId(), switchWrapper.GetSwitchStateId());
	}

	for (uint32 i = 0; i < runtimeParams.numAudioRtpcs; ++i)
	{
		char const* const szParameterName = runtimeParams.audioRtpcs[i].rtpcName;

		if (szParameterName != nullptr && szParameterName[0] != '\0')
		{
			CryAudio::ControlId const parameterId = CryAudio::StringToId(szParameterName);
			pIAudioObject->SetParameter(parameterId, runtimeParams.audioRtpcs[i].rtpcValue);
		}
	}
}
} // namespace MaterialEffectsUtils

//////////////////////////////////////////////////////////////////////////

void SAudioTriggerWrapper::Init(const char* triggerName)
{
	CRY_ASSERT(triggerName != nullptr);
	m_triggerID = CryAudio::StringToId(triggerName);

#if defined(MATERIAL_EFFECTS_DEBUG)
	m_triggerName = triggerName;
#endif
}

void SAudioSwitchWrapper::Init(const char* switchName, const char* switchStateName)
{
	CRY_ASSERT(switchName != nullptr);
	CRY_ASSERT(switchStateName != nullptr);
	m_switchID = CryAudio::StringToId(switchName);
	m_switchStateID = CryAudio::StringToId(switchStateName);

#if defined(MATERIAL_EFFECTS_DEBUG)
	m_switchName = switchName;
	m_switchStateName = switchStateName;
#endif
}

//////////////////////////////////////////////////////////////////////////

CMFXAudioEffect::CMFXAudioEffect()
	: CMFXEffectBase(eMFXPF_Audio)
{

}

void CMFXAudioEffect::Execute(const SMFXRunTimeEffectParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IF_UNLIKELY (!m_audioParams.trigger.IsValid())
		return;

	IEntity* pOwnerEntity = (params.audioProxyEntityId != 0) ? gEnv->pEntitySystem->GetEntity(params.audioProxyEntityId) : nullptr;
	if (pOwnerEntity)
	{
		IEntityAudioComponent* pIEntityAudioComponent = pOwnerEntity->GetOrCreateComponent<IEntityAudioComponent>();
		CRY_ASSERT(pIEntityAudioComponent);

		MaterialEffectsUtils::PrepareForAudioTriggerExecution<IEntityAudioComponent>(pIEntityAudioComponent, m_audioParams, params);

		pIEntityAudioComponent->ExecuteTrigger(m_audioParams.trigger.GetTriggerId(), params.audioProxyId);
	}
	else
	{
		CryAudio::SCreateObjectData const objectData("MFXAudioEffect", CryAudio::EOcclusionType::Low, params.pos, INVALID_ENTITYID, true);
		CryAudio::IObject* const pIObject = gEnv->pAudioSystem->CreateObject(objectData);

		MaterialEffectsUtils::PrepareForAudioTriggerExecution<CryAudio::IObject>(pIObject, m_audioParams, params);

		pIObject->ExecuteTrigger(m_audioParams.trigger.GetTriggerId());
		gEnv->pAudioSystem->ReleaseObject(pIObject);
	}
}

void CMFXAudioEffect::GetResources(SMFXResourceList& resourceList) const
{
	SMFXAudioListNode* pListNode = SMFXAudioListNode::Create();

	pListNode->m_audioParams.triggerName = m_audioParams.trigger.GetTriggerName();

	const size_t switchesCount = std::min<size_t>(m_audioParams.triggerSwitches.size(), pListNode->m_audioParams.triggerSwitches.max_size());

	for (size_t i = 0; i < switchesCount; ++i)
	{
		IMFXAudioParams::SSwitchData switchData;
		switchData.switchName = m_audioParams.triggerSwitches[i].GetSwitchName();
		switchData.switchStateName = m_audioParams.triggerSwitches[i].GetSwitchStateName();

		pListNode->m_audioParams.triggerSwitches.push_back(switchData);
	}

	SMFXAudioListNode* pNextNode = resourceList.m_audioList;

	if (pNextNode == nullptr)
	{
		resourceList.m_audioList = pListNode;
	}
	else
	{
		while (pNextNode->pNext)
			pNextNode = pNextNode->pNext;

		pNextNode->pNext = pListNode;
	}
}

void CMFXAudioEffect::LoadParamsFromXml(const XmlNodeRef& paramsNode)
{
	// Xml data format
	/*
	   <Audio trigger="footstep">
	   <Switch name="Switch1" state="switch1_state" />
	   <Switch name="Switch2" state="swtich2_state" />
	   <Switch ... />
	   </Audio>
	 */

	m_audioParams.trigger.Init(paramsNode->getAttr("trigger"));

	const int childCount = paramsNode->getChildCount();
	m_audioParams.triggerSwitches.reserve(childCount);

	for (int i = 0; i < childCount; ++i)
	{
		const XmlNodeRef& childNode = paramsNode->getChild(i);

		if (strcmp(childNode->getTag(), "Switch") == 0)
		{
			SAudioSwitchWrapper switchWrapper;
			switchWrapper.Init(childNode->getAttr("name"), childNode->getAttr("state"));
			if (switchWrapper.IsValid())
			{
				m_audioParams.triggerSwitches.push_back(switchWrapper);
			}
#if defined(MATERIAL_EFFECTS_DEBUG)
			else
			{
				GameWarning("[MFX] AudioEffect (at line %d) : Switch '%s' or SwitchState '%s' not valid", paramsNode->getLine(), switchWrapper.GetSwitchName(), switchWrapper.GetSwitchStateName());
			}
#endif
		}
	}

#if defined(MATERIAL_EFFECTS_DEBUG)
	if (!m_audioParams.trigger.IsValid())
	{
		GameWarning("[MFX] AudioEffect (at line %d) : Trigger '%s'not valid", paramsNode->getLine(), m_audioParams.trigger.GetTriggerName());
	}
#endif
}

void CMFXAudioEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(&m_audioParams);
}
