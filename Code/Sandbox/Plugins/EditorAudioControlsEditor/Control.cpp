// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Control.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "AssetUtils.h"

#include <IItem.h>
#include <CrySerialization/StringList.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CControl::CControl(string const& name, ControlId const id, EAssetType const type)
	: CAsset(name, type)
	, m_id(id)
	, m_scope(GlobalScopeId)
{}

//////////////////////////////////////////////////////////////////////////
CControl::~CControl()
{
	m_connections.clear();
}

//////////////////////////////////////////////////////////////////////////
void CControl::SetName(string const& name)
{
	if ((!name.IsEmpty()) && (name != m_name) && ((m_flags& EAssetFlags::IsDefaultControl) == 0))
	{
		SignalControlAboutToBeModified();

		if (m_type != EAssetType::State)
		{
			m_name = AssetUtils::GenerateUniqueControlName(name, m_type);
		}
		else
		{
			m_name = AssetUtils::GenerateUniqueName(name, m_type, m_pParent);
		}

		SignalControlModified();
		g_assetsManager.OnAssetRenamed(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SetDescription(string const& description)
{
	if (description != m_description)
	{
		SignalControlAboutToBeModified();
		m_description = description;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::Serialize(Serialization::IArchive& ar)
{
	// Name
	string const name = m_name;

	if ((m_flags& EAssetFlags::IsDefaultControl) != 0)
	{
		ar(name, "name", "!Name");
	}
	else
	{
		ar(name, "name", "Name");
	}

	ar.doc(name);

	// Description
	string const description = m_description;

	if ((m_flags& EAssetFlags::IsDefaultControl) != 0)
	{
		ar(description, "description", "!Description");
	}
	else
	{
		ar(description, "description", "Description");
	}

	ar.doc(description);

	// Scope
	Scope scope = m_scope;

	if (((m_flags& EAssetFlags::IsDefaultControl) == 0) && (m_type != EAssetType::State))
	{
		Serialization::StringList scopeList;
		ScopeInfos scopeInfos;
		g_assetsManager.GetScopeInfos(scopeInfos);

		for (auto const& scopeInfo : scopeInfos)
		{
			scopeList.emplace_back(scopeInfo.name);
		}

		Serialization::StringListValue const selectedScope(scopeList, g_assetsManager.GetScopeInfo(m_scope).name);
		ar(selectedScope, "scope", "Scope");
		scope = g_assetsManager.GetScope(scopeList[selectedScope.index()]);
	}

	// Auto Load
	bool isAutoLoad = m_isAutoLoad;

	if (m_type == EAssetType::Preload)
	{
		ar(isAutoLoad, "auto_load", "Auto Load");
	}

	// Max Radius
	float radius = m_radius;

	if (((m_flags& EAssetFlags::IsDefaultControl) == 0) && (m_type == EAssetType::Trigger))
	{
		bool hasPlaceholderConnections = false;
		float connectionMaxRadius = 0.0f;

		for (auto const& connection : m_connections)
		{
			Impl::IItem const* const pIItem = g_pIImpl->GetItem(connection->GetID());

			if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
			{
				connectionMaxRadius = std::max(connectionMaxRadius, pIItem->GetRadius());
			}
			else
			{
				// If control has placeholder connection we cannot enforce the link between activity radius
				// and attenuation as the user could be missing the middleware project.
				hasPlaceholderConnections = true;
				break;
			}
		}

		if (!hasPlaceholderConnections)
		{
			radius = connectionMaxRadius;
		}
	}

	if (ar.isInput())
	{
		SetName(name);
		SetDescription(description);
		SetScope(scope);
		SetAutoLoad(isAutoLoad);
		SetRadius(radius);
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SetScope(Scope const scope)
{
	if (m_scope != scope)
	{
		SignalControlAboutToBeModified();
		m_scope = scope;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SetAutoLoad(bool const isAutoLoad)
{
	if (isAutoLoad != m_isAutoLoad)
	{
		SignalControlAboutToBeModified();
		m_isAutoLoad = isAutoLoad;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SetRadius(float const radius)
{
	if (radius != m_radius)
	{
		SignalControlAboutToBeModified();
		m_radius = radius;
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CControl::GetConnectionAt(size_t const index) const
{
	ConnectionPtr pConnection = nullptr;

	if (index < m_connections.size())
	{
		pConnection = m_connections[index];
	}

	return pConnection;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CControl::GetConnection(ControlId const id) const
{
	ConnectionPtr pConnection = nullptr;

	for (auto const& connection : m_connections)
	{
		if ((connection != nullptr) && (connection->GetID() == id))
		{
			pConnection = connection;
			break;
		}
	}

	return pConnection;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CControl::GetConnection(Impl::IItem const* const pIItem) const
{
	return GetConnection(pIItem->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CControl::AddConnection(ConnectionPtr const pConnection)
{
	if (pConnection != nullptr)
	{
		Impl::IItem* const pIItem = g_pIImpl->GetItem(pConnection->GetID());

		if (pIItem != nullptr)
		{
			g_pIImpl->EnableConnection(pConnection, g_assetsManager.IsLoading());
			pConnection->SignalConnectionChanged.Connect(this, &CControl::SignalConnectionModified);
			m_connections.push_back(pConnection);
			MatchRadiusToAttenuation();
			SignalConnectionAdded(pIItem);
			SignalControlModified();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::RemoveConnection(ConnectionPtr const pConnection)
{
	if (pConnection != nullptr)
	{
		auto const it = std::find(m_connections.begin(), m_connections.end(), pConnection);

		if (it != m_connections.end())
		{
			Impl::IItem* const pIItem = g_pIImpl->GetItem(pConnection->GetID());

			if (pIItem != nullptr)
			{
				g_pIImpl->DisableConnection(pConnection, g_assetsManager.IsLoading());
				pConnection->SignalConnectionChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
				m_connections.erase(it);
				MatchRadiusToAttenuation();
				SignalConnectionRemoved(pIItem);
				SignalControlModified();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::ClearConnections()
{
	if (!m_connections.empty())
	{
		bool const isLoading = g_assetsManager.IsLoading();

		for (auto const& connection : m_connections)
		{
			g_pIImpl->DisableConnection(connection, isLoading);
			Impl::IItem* const pIItem = g_pIImpl->GetItem(connection->GetID());

			if (pIItem != nullptr)
			{
				SignalConnectionRemoved(pIItem);
			}
		}

		m_connections.clear();
		MatchRadiusToAttenuation();
		SignalControlModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::BackupAndClearConnections()
{
	// Raw connections are used to temporarily store connections in XML format
	// when middleware data gets reloaded.
	m_rawConnections.clear();

	if (m_type != EAssetType::Preload)
	{
		for (auto const& connection : m_connections)
		{
			XmlNodeRef const pRawConnection = g_pIImpl->CreateXMLNodeFromConnection(connection, m_type);

			if (pRawConnection != nullptr)
			{
				m_rawConnections[-1].push_back(pRawConnection);
			}
		}
	}
	else
	{
		auto const numPlatforms = static_cast<int>(g_platforms.size());

		for (auto const& connection : m_connections)
		{
			XmlNodeRef const pRawConnection = g_pIImpl->CreateXMLNodeFromConnection(connection, m_type);

			if (pRawConnection != nullptr)
			{
				for (int i = 0; i < numPlatforms; ++i)
				{
					if (connection->IsPlatformEnabled(static_cast<PlatformIndexType>(i)))
					{
						m_rawConnections[i].push_back(pRawConnection);
					}
				}
			}
		}
	}

	ClearConnections();
}

//////////////////////////////////////////////////////////////////////////
void CControl::ReloadConnections()
{
	if (!m_rawConnections.empty())
	{
		for (auto const& connectionPair : m_rawConnections)
		{
			for (auto const& connection : connectionPair.second)
			{
				LoadConnectionFromXML(connection, connectionPair.first);
			}
		}
	}

	m_rawConnections.clear();
}

//////////////////////////////////////////////////////////////////////////
void CControl::RemoveConnection(Impl::IItem* const pIItem)
{
	if (pIItem != nullptr)
	{
		ControlId const id = pIItem->GetId();
		auto it = m_connections.begin();
		auto const end = m_connections.end();
		bool const isLoading = g_assetsManager.IsLoading();

		for (; it != end; ++it)
		{
			if ((*it)->GetID() == id)
			{
				g_pIImpl->DisableConnection(*it, isLoading);

				m_connections.erase(it);
				MatchRadiusToAttenuation();
				SignalConnectionRemoved(pIItem);
				SignalControlModified();
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SignalControlModified()
{
	if (!g_assetsManager.IsLoading())
	{
		g_assetsManager.OnControlModified(this);
		SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SignalControlAboutToBeModified()
{
	if (!g_assetsManager.IsLoading())
	{
		g_assetsManager.OnControlAboutToBeModified(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::SignalConnectionAdded(Impl::IItem* const pIItem)
{
	g_assetsManager.OnConnectionAdded(this, pIItem);
}

//////////////////////////////////////////////////////////////////////////
void CControl::SignalConnectionRemoved(Impl::IItem* const pIItem)
{
	g_assetsManager.OnConnectionRemoved(this, pIItem);
}

//////////////////////////////////////////////////////////////////////////
void CControl::SignalConnectionModified()
{
	MatchRadiusToAttenuation();
	SignalControlModified();
}

//////////////////////////////////////////////////////////////////////////
void CControl::LoadConnectionFromXML(XmlNodeRef const xmlNode, int const platformIndex /*= -1*/)
{
	ConnectionPtr pConnection = g_pIImpl->CreateConnectionFromXMLNode(xmlNode, m_type);

	if (pConnection != nullptr)
	{
		if (m_type == EAssetType::Preload)
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
				pConnection->SetPlatformEnabled(static_cast<PlatformIndexType>(platformIndex), true);
			}
		}
		else
		{
			AddConnection(pConnection);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CControl::MatchRadiusToAttenuation()
{
	float radius = 0.0f;
	bool isPlaceHolder = false;

	for (auto const& connection : m_connections)
	{
		Impl::IItem const* const pIItem = g_pIImpl->GetItem(connection->GetID());

		if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) == 0))
		{
			radius = std::max(radius, pIItem->GetRadius());
		}
		else
		{
			// We don't match controls that have placeholder
			// connections as we don't know what the real values should be.
			isPlaceHolder = true;
			break;
		}
	}

	if (!isPlaceHolder)
	{
		SetRadius(radius);
	}
}
} // namespace ACE
