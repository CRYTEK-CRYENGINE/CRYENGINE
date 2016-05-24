// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "AudioSystemEditor_sdlmixer.h"
#include "SDLMixerProjectLoader.h"
#include "AudioSystemControl_sdlmixer.h"
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryCore/CryCrc32.h>
#include <CryString/CryPath.h>
#include <CryCore/StlUtils.h>
#include <CryAudio/IAudioSystem.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
const string CAudioSystemEditor_sdlmixer::s_eventConnectionTag = "SDLMixerEvent";
const string CAudioSystemEditor_sdlmixer::s_sampleConnectionTag = "SDLMixerSample";
const string CAudioSystemEditor_sdlmixer::s_controlNameTag = "sdl_name";
const string CAudioSystemEditor_sdlmixer::s_panningEnabledTag = "enable_panning";
const string CAudioSystemEditor_sdlmixer::s_attenuationEnabledTag = "enable_distance_attenuation";
const string CAudioSystemEditor_sdlmixer::s_attenuationDistMin = "attenuation_dist_min";
const string CAudioSystemEditor_sdlmixer::s_attenuationDistMax = "attenuation_dist_max";
const string CAudioSystemEditor_sdlmixer::s_volumeTag = "volume";
const string CAudioSystemEditor_sdlmixer::s_loopCountTag = "loop_count";
const string CAudioSystemEditor_sdlmixer::s_connectionTypeTag = "event_type";

const string g_userSettingsFile = "%USER%/audiocontrolseditor_sdlmixer.user";

CAudioSystemEditor_sdlmixer::CAudioSystemEditor_sdlmixer()
{
	Serialization::LoadJsonFile(m_settings, g_userSettingsFile.c_str());
	Reload();
}

CAudioSystemEditor_sdlmixer::~CAudioSystemEditor_sdlmixer()
{
}

void CAudioSystemEditor_sdlmixer::Reload(bool bPreserveConnectionStatus)
{
	m_controls.clear();
	m_root = IAudioSystemItem();
	ACE::CSdlMixerProjectLoader(m_settings.GetProjectPath(), this);

	SdlMixerConnections::const_iterator it = m_connectionsByID.begin();
	SdlMixerConnections::const_iterator end = m_connectionsByID.end();
	for (; it != end; ++it)
	{
		if (it->second.size() > 0)
		{
			IAudioSystemItem* pControl = GetControl(it->first);
			if (pControl)
			{
				pControl->SetConnected(true);
			}
		}
	}
}

IAudioSystemItem* CAudioSystemEditor_sdlmixer::CreateControl(const SControlDef& controlDefinition)
{
	const CID id = GetId(controlDefinition.name);
	IAudioSystemControl_sdlmixer* pControl = new IAudioSystemControl_sdlmixer(controlDefinition.name, id, controlDefinition.type);
	if (pControl)
	{
		IAudioSystemItem* pParent = controlDefinition.pParent;
		if (pParent == nullptr)
		{
			pParent = &m_root;
		}

		pParent->AddChild(pControl);
		m_controls.push_back(pControl);
	}
	return pControl;
}

IAudioSystemItem* CAudioSystemEditor_sdlmixer::GetControl(CID id) const
{
	if (id >= 0)
	{
		size_t size = m_controls.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (m_controls[i]->GetId() == id)
			{
				return m_controls[i];
			}
		}
	}
	return nullptr;
}

ConnectionPtr CAudioSystemEditor_sdlmixer::CreateConnectionToControl(EACEControlType eATLControlType, IAudioSystemItem* pMiddlewareControl)
{
	std::shared_ptr<CSdlMixerConnection> pConnection = std::make_shared<CSdlMixerConnection>(pMiddlewareControl->GetId());
	pMiddlewareControl->SetConnected(true);
	m_connectionsByID[pMiddlewareControl->GetId()].push_back(pConnection);
	return pConnection;
}

