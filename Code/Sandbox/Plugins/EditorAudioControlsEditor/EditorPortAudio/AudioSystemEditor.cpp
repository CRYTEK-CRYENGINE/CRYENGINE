// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "AudioSystemEditor.h"
#include "ProjectLoader.h"
#include "AudioSystemControl.h"
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryCore/CryCrc32.h>
#include <CryString/CryPath.h>
#include <CryCore/StlUtils.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IProfileData.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
const string CAudioSystemEditor::s_eventConnectionTag = "PortAudioEvent";
const string CAudioSystemEditor::s_sampleConnectionTag = "PortAudioSample";
const string CAudioSystemEditor::s_itemNameTag = "portaudio_name";
const string CAudioSystemEditor::s_pathNameTag = "portaudio_path";
const string CAudioSystemEditor::s_panningEnabledTag = "enable_panning";
const string CAudioSystemEditor::s_attenuationEnabledTag = "enable_distance_attenuation";
const string CAudioSystemEditor::s_attenuationDistMin = "attenuation_dist_min";
const string CAudioSystemEditor::s_attenuationDistMax = "attenuation_dist_max";
const string CAudioSystemEditor::s_volumeTag = "volume";
const string CAudioSystemEditor::s_loopCountTag = "loop_count";
const string CAudioSystemEditor::s_connectionTypeTag = "event_type";

const string g_userSettingsFile = "%USER%/audiocontrolseditor_portaudio.user";

CAudioSystemEditor::CAudioSystemEditor()
{
	Serialization::LoadJsonFile(m_settings, g_userSettingsFile.c_str());
}

CAudioSystemEditor::~CAudioSystemEditor()
{
	Clear();
}

void CAudioSystemEditor::Reload(bool bPreserveConnectionStatus)
{
	Clear();

	CProjectLoader(m_settings.GetProjectPath(), m_root);
	CreateControlCache(&m_root);

	for (auto controlPair : m_connectionsByID)
	{
		if (controlPair.second.size() > 0)
		{
			IAudioSystemItem* pControl = GetControl(controlPair.first);
			if (pControl)
			{
				pControl->SetConnected(true);
			}
		}
	}
}

void CAudioSystemEditor::Clear()
{
	// Delete all the controls
	for (IAudioSystemItem* pControl : m_controlsCache)
	{
		delete pControl;
	}
	m_controlsCache.clear();

	// Clean up the root control
	m_root = IAudioSystemItem();
}

void CAudioSystemEditor::CreateControlCache(IAudioSystemItem* pParent)
{
	if (pParent)
	{
		const size_t count = pParent->ChildCount();
		for (size_t i = 0; i < count; ++i)
		{
			IAudioSystemItem* pChild = pParent->GetChildAt(i);
			if (pChild)
			{
				m_controlsCache.push_back(pChild);
				CreateControlCache(pChild);
			}
		}
	}
}

IAudioSystemItem* CAudioSystemEditor::GetControl(CID id) const
{
	if (id >= 0)
	{
		size_t size = m_controlsCache.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (m_controlsCache[i]->GetId() == id)
			{
				return m_controlsCache[i];
			}
		}
	}
	return nullptr;
}

ConnectionPtr CAudioSystemEditor::CreateConnectionToControl(EItemType eATLControlType, IAudioSystemItem* pMiddlewareControl)
{
	std::shared_ptr<CConnection> pConnection = std::make_shared<CConnection>(pMiddlewareControl->GetId());
	pMiddlewareControl->SetConnected(true);
	m_connectionsByID[pMiddlewareControl->GetId()].push_back(pConnection);
	return pConnection;
}

ConnectionPtr CAudioSystemEditor::CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType eATLControlType)
{
	if (pNode)
	{
		const string nodeTag = pNode->getTag();
		if (nodeTag == s_eventConnectionTag || nodeTag == s_sampleConnectionTag)
		{
			const string name = pNode->getAttr(s_itemNameTag);
			const string path = pNode->getAttr(s_pathNameTag);

			CID id;
			if (path.empty())
			{
				id = GetId(name);
			}
			else
			{
				id = GetId(path + CRY_NATIVE_PATH_SEPSTR + name);
			}

			IAudioSystemItem* pControl = GetControl(id);
			if (!pControl)
			{
				pControl = new IAudioSystemControl(name, id, ePortAudioTypes_Event);
				pControl->SetPlaceholder(true);
				m_controlsCache.push_back(pControl);
			}

			if (pControl)
			{
				PortAudioConnectionPtr pConnection = std::make_shared<CConnection>(pControl->GetId());
				pControl->SetConnected(true);
				m_connectionsByID[pControl->GetId()].push_back(pConnection);
				const string connectionType = pNode->getAttr(s_connectionTypeTag);
				pConnection->type = connectionType == "stop" ? ePortAudioConnectionType_Stop : ePortAudioConnectionType_Start;

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
	}
	return nullptr;
}

XmlNodeRef CAudioSystemEditor::CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType eATLControlType)
{
	std::shared_ptr<const CConnection> pSDLMixerConnection = std::static_pointer_cast<const CConnection>(pConnection);
	const IAudioSystemItem* pControl = GetControl(pConnection->GetID());
	if (pControl && pSDLMixerConnection && eATLControlType == eItemType_Trigger)
	{
		XmlNodeRef pConnectionNode = GetISystem()->CreateXmlNode(s_eventConnectionTag);
		pConnectionNode->setAttr(s_itemNameTag, pControl->GetName());

		string path;
		const IAudioSystemItem* pParent = pControl->GetParent();
		while (pParent)
		{
			const string parentName = pParent->GetName();
			if (!parentName.empty())
			{
				if (path.empty())
				{
					path = parentName;
				}
				else
				{
					path = parentName + CRY_NATIVE_PATH_SEPSTR + path;
				}
			}
			pParent = pParent->GetParent();
		}

		pConnectionNode->setAttr(s_pathNameTag, path);

		if (pSDLMixerConnection->type == ePortAudioConnectionType_Start)
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

ACE::CID CAudioSystemEditor::GetId(const string& sName) const
{
	return CCrc32::ComputeLowercase(sName);
}

const char* CAudioSystemEditor::GetTypeIcon(ItemType type) const
{
	switch (type)
	{
	case ePortAudioTypes_Event:
		return "Editor/Icons/audio/portaudio/Audio_Event.png";
	case ePortAudioTypes_Folder:
		return "Editor/Icons/audio/portaudio/Folder.ico";
	}
	return "Editor/Icons/audio/portaudio/Audio_Event.png";
}

ACE::EItemType CAudioSystemEditor::ImplTypeToATLType(ItemType type) const
{
	switch (type)
	{
	case ePortAudioTypes_Event:
		return eItemType_Trigger;
	}
	return eItemType_Invalid;
}

ACE::TImplControlTypeMask CAudioSystemEditor::GetCompatibleTypes(EItemType eATLControlType) const
{
	switch (eATLControlType)
	{
	case eItemType_Trigger:
		return ePortAudioTypes_Event;
	}
	return AUDIO_SYSTEM_INVALID_TYPE;
}

string CAudioSystemEditor::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
}

void CImplementationSettings::SetProjectPath(const char* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

SERIALIZATION_ENUM_BEGIN(EPortAudioConnectionType, "Event Type")
SERIALIZATION_ENUM(ePortAudioConnectionType_Start, "start", "Start")
SERIALIZATION_ENUM(ePortAudioConnectionType_Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()

}
