// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include "ImplConnections.h"
#include "ProjectLoader.h"

#include <CrySystem/ISystem.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/StlUtils.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IProfileData.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/ClassFactory.h>

namespace ACE
{
namespace SDLMixer
{
string const CEditorImpl::s_eventConnectionTag = "SDLMixerEvent";
string const CEditorImpl::s_sampleConnectionTag = "SDLMixerSample";
string const CEditorImpl::s_itemNameTag = "sdl_name";
string const CEditorImpl::s_pathNameTag = "sdl_path";
string const CEditorImpl::s_panningEnabledTag = "enable_panning";
string const CEditorImpl::s_attenuationEnabledTag = "enable_distance_attenuation";
string const CEditorImpl::s_attenuationDistMin = "attenuation_dist_min";
string const CEditorImpl::s_attenuationDistMax = "attenuation_dist_max";
string const CEditorImpl::s_volumeTag = "volume";
string const CEditorImpl::s_loopCountTag = "loop_count";
string const CEditorImpl::s_connectionTypeTag = "event_type";

string const g_userSettingsFile = "%USER%/audiocontrolseditor_sdlmixer.user";

//////////////////////////////////////////////////////////////////////////
void CImplSettings::SetProjectPath(char const* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	Serialization::LoadJsonFile(m_implSettings, g_userSettingsFile.c_str());
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(GetSettings()->GetProjectPath(), m_rootControl);

	CreateControlCache(&m_rootControl);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				CImplItem* const pImplControl = GetControl(connection.first);

				if (pImplControl != nullptr)
				{
					pImplControl->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetControl(CID const id) const
{
	if (id >= 0)
	{
		return stl::find_in_map(m_controlsCache, id, nullptr);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
char const* CEditorImpl::GetTypeIcon(CImplItem const* const pImplItem) const
{
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Event:
		return "icons:audio/sdlmixer/event.ico";
	case EImpltemType::Folder:
		return "icons:General/Folder.ico";
	}
	return "icons:Dialogs/dialog-error.ico";
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTypeCompatible(ESystemItemType const systemType, CImplItem const* const pImplItem) const
{
	auto const implType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (systemType)
	{
	case ESystemItemType::Trigger:
		return implType == EImpltemType::Event;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
ESystemItemType CEditorImpl::ImplTypeToSystemType(CImplItem const* const pImplItem) const
{
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Event:
		return ESystemItemType::Trigger;
	}
	return ESystemItemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionToControl(ESystemItemType const controlType, CImplItem* const pImplItem)
{
	if (pImplItem != nullptr)
	{
		return std::make_shared<CConnection>(pImplItem->GetId());
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, ESystemItemType const controlType)
{
	if (pNode != nullptr)
	{
		string const nodeTag = pNode->getTag();

		if ((nodeTag == s_eventConnectionTag) || (nodeTag == s_sampleConnectionTag))
		{
			string const name = pNode->getAttr(s_itemNameTag);
			string const path = pNode->getAttr(s_pathNameTag);

			CID id;

			if (path.empty())
			{
				id = GetId(name);
			}
			else
			{
				id = GetId(path + CRY_NATIVE_PATH_SEPSTR + name);
			}

			CImplItem* pImplControl = GetControl(id);

			if (pImplControl == nullptr)
			{
				pImplControl = new CImplControl(name, id, static_cast<ItemType>(EImpltemType::Event));
				pImplControl->SetPlaceholder(true);
				m_controlsCache[id] = pImplControl;
			}

			if (pImplControl != nullptr)
			{
				auto const pConnection = std::make_shared<CConnection>(pImplControl->GetId());
				string const connectionType = pNode->getAttr(s_connectionTypeTag);
				pConnection->m_type = connectionType == "stop" ? EConnectionType::Stop : EConnectionType::Start;

				string const enablePanning = pNode->getAttr(s_panningEnabledTag);
				pConnection->m_isPanningEnabled = enablePanning == "true" ? true : false;

				string const enableDistAttenuation = pNode->getAttr(s_attenuationEnabledTag);
				pConnection->m_isAttenuationEnabled = enableDistAttenuation == "true" ? true : false;

				pNode->getAttr(s_attenuationDistMin, pConnection->m_minAttenuation);
				pNode->getAttr(s_attenuationDistMax, pConnection->m_maxAttenuation);
				pNode->getAttr(s_volumeTag, pConnection->m_volume);
				pNode->getAttr(s_loopCountTag, pConnection->m_loopCount);

				if (pConnection->m_loopCount == -1)
				{
					pConnection->m_isInfiniteLoop = true;
				}

				return pConnection;
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEditorImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, ESystemItemType const controlType)
{
	std::shared_ptr<CConnection const> const pImplConnection = std::static_pointer_cast<CConnection const>(pConnection);
	CImplItem const* const pImplControl = GetControl(pConnection->GetID());
	if ((pImplControl != nullptr) && (pImplConnection != nullptr) && (controlType == ESystemItemType::Trigger))
	{
		XmlNodeRef const pConnectionNode = GetISystem()->CreateXmlNode(s_eventConnectionTag);
		pConnectionNode->setAttr(s_itemNameTag, pImplControl->GetName());

		string path;
		CImplItem const* pParent = pImplControl->GetParent();

		while (pParent != nullptr)
		{
			string const parentName = pParent->GetName();

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

		if (pImplConnection->m_type == EConnectionType::Start)
		{
			pConnectionNode->setAttr(s_connectionTypeTag, "start");
			pConnectionNode->setAttr(s_panningEnabledTag, pImplConnection->m_isPanningEnabled ? "true" : "false");
			pConnectionNode->setAttr(s_attenuationEnabledTag, pImplConnection->m_isAttenuationEnabled ? "true" : "false");
			pConnectionNode->setAttr(s_attenuationDistMin, pImplConnection->m_minAttenuation);
			pConnectionNode->setAttr(s_attenuationDistMax, pImplConnection->m_maxAttenuation);
			pConnectionNode->setAttr(s_volumeTag, pImplConnection->m_volume);

			if (pImplConnection->m_isInfiniteLoop)
			{
				pConnectionNode->setAttr(s_loopCountTag, -1);
			}
			else
			{
				pConnectionNode->setAttr(s_loopCountTag, pImplConnection->m_loopCount);
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

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		++m_connectionsByID[pImplControl->GetId()];
		pImplControl->SetConnected(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DisableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		int connectionCount = m_connectionsByID[pImplControl->GetId()] - 1;

		if (connectionCount <= 0)
		{
			connectionCount = 0;
			pImplControl->SetConnected(false);
		}

		m_connectionsByID[pImplControl->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Clear()
{
	// Delete all the controls
	for (auto const& controlPair : m_controlsCache)
	{
		CImplItem const* const pImplControl = controlPair.second;

		if (pImplControl != nullptr)
		{
			delete pImplControl;
		}
	}

	m_controlsCache.clear();

	// Clean up the root control
	m_rootControl = CImplItem();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CreateControlCache(CImplItem const* const pParent)
{
	if (pParent != nullptr)
	{
		size_t const count = pParent->ChildCount();

		for (size_t i = 0; i < count; ++i)
		{
			CImplItem* const pChild = pParent->GetChildAt(i);

			if (pChild != nullptr)
			{
				m_controlsCache[pChild->GetId()] = pChild;
				CreateControlCache(pChild);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CID CEditorImpl::GetId(string const& name) const
{
	return CCrc32::ComputeLowercase(name);
}
} // namespace SDLMixer
} // namespace ACE
