// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemAssets.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"

#include <IEditor.h>
#include <ImplItem.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/Decorators/Range.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <Serialization/Decorators/ToggleButton.h>
#include <CryMath/Cry_Geo.h>
#include <Util/Math.h>
#include <ConfigurationManager.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CSystemAsset::CSystemAsset(string const& name, ESystemItemType const type)
	: m_name(name)
	, m_type(type)
{
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* CSystemAsset::GetChild(size_t const index) const
{
	CRY_ASSERT_MESSAGE(index < m_children.size(), "Index out of bounds.");
	return m_children[index];
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::SetParent(CSystemAsset* const pParent)
{
	m_pParent = pParent;
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::AddChild(CSystemAsset* const pChildControl)
{
	m_children.emplace_back(pChildControl);
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::RemoveChild(CSystemAsset const* const pChildControl)
{
	m_children.erase(std::remove(m_children.begin(), m_children.end(), pChildControl), m_children.end());
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::SetName(string const& name)
{
	if ((!name.IsEmpty()) && (name != m_name))
	{
		m_name = Utils::GenerateUniqueControlName(name, GetType(), *CAudioControlsEditorPlugin::GetAssetsManager());
		SetModified(true);
		CAudioControlsEditorPlugin::GetAssetsManager()->OnAssetRenamed();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::SetDescription(string const& description)
{
	if (description != m_description)
	{
		m_description = description;
		SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::SetHiddenDefault(bool const isHiddenDefault)
{
	m_isHiddenDefault = isHiddenDefault;

	if (GetType() == ESystemItemType::Switch)
	{
		for (auto const pChild : m_children)
		{
			pChild->SetHiddenDefault(isHiddenDefault);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::SetModified(bool const isModified, bool const isForced /* = false */)
{
	if ((m_pParent != nullptr) && (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading() || isForced))
	{
		CAudioControlsEditorPlugin::GetAssetsManager()->SetAssetModified(this, isModified);
		m_isModified = isModified;
		// Note: This need to get changed once undo is working.
		// Then we can't set the parent to be not modified if it still could contain other modified children.
		m_pParent->SetModified(isModified, isForced);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAsset::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("properties", "Properties"))
	{
		string const name = m_name;
		ar(name, "name", "Name");
		ar.doc(name);

		string const description = m_description;
		ar(description, "description", "Description");
		ar.doc(description);

		if (ar.isInput())
		{
			SetName(name);
			SetDescription(description);
		}

		ar.closeBlock();
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemControl::CSystemControl(string const& name, CID const id, ESystemItemType const type)
	: CSystemAsset(name, type)
	, m_id(id)
{
	m_scope = Utils::GetGlobalScope();
}

//////////////////////////////////////////////////////////////////////////
CSystemControl::~CSystemControl()
{
	m_connectedControls.clear();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetName(string const& name)
{
	if ((!name.IsEmpty()) && (name != m_name))
	{
		SignalControlAboutToBeModified();
		m_name = Utils::GenerateUniqueControlName(name, GetType(), *CAudioControlsEditorPlugin::GetAssetsManager());
		SignalControlModified();
		CAudioControlsEditorPlugin::GetAssetsManager()->OnAssetRenamed();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetDescription(string const& description)
{
	if (description != m_description)
	{
		SignalControlAboutToBeModified();
		m_description = description;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetScope(Scope const scope)
{
	if (m_scope != scope)
	{
		SignalControlAboutToBeModified();
		m_scope = scope;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetAutoLoad(bool const isAutoLoad)
{
	if (isAutoLoad != m_isAutoLoad)
	{
		SignalControlAboutToBeModified();
		m_isAutoLoad = isAutoLoad;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetRadius(float const radius)
{
	if (radius != m_radius)
	{
		SignalControlAboutToBeModified();
		m_radius = radius;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CSystemControl::GetConnectionAt(int const index) const
{
	if (index < m_connectedControls.size())
	{
		return m_connectedControls[index];
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CSystemControl::GetConnection(CID const id) const
{
	for (auto const pConnection : m_connectedControls)
	{
		if ((pConnection != nullptr) && (pConnection->GetID() == id))
		{
			return pConnection;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CSystemControl::GetConnection(CImplItem const* const pAudioSystemControl) const
{
	return GetConnection(pAudioSystemControl->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::AddConnection(ConnectionPtr const pConnection)
{
	if (pConnection != nullptr)
	{
		IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

		if (pEditorImpl != nullptr)
		{
			CImplItem* const pImplControl = pEditorImpl->GetControl(pConnection->GetID());

			if (pImplControl != nullptr)
			{
				pEditorImpl->EnableConnection(pConnection);

				pConnection->signalConnectionChanged.Connect(this, &CSystemControl::SignalConnectionModified);
				m_connectedControls.emplace_back(pConnection);

				if (m_matchRadiusAndAttenuation)
				{
					MatchRadiusToAttenuation();
				}

				SignalConnectionAdded(pImplControl);
				SignalControlModified();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::RemoveConnection(ConnectionPtr const pConnection)
{
	if (pConnection != nullptr)
	{
		auto const it = std::find(m_connectedControls.begin(), m_connectedControls.end(), pConnection);

		if (it != m_connectedControls.end())
		{
			IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

			if (pEditorImpl != nullptr)
			{
				CImplItem* const pImplControl = pEditorImpl->GetControl(pConnection->GetID());

				if (pImplControl != nullptr)
				{
					pEditorImpl->DisableConnection(pConnection);
					m_connectedControls.erase(it);

					if (m_matchRadiusAndAttenuation)
					{
						MatchRadiusToAttenuation();
					}

					SignalConnectionRemoved(pImplControl);
					SignalControlModified();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::ClearConnections()
{
	if (!m_connectedControls.empty())
	{
		IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

		if (pEditorImpl != nullptr)
		{
			for (ConnectionPtr const& connection : m_connectedControls)
			{
				pEditorImpl->DisableConnection(connection);
				CImplItem* const pImplControl = pEditorImpl->GetControl(connection->GetID());

				if (pImplControl != nullptr)
				{
					SignalConnectionRemoved(pImplControl);
				}
			}
		}
		m_connectedControls.clear();

		if (m_matchRadiusAndAttenuation)
		{
			MatchRadiusToAttenuation();
		}

		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::RemoveConnection(CImplItem* const pImplControl)
{
	if (pImplControl != nullptr)
	{
		CID const id = pImplControl->GetId();
		auto it = m_connectedControls.begin();
		auto const end = m_connectedControls.end();
		IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

		for (; it != end; ++it)
		{
			if ((*it)->GetID() == id)
			{
				if (pEditorImpl != nullptr)
				{
					pEditorImpl->DisableConnection(*it);
				}

				m_connectedControls.erase(it);

				if (m_matchRadiusAndAttenuation)
				{
					MatchRadiusToAttenuation();
				}

				SignalConnectionRemoved(pImplControl);
				SignalControlModified();
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SignalControlModified()
{
	if (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading())
	{
		CAudioControlsEditorPlugin::GetAssetsManager()->OnControlModified(this);
		SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SignalControlAboutToBeModified()
{
	if (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading())
	{
		CAudioControlsEditorPlugin::GetAssetsManager()->OnControlAboutToBeModified(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SignalConnectionAdded(CImplItem* const pImplControl)
{
	CAudioControlsEditorPlugin::GetAssetsManager()->OnConnectionAdded(this, pImplControl);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SignalConnectionRemoved(CImplItem* const pImplControl)
{
	CAudioControlsEditorPlugin::GetAssetsManager()->OnConnectionRemoved(this, pImplControl);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SignalConnectionModified()
{
	if (m_matchRadiusAndAttenuation)
	{
		MatchRadiusToAttenuation();
	}

	SignalControlModified();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::ReloadConnections()
{
	IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

	if (pEditorImpl != nullptr)
	{
		std::map<int, XMLNodeList> connectionNodes;
		std::swap(connectionNodes, m_connectionNodes);

		for (auto const& connectionPair : connectionNodes)
		{
			for (auto const& connection : connectionPair.second)
			{
				LoadConnectionFromXML(connection.xmlNode, connectionPair.first);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::LoadConnectionFromXML(XmlNodeRef const xmlNode, int const platformIndex)
{
	IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

	if (pEditorImpl != nullptr)
	{
		ConnectionPtr pConnection = pEditorImpl->CreateConnectionFromXMLNode(xmlNode, GetType());

		if (pConnection != nullptr)
		{
			if (GetType() == ESystemItemType::Preload)
			{
				// The connection could already exist but using a different platform
				ConnectionPtr const pPreviousConnection = GetConnection(pConnection->GetID());

				if (pPreviousConnection == nullptr)
				{
					if (platformIndex != -1)
					{
						pConnection->ClearPlatforms();
					}
					AddConnection(pConnection);
				}
				else
				{
					pConnection = pPreviousConnection;
				}

				if (platformIndex != -1)
				{
					pConnection->EnableForPlatform(platformIndex, true);
				}
			}
			else
			{
				AddConnection(pConnection);
			}
		}
		else if ((GetType() == ESystemItemType::Preload) && (platformIndex == -1))
		{
			// If it's a preload connection from another middleware and the platform
			// wasn't found (old file format) fall back to adding them to all the platforms
			std::vector<dll_string> const& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();
			size_t const count = platforms.size();

			for (size_t i = 0; i < count; ++i)
			{
				AddRawXMLConnection(xmlNode, false, i);
			}

			return;
		}

		AddRawXMLConnection(xmlNode, pConnection != nullptr, platformIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("properties", "Properties"))
	{
		// Name
		string const name = m_name;
		ar(name, "name", "Name");
		ar.doc(name);

		// Description
		string const description = m_description;
		ar(description, "description", "Description");
		ar.doc(description);

		// Scope
		Serialization::StringList scopeList;
		ScopeInfoList scopeInfoList;
		CAudioControlsEditorPlugin::GetAssetsManager()->GetScopeInfoList(scopeInfoList);

		for (auto const& scopeInfo : scopeInfoList)
		{
			scopeList.emplace_back(scopeInfo.name);
		}

		Serialization::StringListValue const selectedScope(scopeList, CAudioControlsEditorPlugin::GetAssetsManager()->GetScopeInfo(m_scope).name);
		Scope scope = m_scope;

		if (m_type != ESystemItemType::State)
		{
			ar(selectedScope, "scope", "Scope");
			scope = CAudioControlsEditorPlugin::GetAssetsManager()->GetScope(scopeList[selectedScope.index()]);
		}

		// Auto Load
		bool isAutoLoad = m_isAutoLoad;

		if (m_type == ESystemItemType::Preload)
		{
			ar(isAutoLoad, "auto_load", "Auto Load");
		}

		// Max Radius
		float radius = m_radius;
		float fadeOutDistance = m_occlusionFadeOutDistance;

		if ((m_type == ESystemItemType::Trigger) && (ar.openBlock("activity_radius", "Activity Radius")))
		{
			bool hasPlaceholderConnections = false;
			IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

			if (pEditorImpl != nullptr)
			{
				radius = 0.0f;

				for (auto const pConnection : m_connectedControls)
				{
					CImplItem const* const pImplControl = pEditorImpl->GetControl(pConnection->GetID());

					if ((pImplControl != nullptr) && (!pImplControl->IsPlaceholder()))
					{
						radius = std::max(radius, pImplControl->GetRadius());
					}
					else
					{
						// If control has placeholder connection we cannot enforce the link between activity radius
						// and attenuation as the user could be missing the middleware project
						hasPlaceholderConnections = true;
					}
				}
			}

			if (m_matchRadiusAndAttenuation && !hasPlaceholderConnections)
			{
				ar(Serialization::Range<float>(radius, 0.0f, std::numeric_limits<float>::max()), "max_radius", "!^");
			}
			else
			{
				ar(Serialization::Range<float>(m_radius, 0.0f, std::numeric_limits<float>::max()), "max_radius", "^");
			}

			if (!hasPlaceholderConnections)
			{
				if (ar.openBlock("attenuation", "Attenuation"))
				{
					ar(radius, "attenuation", "!^");
					ar(Serialization::ToggleButton(m_matchRadiusAndAttenuation, "icons:Navigation/Tools_Link.ico", "icons:Navigation/Tools_Link_Unlink.ico"), "link", "^");

					if (!m_matchRadiusAndAttenuation)
					{
						radius = m_radius;
					}

					ar.closeBlock();
				}
			}

			ar(Serialization::Range<float>(fadeOutDistance, 0.0f, radius), "fadeOutDistance", "Occlusion Fade-Out Distance");
			ar.closeBlock();
		}

		if (ar.isInput())
		{
			SetName(name);
			SetDescription(description);
			SetScope(scope);
			SetAutoLoad(isAutoLoad);
			SetRadius(radius);
			SetOcclusionFadeOutDistance(fadeOutDistance);
		}

		ar.closeBlock();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::SetOcclusionFadeOutDistance(float const fadeOutDistance)
{
	if (fadeOutDistance != m_occlusionFadeOutDistance)
	{
		SignalControlAboutToBeModified();
		m_occlusionFadeOutDistance = fadeOutDistance;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::AddRawXMLConnection(XmlNodeRef const xmlNode, bool const isValid, int const platformIndex /*= -1*/)
{
	m_connectionNodes[platformIndex].emplace_back(xmlNode, isValid);
}

//////////////////////////////////////////////////////////////////////////
XMLNodeList& CSystemControl::GetRawXMLConnections(int const platformIndex /*= -1*/)
{
	return m_connectionNodes[platformIndex];
}

//////////////////////////////////////////////////////////////////////////
void CSystemControl::MatchRadiusToAttenuation()
{
	IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

	if (pEditorImpl != nullptr)
	{
		float radius = 0.0f;

		for (auto const pConnection : m_connectedControls)
		{
			CImplItem const* const pImplControl = pEditorImpl->GetControl(pConnection->GetID());

			if (pImplControl != nullptr)
			{
				if (pImplControl->IsPlaceholder())
				{
					// We don't match controls that have placeholder
					// connections as we don't know what the real values should be
					return;
				}
				else
				{
					radius = std::max(radius, pImplControl->GetRadius());
				}
			}
		}

		SetRadius(radius);
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemLibrary::CSystemLibrary(string const& name)
	: CSystemAsset(name, ESystemItemType::Library)
{
}

//////////////////////////////////////////////////////////////////////////
void CSystemLibrary::SetModified(bool const isModified, bool const isForced /* = false */)
{
	if (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading() || isForced)
	{
		CAudioControlsEditorPlugin::GetAssetsManager()->SetAssetModified(this, isModified);
		m_isModified = isModified;
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemFolder::CSystemFolder(string const& name)
	: CSystemAsset(name, ESystemItemType::Folder)
{
}
} // namespace ACE
