// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include "Common.h"
#include "BankConnection.h"
#include "EventConnection.h"
#include "ParameterConnection.h"
#include "ParameterToStateConnection.h"
#include "SnapshotConnection.h"
#include "ProjectLoader.h"
#include "DataPanel.h"
#include "Utils.h"

#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
EItemType TagToType(char const* const szTag)
{
	EItemType type = EItemType::None;

	if (_stricmp(szTag, CryAudio::s_szEventTag) == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::s_szParameterTag) == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::s_szSnapshotTag) == 0)
	{
		type = EItemType::Snapshot;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::s_szFileTag) == 0)
	{
		type = EItemType::Bank;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::s_szBusTag) == 0)
	{
		type = EItemType::Return;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::s_szVcaTag) == 0)
	{
		type = EItemType::VCA;
	}

	// Backwards compatibility will be removed before March 2019.
#if defined (USE_BACKWARDS_COMPATIBILITY)
	else if (_stricmp(szTag, "FmodEvent") == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, "FmodEventParameter") == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, "FmodSnapshot") == 0)
	{
		type = EItemType::Snapshot;
	}
	else if (_stricmp(szTag, "FmodSnapshotParameter") == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, "FmodFile") == 0)
	{
		type = EItemType::Bank;
	}
	else if (_stricmp(szTag, "FmodBus") == 0)
	{
		type = EItemType::Return;
	}
#endif  // USE_BACKWARDS_COMPATIBILITY

	else
	{
		type = EItemType::None;
	}

	return type;
}

//////////////////////////////////////////////////////////////////////////
char const* TypeToTag(EItemType const type)
{
	char const* szTag = nullptr;

	switch (type)
	{
	case EItemType::Event:
		szTag = CryAudio::s_szEventTag;
		break;
	case EItemType::Parameter:
		szTag = CryAudio::Impl::Fmod::s_szParameterTag;
		break;
	case EItemType::Snapshot:
		szTag = CryAudio::Impl::Fmod::s_szSnapshotTag;
		break;
	case EItemType::Bank:
		szTag = CryAudio::Impl::Fmod::s_szFileTag;
		break;
	case EItemType::Return:
		szTag = CryAudio::Impl::Fmod::s_szBusTag;
		break;
	case EItemType::VCA:
		szTag = CryAudio::Impl::Fmod::s_szVcaTag;
		break;
	default:
		szTag = nullptr;
		break;
	}

	return szTag;
}

//////////////////////////////////////////////////////////////////////////
string TypeToEditorFolderName(EItemType const type)
{
	string folderName = "";

	switch (type)
	{
	case EItemType::Event:
		folderName = s_eventsFolderName + "/";
		break;
	case EItemType::Parameter:
		folderName = s_parametersFolderName + "/";
		break;
	case EItemType::Snapshot:
		folderName = s_snapshotsFolderName + "/";
		break;
	case EItemType::Bank:
		folderName = s_soundBanksFolderName + "/";
		break;
	case EItemType::Return:
		folderName = s_returnsFolderName + "/";
		break;
	case EItemType::VCA:
		folderName = s_vcasFolderName + "/";
		break;
	default:
		folderName = "";
		break;
	}

	return folderName;
}

//////////////////////////////////////////////////////////////////////////
CItem* SearchForItem(CItem* const pItem, string const& name, EItemType const type)
{
	CItem* pSearchedItem = nullptr;

	if ((pItem->GetName().compareNoCase(name) == 0) && (pItem->GetType() == type))
	{
		pSearchedItem = pItem;
	}
	else
	{
		size_t const numChildren = pItem->GetNumChildren();

		for (size_t i = 0; i < numChildren; ++i)
		{
			CItem* const pFoundItem = SearchForItem(static_cast<CItem* const>(pItem->GetChildAt(i)), name, type);

			if (pFoundItem != nullptr)
			{
				pSearchedItem = pFoundItem;
				break;
			}
		}
	}

	return pSearchedItem;
}

//////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pDataPanel(nullptr)
	, m_projectPath(AUDIO_SYSTEM_DATA_ROOT "/fmod_project")
	, m_assetsPath(AUDIO_SYSTEM_DATA_ROOT "/" + string(CryAudio::Impl::Fmod::s_szImplFolderName) + "/" + string(CryAudio::s_szAssetsFolderName))
	, m_szUserSettingsFile("%USER%/audiocontrolseditor_fmod.user")
{
	gEnv->pAudioSystem->GetImplInfo(m_implInfo);
	m_implName = m_implInfo.name.c_str();
	m_implFolderName = CryAudio::Impl::Fmod::s_szImplFolderName;

	Serialization::LoadJsonFile(*this, m_szUserSettingsFile);
}