ConnectionPtr CAudioSystemEditor_sdlmixer::CreateConnectionFromXMLNode(XmlNodeRef pNode, EACEControlType eATLControlType)
{
	if (pNode)
	{
		const string nodeTag = pNode->getTag();
		if (nodeTag == s_eventConnectionTag || nodeTag == s_sampleConnectionTag)
		{
			const string name = pNode->getAttr(s_controlNameTag);
			CID id = GetId(name);
			if (id != ACE_INVALID_ID)
			{
				IAudioSystemItem* pControl = GetControl(id);
				if (pControl == nullptr)
				{
					pControl = CreateControl(SControlDef(name, eSdlMixerTypes_Event));
					if (pControl)
					{
						pControl->SetPlaceholder(true);
					}
				}

				if (pControl)
				{
					SdlConnectionPtr pConnection = std::make_shared<CSdlMixerConnection>(pControl->GetId());
					pControl->SetConnected(true);
					m_connectionsByID[pControl->GetId()].push_back(pConnection);

					const string connectionType = pNode->getAttr(s_connectionTypeTag);
					pConnection->type = connectionType == "stop" ? eSdlMixerConnectionType_Stop : eSdlMixerConnectionType_Start;

					const string enablePanning = pNode->getAttr(s_panningEnabledTag);
					pConnection->bPanningEnabled = enablePanning == "true" ? true : false;

					const string enableDistAttenuation = pNode->getAttr(s_attenuationEnabledTag);
					pConnection->bAttenuationEnabled = enableDistAttenuation == "true" ? true : false;

					pNode->getAttr(s_attenuationDistMin, pConnection->minAttenuation);
					pNode->getAttr(s_attenuationDistMax, pConnection->maxAttenuation);
					pNode->getAttr(s_volumeTag, pConnection->volume);
					pNode->getAttr(s_loopCountTag, pConnection->loopCount);

					if (pConnection->loopCount == -1)
					{
						pConnection->bInfiniteLoop = true;
					}

					return pConnection;
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] [SDL Mixer] Error reading connection to %s", name.c_str());
			}
		}
	}
	return nullptr;
}

XmlNodeRef CAudioSystemEditor_sdlmixer::CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EACEControlType eATLControlType)
{
	std::shared_ptr<const CSdlMixerConnection> pSDLMixerConnection = std::static_pointer_cast<const CSdlMixerConnection>(pConnection);
	const IAudioSystemItem* pControl = GetControl(pConnection->GetID());
	if (pControl && pSDLMixerConnection && eATLControlType == eACEControlType_Trigger)
	{
		XmlNodeRef pConnectionNode = GetISystem()->CreateXmlNode(s_eventConnectionTag);
		pConnectionNode->setAttr(s_controlNameTag, pControl->GetName());

		if (pSDLMixerConnection->type == eSdlMixerConnectionType_Start)
		{
			pConnectionNode->setAttr(s_connectionTypeTag, "start");
			pConnectionNode->setAttr(s_panningEnabledTag, pSDLMixerConnection->bPanningEnabled ? "true" : "false");
			pConnectionNode->setAttr(s_attenuationEnabledTag, pSDLMixerConnection->bAttenuationEnabled ? "true" : "false");
			pConnectionNode->setAttr(s_attenuationDistMin, pSDLMixerConnection->minAttenuation);
			pConnectionNode->setAttr(s_attenuationDistMax, pSDLMixerConnection->maxAttenuation);
			pConnectionNode->setAttr(s_volumeTag, pSDLMixerConnection->volume);
			if (pSDLMixerConnection->bInfiniteLoop)
			{
				pConnectionNode->setAttr(s_loopCountTag, -1);
			}
			else
			{
				pConnectionNode->setAttr(s_loopCountTag, pSDLMixerConnection->loopCount);
			}
		}
		else
		{
			pConnectionNode->setAttr(s_connectionTypeTag, "stop");
		}

		return pConnectionNode;
	}
	return nullptr;
}

ACE::CID CAudioSystemEditor_sdlmixer::GetId(const string& sName) const
{
	return CCrc32::Compute(sName);
}

const char* CAudioSystemEditor_sdlmixer::GetTypeIcon(ItemType type) const
{
	return "Editor/Icons/audio/sdl_mixer/Audio_Event.png";
}

ACE::EACEControlType CAudioSystemEditor_sdlmixer::ImplTypeToATLType(ItemType type) const
{
	return eACEControlType_Trigger;
}

ACE::TImplControlTypeMask CAudioSystemEditor_sdlmixer::GetCompatibleTypes(EACEControlType eATLControlType) const
{
	switch (eATLControlType)
	{
	case eACEControlType_Trigger:
		return eSdlMixerTypes_Event;
	}
	return AUDIO_SYSTEM_INVALID_TYPE;
}

string CAudioSystemEditor_sdlmixer::GetName() const
{
	return "SDL Mixer";
}

void CImplementationSettings_sdlmixer::SetProjectPath(const char* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

SERIALIZATION_ENUM_BEGIN(ESdlMixerConnectionType, "Event Type")
SERIALIZATION_ENUM(eSdlMixerConnectionType_Start, "start", "Start")
SERIALIZATION_ENUM(eSdlMixerConnectionType_Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()

}
