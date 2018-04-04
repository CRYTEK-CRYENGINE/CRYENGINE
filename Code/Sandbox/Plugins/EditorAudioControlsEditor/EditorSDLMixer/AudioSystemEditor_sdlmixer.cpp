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
#include <CryAudio/IProfileData.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
const string CAudioSystemEditor_sdlmixer::s_eventConnectionTag = "SDLMixerEvent";
const string CAudioSystemEditor_sdlmixer::s_sampleConnectionTag = "SDLMixerSample";
const string CAudioSystemEditor_sdlmixer::s_itemNameTag = "sdl_name";
const string CAudioSystemEditor_sdlmixer::s_pathNameTag = "sdl_path";
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
	m_controlsCache.reserve(1024);
}

CAudioSystemEditor_sdlmixer::~CAudioSystemEditor_sdlmixer()
{
	Clear();
}

void CAudioSystemEditor_sdlmixer::Reload(bool bPreserveConnectionStatus)
{
	Clear();

	CSdlMixerProjectLoader(m_settings.GetProjectPath(), m_root);
	CreateControlCache(&m_root);

	for (auto const& controlPair : m_connectionsByID)
	{
		if (!controlPair.second.empty())
		{
			IAudioSystemItem* const pControl = GetControl(controlPair.first);

			if (pControl != nullptr)
			{
				pControl->SetConnected(true);
			}
		}
	}
}

void CAudioSystemEditor_sdlmixer::Clear()
{
	// Delete all the controls
	for (auto const pControl : m_controlsCache)
	{
		delete pControl;
	}

	m_controlsCache.clear();

	// Clean up the root control
	m_root = IAudioSystemItem();
}

void CAudioSystemEditor_sdlmixer::CreateControlCache(IAudioSystemItem* pParent)
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

IAudioSystemItem* CAudioSystemEditor_sdlmixer::GetControl(CID id) const
{
	for (auto const pItem : m_controlsCache)
	{
		if (pItem->GetId() == id)
		{
			return pItem;
		}
	}

	return nullptr;
}

ConnectionPtr CAudioSystemEditor_sdlmixer::CreateConnectionToControl(EItemType eATLControlType, IAudioSystemItem* pMiddlewareControl)
{
	std::shared_ptr<CSdlMixerConnection> pConnection = std::make_shared<CSdlMixerConnection>(pMiddlewareControl->GetId());
	pMiddlewareControl->SetConnected(true);
	m_connectionsByID[pMiddlewareControl->GetId()].push_back(pConnection);
	return pConnection;
}

ConnectionPtr CAudioSystemEditor_sdlmixer::CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType eATLControlType)
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

			IAudioSystemItem* pItem = GetControl(id);

			if (pItem == nullptr)
			{
				pItem = new IAudioSystemControl_sdlmixer(name, id, eSdlMixerTypes_Event);
				pItem->SetPlaceholder(true);
				m_controlsCache.push_back(pItem);
			}

			if (pItem != nullptr)
			{
				SdlConnectionPtr pConnection = std::make_shared<CSdlMixerConnection>(pItem->GetId());
				pItem->SetConnected(true);
				m_connectionsByID[pItem->GetId()].push_back(pConnection);
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
	}
	return nullptr;
}

XmlNodeRef CAudioSystemEditor_sdlmixer::CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType eATLControlType)
{
	std::shared_ptr<const CSdlMixerConnection> pSDLMixerConnection = std::static_pointer_cast<const CSdlMixerConnection>(pConnection);
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

void CAudioSystemEditor_sdlmixer::EnableConnection(ConnectionPtr pConnection)
{
	IAudioSystemItem* const pItem = GetControl(pConnection->GetID());

	if (pItem != nullptr)
	{
		CSdlMixerConnection const* const pConn = static_cast<CSdlMixerConnection*>(pConnection.get());
		pItem->SetRadius(pConn->maxAttenuation);
	}
}

void CAudioSystemEditor_sdlmixer::DisableConnection(ConnectionPtr pConnection)
{
	IAudioSystemItem* const pItem = GetControl(pConnection->GetID());

	if (pItem != nullptr)
	{
		CSdlMixerConnection const* const pConn = static_cast<CSdlMixerConnection*>(pConnection.get());
		pItem->SetRadius(0.0f);
	}
}

ACE::CID CAudioSystemEditor_sdlmixer::GetId(const string& sName) const
{
	return CCrc32::ComputeLowercase(sName);
}

const char* CAudioSystemEditor_sdlmixer::GetTypeIcon(ItemType type) const
{
	switch (type)
	{
	case eSdlMixerTypes_Event:
		return "Editor/Icons/audio/sdl_mixer/Audio_Event.png";
	case eSdlMixerTypes_Folder:
		return "Editor/Icons/audio/sdl_mixer/Folder.ico";
	}
	return "Editor/Icons/audio/sdl_mixer/Audio_Event.png";
}

ACE::EItemType CAudioSystemEditor_sdlmixer::ImplTypeToATLType(ItemType type) const
{
	switch (type)
	{
	case eSdlMixerTypes_Event:
		return eItemType_Trigger;
	}
	return eItemType_Invalid;
}

ACE::TImplControlTypeMask CAudioSystemEditor_sdlmixer::GetCompatibleTypes(EItemType eATLControlType) const
{
	switch (eATLControlType)
	{
	case eItemType_Trigger:
		return eSdlMixerTypes_Event;
	}
	return AUDIO_SYSTEM_INVALID_TYPE;
}

string CAudioSystemEditor_sdlmixer::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
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

void CSdlMixerConnection::Serialize(Serialization::IArchive& ar)
{
	ar(type, "action", "Action");
	ar(bPanningEnabled, "panning", "Enable Panning");

	if (ar.openBlock("DistanceAttenuation", "+Distance Attenuation"))
	{
		ar(bAttenuationEnabled, "attenuation", "Enable");
		if (bAttenuationEnabled)
		{
			if (ar.isInput())
			{
				float minAtt = minAttenuation;
				float maxAtt = maxAttenuation;
				ar(minAtt, "min_att", "Min Distance");
				ar(maxAtt, "max_att", "Max Distance");

				if (minAtt > maxAtt)
				{
					if (minAtt != minAttenuation)
					{
						maxAtt = minAtt;
					}
					else
					{
						minAtt = maxAtt;
					}
				}

				minAttenuation = minAtt;
				maxAttenuation = maxAtt;

				IAudioSystemItem* const pItem = s_pSdlMixerInterface->GetControl(GetID());

				if (pItem != nullptr)
				{
					pItem->SetRadius(maxAttenuation);
				}
			}
			else
			{
				ar(minAttenuation, "min_att", "Min Distance");
				ar(maxAttenuation, "max_att", "Max Distance");
			}
		}
		else
		{
			ar(minAttenuation, "min_att", "!Min Distance");
			ar(maxAttenuation, "max_att", "!Max Distance");
		}
		ar.closeBlock();
	}

	ar(Serialization::Range(volume, -96.0f, 0.0f), "vol", "Volume (dB)");

	if (ar.openBlock("Looping", "+Looping"))
	{
		ar(bInfiniteLoop, "infinite", "Infinite");
		if (bInfiniteLoop)
		{
			ar(loopCount, "loop_count", "!Count");
		}
		else
		{
			ar(loopCount, "loop_count", "Count");
		}
		ar.closeBlock();
	}

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}
} // namespace ACE