//////////////////////////////////////////////////////////////////////////
CImpl::~CImpl()
{
	Clear();
	DestroyDataPanel();
}

//////////////////////////////////////////////////////////////////////////
QWidget* CImpl::CreateDataPanel()
{
	m_pDataPanel = new CDataPanel(*this);
	return m_pDataPanel;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestroyDataPanel()
{
	if (m_pDataPanel != nullptr)
	{
		delete m_pDataPanel;
		m_pDataPanel = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(m_projectPath, m_assetsPath, m_rootItem, m_itemCache, *this);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				auto const pItem = static_cast<CItem* const>(GetItem(connection.first));

				if (pItem != nullptr)
				{
					pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsConnected);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}

	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
IItem* CImpl::GetItem(ControlId const id) const
{
	IItem* pIItem = nullptr;

	if (id >= 0)
	{
		pIItem = stl::find_in_map(m_itemCache, id, nullptr);
	}

	return pIItem;
}

//////////////////////////////////////////////////////////////////////////
CryIcon const& CImpl::GetItemIcon(IItem const* const pIItem) const
{
	auto const pItem = static_cast<CItem const* const>(pIItem);
	CRY_ASSERT_MESSAGE(pItem != nullptr, "Impl item is null pointer.");
	return GetTypeIcon(pItem->GetType());
}

//////////////////////////////////////////////////////////////////////////
QString const& CImpl::GetItemTypeName(IItem const* const pIItem) const
{
	auto const pItem = static_cast<CItem const* const>(pIItem);
	CRY_ASSERT_MESSAGE(pItem != nullptr, "Impl item is null pointer.");
	return TypeToString(pItem->GetType());
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetProjectPath(char const* const szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(m_szUserSettingsFile, *this);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const
{
	bool isCompatible = false;
	auto const pItem = static_cast<CItem const* const>(pIItem);

	if (pItem != nullptr)
	{
		EItemType const implType = pItem->GetType();

		switch (assetType)
		{
		case EAssetType::Trigger:
			isCompatible = (implType == EItemType::Event) || (implType == EItemType::Snapshot);
			break;
		case EAssetType::Parameter:
			isCompatible = (implType == EItemType::Parameter) || (implType == EItemType::VCA);
			break;
		case EAssetType::State:
			isCompatible = (implType == EItemType::Parameter) || (implType == EItemType::VCA);
			break;
		case EAssetType::Environment:
			isCompatible = (implType == EItemType::Return) || (implType == EItemType::Parameter);
			break;
		case EAssetType::Preload:
			isCompatible = (implType == EItemType::Bank);
			break;
		default:
			isCompatible = false;
			break;
		}
	}

	return isCompatible;
}

//////////////////////////////////////////////////////////////////////////
EAssetType CImpl::ImplTypeToAssetType(IItem const* const pIItem) const
{
	EAssetType assetType = EAssetType::None;
	auto const pItem = static_cast<CItem const* const>(pIItem);

	if (pItem != nullptr)
	{
		EItemType const implType = pItem->GetType();

		switch (implType)
		{
		case EItemType::Event:
			assetType = EAssetType::Trigger;
			break;
		case EItemType::Parameter:
		case EItemType::VCA:
			assetType = EAssetType::Parameter;
			break;
		case EItemType::Snapshot:
			assetType = EAssetType::Trigger;
			break;
		case EItemType::Bank:
			assetType = EAssetType::Preload;
			break;
		case EItemType::Return:
			assetType = EAssetType::Environment;
			break;
		default:
			assetType = EAssetType::None;
			break;
		}
	}

	return assetType;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CImpl::CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem)
{
	ConnectionPtr pConnection = nullptr;
	auto const pItem = static_cast<CItem const* const>(pIItem);

	if (pItem != nullptr)
	{
		EItemType const type = pItem->GetType();

		if (type == EItemType::Event)
		{
			pConnection = std::make_shared<CEventConnection>(pItem->GetId());
		}
		else if (type == EItemType::Snapshot)
		{
			pConnection = std::make_shared<CSnapshotConnection>(pItem->GetId());
		}
		else if ((type == EItemType::Parameter) || (type == EItemType::VCA))
		{
			if ((assetType == EAssetType::Parameter) || (assetType == EAssetType::Environment))
			{
				pConnection = std::make_shared<CParameterConnection>(pItem->GetId());
			}
			else if (assetType == EAssetType::State)
			{
				pConnection = std::make_shared<CParameterToStateConnection>(pItem->GetId(), type);
			}
			else
			{
				pConnection = std::make_shared<CBaseConnection>(pItem->GetId());
			}
		}
		else if (type == EItemType::Bank)
		{
			pConnection = std::make_shared<CBankConnection>(pItem->GetId());
		}
		else
		{
			pConnection = std::make_shared<CBaseConnection>(pItem->GetId());
		}
	}

	return pConnection;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType)
{
	ConnectionPtr pConnectionPtr = nullptr;

	if (pNode != nullptr)
	{
		auto const type = TagToType(pNode->getTag());

		if (type != EItemType::None)
		{
			string name = pNode->getAttr(CryAudio::s_szNameAttribute);
			string localizedAttribute = pNode->getAttr(CryAudio::Impl::Fmod::s_szLocalizedAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && pNode->haveAttr("fmod_name"))
			{
				name = pNode->getAttr("fmod_name");
			}

			if (localizedAttribute.IsEmpty() && pNode->haveAttr("fmod_localized"))
			{
				localizedAttribute = pNode->getAttr("fmod_localized");
			}
#endif      // USE_BACKWARDS_COMPATIBILITY
			bool const isLocalized = (localizedAttribute.compareNoCase(CryAudio::Impl::Fmod::s_szTrueValue) == 0);

			CItem* pItem = nullptr;

			if (type != EItemType::Parameter)
			{
				pItem = GetItemFromPath(TypeToEditorFolderName(type) + name);
			}
			else
			{
				pItem = SearchForItem(&m_rootItem, name, type);
			}

			if ((pItem == nullptr) || (type != pItem->GetType()))
			{
				// If item not found, create a placeholder.
				// We want to keep that connection even if it's not in the middleware as the user could
				// be using the engine without the fmod project

				string path = "";
				int const pos = name.find_last_of("/");

				if (pos != string::npos)
				{
					path = name.substr(0, pos);
					name = name.substr(pos + 1, name.length() - pos);
				}

				pItem = CreatePlaceholderItem(name, type, CreatePlaceholderFolderPath(path));
			}

			switch (type)
			{
			case EItemType::Event:
				{
					string actionType = pNode->getAttr(CryAudio::s_szTypeAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
					if (actionType.IsEmpty() && pNode->haveAttr("fmod_event_type"))
					{
						actionType = pNode->getAttr("fmod_event_type");
					}
#endif          // USE_BACKWARDS_COMPATIBILITY

					CEventConnection::EActionType eventActionType = CEventConnection::EActionType::Start;

					if (actionType.compareNoCase(CryAudio::Impl::Fmod::s_szStopValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Stop;
					}
					else if (actionType.compareNoCase(CryAudio::Impl::Fmod::s_szPauseValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Pause;
					}
					else if (actionType.compareNoCase(CryAudio::Impl::Fmod::s_szResumeValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Resume;
					}

					auto const pConnection = std::make_shared<CEventConnection>(pItem->GetId(), eventActionType);
					pConnectionPtr = pConnection;
				}
				break;
			case EItemType::Snapshot:
				{
					string actionType = pNode->getAttr(CryAudio::s_szTypeAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
					if (actionType.IsEmpty() && pNode->haveAttr("fmod_event_type"))
					{
						actionType = pNode->getAttr("fmod_event_type");
					}
#endif          // USE_BACKWARDS_COMPATIBILITY

					CSnapshotConnection::EActionType const snapshotActionType = (actionType.compareNoCase(CryAudio::Impl::Fmod::s_szStopValue) == 0) ? CSnapshotConnection::EActionType::Stop : CSnapshotConnection::EActionType::Start;
					auto const pConnection = std::make_shared<CSnapshotConnection>(pItem->GetId(), snapshotActionType);
					pConnectionPtr = pConnection;
				}
				break;
			case EItemType::Parameter:
			case EItemType::VCA:
				{
					if ((assetType == EAssetType::Parameter) || (assetType == EAssetType::Environment))
					{
						float mult = CryAudio::Impl::Fmod::s_defaultParamMultiplier;
						float shift = CryAudio::Impl::Fmod::s_defaultParamShift;

						if (pNode->haveAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute))
						{
							string const value = pNode->getAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute);
							mult = (float)std::atof(value.c_str());
						}
#if defined (USE_BACKWARDS_COMPATIBILITY)
						else if (pNode->haveAttr("fmod_value_multiplier"))
						{
							string const value = pNode->getAttr("fmod_value_multiplier");
							mult = (float)std::atof(value.c_str());
						}
#endif            // USE_BACKWARDS_COMPATIBILITY
						if (pNode->haveAttr(CryAudio::Impl::Fmod::s_szShiftAttribute))
						{
							string const value = pNode->getAttr(CryAudio::Impl::Fmod::s_szShiftAttribute);
							shift = (float)std::atof(value.c_str());
						}
#if defined (USE_BACKWARDS_COMPATIBILITY)
						else if (pNode->haveAttr("fmod_value_shift"))
						{
							string const value = pNode->getAttr("fmod_value_shift");
							shift = (float)std::atof(value.c_str());
						}
#endif            // USE_BACKWARDS_COMPATIBILITY

						auto const pConnection = std::make_shared<CParameterConnection>(pItem->GetId(), mult, shift);
						pConnectionPtr = pConnection;
					}
					else if (assetType == EAssetType::State)
					{
						string valueString = pNode->getAttr(CryAudio::Impl::Fmod::s_szValueAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
						if (valueString.IsEmpty() && pNode->haveAttr("fmod_value"))
						{
							valueString = pNode->getAttr("fmod_value");
						}
#endif            // USE_BACKWARDS_COMPATIBILITY

						auto const valueFloat = static_cast<float>(std::atof(valueString.c_str()));
						auto const pConnection = std::make_shared<CParameterToStateConnection>(pItem->GetId(), type, valueFloat);
						pConnectionPtr = pConnection;
					}
				}
				break;
			case EItemType::Bank:
				{
					pConnectionPtr = std::make_shared<CBankConnection>(pItem->GetId());
				}
				break;
			case EItemType::Return:
				{
					pConnectionPtr = std::make_shared<CBaseConnection>(pItem->GetId());
				}
				break;
			}
		}
	}

	return pConnectionPtr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType)
{
	XmlNodeRef pNode = nullptr;

	auto const pItem = static_cast<CItem* const>(GetItem(pConnection->GetID()));

	if (pItem != nullptr)
	{
		auto const type = pItem->GetType();
		pNode = GetISystem()->CreateXmlNode(TypeToTag(type));

		switch (type)
		{
		case EItemType::Event:
			{
				pNode->setAttr(CryAudio::s_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
				auto const pEventConnection = static_cast<const CEventConnection*>(pConnection.get());

				if (pEventConnection != nullptr)
				{
					if (pEventConnection->GetActionType() == CEventConnection::EActionType::Stop)
					{
						pNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::Fmod::s_szStopValue);
					}
					else if (pEventConnection->GetActionType() == CEventConnection::EActionType::Pause)
					{
						pNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::Fmod::s_szPauseValue);
					}
					else if (pEventConnection->GetActionType() == CEventConnection::EActionType::Resume)
					{
						pNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::Fmod::s_szResumeValue);
					}
				}
			}
			break;
		case EItemType::Snapshot:
			{
				pNode->setAttr(CryAudio::s_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
				auto const pEventConnection = static_cast<const CSnapshotConnection*>(pConnection.get());

				if ((pEventConnection != nullptr) && (pEventConnection->GetActionType() == CSnapshotConnection::EActionType::Stop))
				{
					pNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::Fmod::s_szStopValue);
				}
			}
			break;
		case EItemType::Return:
			{
				pNode->setAttr(CryAudio::s_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
			}
			break;
		case EItemType::Parameter:
		case EItemType::VCA:
			{
				pNode->setAttr(CryAudio::s_szNameAttribute, pItem->GetName());

				if (assetType == EAssetType::State)
				{
					auto const pStateConnection = static_cast<const CParameterToStateConnection*>(pConnection.get());

					if (pStateConnection != nullptr)
					{
						pNode->setAttr(CryAudio::Impl::Fmod::s_szValueAttribute, pStateConnection->GetValue());
					}
				}
				else if ((assetType == EAssetType::Parameter) || (assetType == EAssetType::Environment))
				{
					auto const pParamConnection = static_cast<const CParameterConnection*>(pConnection.get());

					if (pParamConnection->GetMultiplier() != CryAudio::Impl::Fmod::s_defaultParamMultiplier)
					{
						pNode->setAttr(CryAudio::Impl::Fmod::s_szMutiplierAttribute, pParamConnection->GetMultiplier());
					}

					if (pParamConnection->GetShift() != CryAudio::Impl::Fmod::s_defaultParamShift)
					{
						pNode->setAttr(CryAudio::Impl::Fmod::s_szShiftAttribute, pParamConnection->GetShift());
					}
				}
			}
			break;
		case EItemType::Bank:
			{
				pNode->setAttr(CryAudio::s_szNameAttribute, pItem->GetName());

				if ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0)
				{
					pNode->setAttr(CryAudio::Impl::Fmod::s_szLocalizedAttribute, CryAudio::Impl::Fmod::s_szTrueValue);
				}
			}
			break;
		}
	}

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::EnableConnection(ConnectionPtr const pConnection, bool const isLoading)
{
	auto const pItem = static_cast<CItem* const>(GetItem(pConnection->GetID()));

	if (pItem != nullptr)
	{
		++m_connectionsByID[pItem->GetId()];
		pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsConnected);

		if ((m_pDataPanel != nullptr) && !isLoading)
		{
			m_pDataPanel->OnConnectionAdded();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DisableConnection(ConnectionPtr const pConnection, bool const isLoading)
{
	auto const pItem = static_cast<CItem* const>(GetItem(pConnection->GetID()));

	if (pItem != nullptr)
	{
		int connectionCount = m_connectionsByID[pItem->GetId()] - 1;

		if (connectionCount < 1)
		{
			CRY_ASSERT_MESSAGE(connectionCount >= 0, "Connection count is < 0");
			connectionCount = 0;
			pItem->SetFlags(pItem->GetFlags() & ~EItemFlags::IsConnected);
		}

		m_connectionsByID[pItem->GetId()] = connectionCount;

		if ((m_pDataPanel != nullptr) && !isLoading)
		{
			m_pDataPanel->OnConnectionRemoved();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAboutToReload()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnAboutToReload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnReloaded()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnReloaded();
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnSelectConnectedItem(ControlId const id) const
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnSelectConnectedItem(id);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Clear()
{
	for (auto const& itemPair : m_itemCache)
	{
		delete itemPair.second;
	}

	m_itemCache.clear();
	m_rootItem.Clear();
}

//////////////////////////////////////////////////////////////////////////
CItem* CImpl::CreatePlaceholderItem(string const& name, EItemType const type, CItem* const pParent)
{
	ControlId const id = Utils::GetId(type, name, pParent, m_rootItem);

	auto pItem = static_cast<CItem*>(GetItem(id));

	if (pItem == nullptr)
	{
		pItem = new CItem(name, id, type, EItemFlags::IsPlaceHolder);

		if (pParent != nullptr)
		{
			pParent->AddChild(pItem);
		}
		else
		{
			m_rootItem.AddChild(pItem);
		}

		m_itemCache[id] = pItem;

	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CImpl::GetItemFromPath(string const& fullpath)
{
	CItem* pItem = &m_rootItem;
	int start = 0;
	string token = fullpath.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CItem* pChild = nullptr;
		size_t const numChildren = pItem->GetNumChildren();

		for (size_t i = 0; i < numChildren; ++i)
		{
			auto const pNextChild = static_cast<CItem* const>(pItem->GetChildAt(i));

			if ((pNextChild != nullptr) && (pNextChild->GetName().compareNoCase(token) == 0))
			{
				pChild = pNextChild;
				break;
			}
		}

		if (pChild != nullptr)
		{
			pItem = pChild;
			token = fullpath.Tokenize("/", start);
		}
		else
		{
			pItem = nullptr;
			break;
		}
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CImpl::CreatePlaceholderFolderPath(string const& path)
{
	CItem* pItem = &m_rootItem;
	int start = 0;
	string token = path.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CItem* pFoundChild = nullptr;
		size_t const numChildren = pItem->GetNumChildren();

		for (size_t i = 0; i < numChildren; ++i)
		{
			auto const pChild = static_cast<CItem* const>(pItem->GetChildAt(i));

			if ((pChild != nullptr) && (pChild->GetName().compareNoCase(token) == 0))
			{
				pFoundChild = pChild;
				break;
			}
		}

		if (pFoundChild == nullptr)
		{
			pFoundChild = CreatePlaceholderItem(token, EItemType::Folder, pItem);
		}

		pItem = pFoundChild;
		token = path.Tokenize("/", start);
	}

	return pItem;
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
