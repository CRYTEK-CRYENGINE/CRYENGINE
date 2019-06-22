// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include "Common.h"
#include "EventConnection.h"
#include "GenericConnection.h"
#include "KeyConnection.h"
#include "ParameterConnection.h"
#include "ParameterToStateConnection.h"
#include "SnapshotConnection.h"
#include "ProjectLoader.h"
#include "DataPanel.h"
#include "Utils.h"

#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
std::vector<string> CImpl::s_programmerSoundEvents;

constexpr uint32 g_itemPoolSize = 8192;
constexpr uint32 g_eventConnectionPoolSize = 8192;
constexpr uint32 g_keyConnectionPoolSize = 4096;
constexpr uint32 g_parameterConnectionPoolSize = 512;
constexpr uint32 g_parameterToStateConnectionPoolSize = 256;
constexpr uint32 g_snapshotConnectionPoolSize = 128;
constexpr uint32 g_genericConnectionPoolSize = 128;

//////////////////////////////////////////////////////////////////////////
EItemType TagToType(char const* const szTag)
{
	EItemType type = EItemType::None;

	if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szEventTag) == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szParameterTag) == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szKeyTag) == 0)
	{
		type = EItemType::Key;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szSnapshotTag) == 0)
	{
		type = EItemType::Snapshot;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szFileTag) == 0)
	{
		type = EItemType::Bank;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szBusTag) == 0)
	{
		type = EItemType::Return;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Fmod::g_szVcaTag) == 0)
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
		{
			szTag = CryAudio::Impl::Fmod::g_szEventTag;
			break;
		}
	case EItemType::Key:
		{
			szTag = CryAudio::Impl::Fmod::g_szKeyTag;
			break;
		}
	case EItemType::Parameter:
		{
			szTag = CryAudio::Impl::Fmod::g_szParameterTag;
			break;
		}
	case EItemType::Snapshot:
		{
			szTag = CryAudio::Impl::Fmod::g_szSnapshotTag;
			break;
		}
	case EItemType::Bank:
		{
			szTag = CryAudio::Impl::Fmod::g_szFileTag;
			break;
		}
	case EItemType::Return:
		{
			szTag = CryAudio::Impl::Fmod::g_szBusTag;
			break;
		}
	case EItemType::VCA:
		{
			szTag = CryAudio::Impl::Fmod::g_szVcaTag;
			break;
		}
	default:
		{
			szTag = nullptr;
			break;
		}
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
		{
			folderName = s_eventsFolderName + "/";
			break;
		}
	case EItemType::Key:
		{
			folderName = s_keysFolderName + "/";
			break;
		}
	case EItemType::Parameter:
		{
			folderName = s_parametersFolderName + "/";
			break;
		}
	case EItemType::Snapshot:
		{
			folderName = s_snapshotsFolderName + "/";
			break;
		}
	case EItemType::Bank:
		{
			folderName = s_soundBanksFolderName + "/";
			break;
		}
	case EItemType::Return:
		{
			folderName = s_returnsFolderName + "/";
			break;
		}
	case EItemType::VCA:
		{
			folderName = s_vcasFolderName + "/";
			break;
		}
	default:
		{
			folderName = "";
			break;
		}
	}

	return folderName;
}

//////////////////////////////////////////////////////////////////////////
void CountConnections(
	EAssetType const assetType,
	EItemType const itemType,
	CryAudio::ContextId const contextId)
{
	switch (itemType)
	{
	case EItemType::Event: // Intentional fall-through.
	case EItemType::Key:
		{
			++g_connections[contextId].events;
			break;
		}

	case EItemType::Snapshot:
		{
			++g_connections[contextId].snapshots;
			break;
		}
	case EItemType::Parameter:
		{
			switch (assetType)
			{
			case EAssetType::Parameter:
				{
					++g_connections[contextId].parameters;
					break;
				}
			case EAssetType::State:
				{
					++g_connections[contextId].parameterStates;
					break;
				}
			case EAssetType::Environment:
				{
					++g_connections[contextId].parameterEnvironments;
					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case EItemType::VCA:
		{
			switch (assetType)
			{
			case EAssetType::Parameter:
				{
					++g_connections[contextId].vcas;
					break;
				}
			case EAssetType::State:
				{
					++g_connections[contextId].vcaStates;
					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case EItemType::Return:
		{
			++g_connections[contextId].returns;
			break;
		}
	case EItemType::Bank:
		{
			++g_connections[contextId].banks;
			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pDataPanel(nullptr)
	, m_projectPath(CRY_AUDIO_DATA_ROOT "/fmod_project")
	, m_assetsPath(CRY_AUDIO_DATA_ROOT "/" + string(CryAudio::Impl::Fmod::g_szImplFolderName) + "/" + string(CryAudio::g_szAssetsFolderName))
	, m_localizedAssetsPath(m_assetsPath)
	, m_szUserSettingsFile("%USER%/audiocontrolseditor_fmod.user")
{
}

//////////////////////////////////////////////////////////////////////////
CImpl::~CImpl()
{
	Clear();
	DestroyDataPanel();

	CItem::FreeMemoryPool();
	CEventConnection::FreeMemoryPool();
	CKeyConnection::FreeMemoryPool();
	CParameterConnection::FreeMemoryPool();
	CParameterToStateConnection::FreeMemoryPool();
	CSnapshotConnection::FreeMemoryPool();
	CGenericConnection::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Initialize(
	SImplInfo& implInfo,
	ExtensionFilterVector& extensionFilters,
	QStringList& supportedFileTypes)
{
	CItem::CreateAllocator(g_itemPoolSize);
	CEventConnection::CreateAllocator(g_eventConnectionPoolSize);
	CKeyConnection::CreateAllocator(g_keyConnectionPoolSize);
	CParameterConnection::CreateAllocator(g_parameterConnectionPoolSize);
	CParameterToStateConnection::CreateAllocator(g_parameterToStateConnectionPoolSize);
	CSnapshotConnection::CreateAllocator(g_snapshotConnectionPoolSize);
	CGenericConnection::CreateAllocator(g_genericConnectionPoolSize);

	CryAudio::SImplInfo systemImplInfo;
	gEnv->pAudioSystem->GetImplInfo(systemImplInfo);
	m_implName = systemImplInfo.name.c_str();

	SetImplInfo(implInfo);

	Serialization::LoadJsonFile(*this, m_szUserSettingsFile);
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
void CImpl::Reload(SImplInfo& implInfo)
{
	Clear();
	SetImplInfo(implInfo);

	CProjectLoader(m_projectPath, m_assetsPath, m_localizedAssetsPath, m_rootItem, m_itemCache, *this);

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
	CRY_ASSERT_MESSAGE(pItem != nullptr, "Impl item is null pointer during %s", __FUNCTION__);
	return GetTypeIcon(pItem->GetType());
}

//////////////////////////////////////////////////////////////////////////
QString const& CImpl::GetItemTypeName(IItem const* const pIItem) const
{
	auto const pItem = static_cast<CItem const* const>(pIItem);
	CRY_ASSERT_MESSAGE(pItem != nullptr, "Impl item is null pointer during %s", __FUNCTION__);
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
	EItemType const implType = pItem->GetType();

	switch (assetType)
	{
	case EAssetType::Trigger:
		{
			isCompatible = (implType == EItemType::Event) || (implType == EItemType::Key) || (implType == EItemType::Snapshot);
			break;
		}
	case EAssetType::Parameter:
		{
			isCompatible = (implType == EItemType::Parameter) || (implType == EItemType::VCA);
			break;
		}
	case EAssetType::State:
		{
			isCompatible = (implType == EItemType::Parameter) || (implType == EItemType::VCA);
			break;
		}
	case EAssetType::Environment:
		{
			isCompatible = (implType == EItemType::Return) || (implType == EItemType::Parameter);
			break;
		}
	case EAssetType::Preload:
		{
			isCompatible = (implType == EItemType::Bank);
			break;
		}
	default:
		{
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

	switch (pItem->GetType())
	{
	case EItemType::Event: // Intentional fall-through.
	case EItemType::Key:   // Intentional fall-through.
	case EItemType::Snapshot:
		{
			assetType = EAssetType::Trigger;
			break;
		}
	case EItemType::Parameter: // Intentional fall-through.
	case EItemType::VCA:
		{
			assetType = EAssetType::Parameter;
			break;
		}
	case EItemType::Bank:
		{
			assetType = EAssetType::Preload;
			break;
		}
	case EItemType::Return:
		{
			assetType = EAssetType::Environment;
			break;
		}
	default:
		{
			assetType = EAssetType::None;
			break;
		}
	}

	return assetType;
}

//////////////////////////////////////////////////////////////////////////
IConnection* CImpl::CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem)
{
	IConnection* pIConnection = nullptr;
	auto const pItem = static_cast<CItem const* const>(pIItem);

	switch (pItem->GetType())
	{
	case EItemType::Event:
		{
			pIConnection = static_cast<IConnection*>(new CEventConnection(pItem->GetId()));
			break;
		}
	case EItemType::Key:
		{
			pIConnection = static_cast<IConnection*>(new CKeyConnection(pItem->GetId()));
			break;
		}
	case EItemType::Snapshot:
		{
			pIConnection = static_cast<IConnection*>(new CSnapshotConnection(pItem->GetId()));
			break;
		}
	case EItemType::Parameter: // Intentional fall-through.
	case EItemType::VCA:
		{
			switch (assetType)
			{
			case EAssetType::Parameter: // Intentional fall-through.
			case EAssetType::Environment:
				{
					pIConnection = static_cast<IConnection*>(new CParameterConnection(pItem->GetId()));
					break;
				}
			case EAssetType::State:
				{
					pIConnection = static_cast<IConnection*>(new CParameterToStateConnection(pItem->GetId(), pItem->GetType()));
					break;
				}
			default:
				{
					pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
					break;
				}
			}

			break;
		}
	default:
		{
			pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
			break;
		}
	}

	return pIConnection;
}

//////////////////////////////////////////////////////////////////////////
IConnection* CImpl::CreateConnectionFromXMLNode(XmlNodeRef const& node, EAssetType const assetType)
{
	IConnection* pIConnection = nullptr;

	if (node.isValid())
	{
		auto const type = TagToType(node->getTag());

		if (type != EItemType::None)
		{
			string name = node->getAttr(CryAudio::g_szNameAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && node->haveAttr("fmod_name"))
			{
				name = node->getAttr("fmod_name");
			}
#endif      // USE_BACKWARDS_COMPATIBILITY

			CItem* pItem = GetItemFromPath(TypeToEditorFolderName(type) + name);

			if ((pItem == nullptr) || (type != pItem->GetType()))
			{
				// If item not found, create a placeholder.
				// We want to keep that connection even if it's not in the middleware as the user could
				// be using the engine without the fmod project

				string localizedAttribute = node->getAttr(CryAudio::Impl::Fmod::g_szLocalizedAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
				if (localizedAttribute.IsEmpty() && node->haveAttr("fmod_localized"))
				{
					localizedAttribute = node->getAttr("fmod_localized");
				}
#endif        // USE_BACKWARDS_COMPATIBILITY
				bool const isLocalized = (localizedAttribute.compareNoCase(CryAudio::Impl::Fmod::g_szTrueValue) == 0);

				string path = "";
				int const pos = name.find_last_of("/");

				if (pos != string::npos)
				{
					path = name.substr(0, pos);
					name = name.substr(pos + 1, name.length() - pos);
				}

				pItem = CreatePlaceholderItem(name, type, isLocalized, CreatePlaceholderFolderPath(path));
			}

			switch (type)
			{
			case EItemType::Event:
				{
					string actionType = node->getAttr(CryAudio::g_szTypeAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
					if (actionType.IsEmpty() && node->haveAttr("fmod_event_type"))
					{
						actionType = node->getAttr("fmod_event_type");
					}
#endif          // USE_BACKWARDS_COMPATIBILITY

					CEventConnection::EActionType eventActionType = CEventConnection::EActionType::Start;

					if (actionType.compareNoCase(CryAudio::Impl::Fmod::g_szStopValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Stop;
					}
					else if (actionType.compareNoCase(CryAudio::Impl::Fmod::g_szPauseValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Pause;
					}
					else if (actionType.compareNoCase(CryAudio::Impl::Fmod::g_szResumeValue) == 0)
					{
						eventActionType = CEventConnection::EActionType::Resume;
					}

					pIConnection = static_cast<IConnection*>(new CEventConnection(pItem->GetId(), eventActionType));

					break;
				}
			case EItemType::Key:
				{
					string const eventName = node->getAttr(CryAudio::Impl::Fmod::g_szEventAttribute);

					pIConnection = static_cast<IConnection*>(new CKeyConnection(pItem->GetId(), eventName));

					break;
				}
			case EItemType::Snapshot:
				{
					string actionType = node->getAttr(CryAudio::g_szTypeAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
					if (actionType.IsEmpty() && node->haveAttr("fmod_event_type"))
					{
						actionType = node->getAttr("fmod_event_type");
					}
#endif          // USE_BACKWARDS_COMPATIBILITY

					CSnapshotConnection::EActionType const snapshotActionType = (actionType.compareNoCase(CryAudio::Impl::Fmod::g_szStopValue) == 0) ? CSnapshotConnection::EActionType::Stop : CSnapshotConnection::EActionType::Start;
					pIConnection = static_cast<IConnection*>(new CSnapshotConnection(pItem->GetId(), snapshotActionType));

					break;
				}
			case EItemType::Parameter: // Intentional fall-through.
			case EItemType::VCA:
				{
					switch (assetType)
					{
					case EAssetType::Parameter: // Intentional fall-through.
					case EAssetType::Environment:
						{
							float mult = CryAudio::Impl::Fmod::g_defaultParamMultiplier;
							float shift = CryAudio::Impl::Fmod::g_defaultParamShift;

							if (node->haveAttr(CryAudio::Impl::Fmod::g_szMutiplierAttribute))
							{
								string const value = node->getAttr(CryAudio::Impl::Fmod::g_szMutiplierAttribute);
								mult = (float)std::atof(value.c_str());
							}
#if defined (USE_BACKWARDS_COMPATIBILITY)
							else if (node->haveAttr("fmod_value_multiplier"))
							{
								string const value = node->getAttr("fmod_value_multiplier");
								mult = (float)std::atof(value.c_str());
							}
#endif              // USE_BACKWARDS_COMPATIBILITY
							if (node->haveAttr(CryAudio::Impl::Fmod::g_szShiftAttribute))
							{
								string const value = node->getAttr(CryAudio::Impl::Fmod::g_szShiftAttribute);
								shift = (float)std::atof(value.c_str());
							}
#if defined (USE_BACKWARDS_COMPATIBILITY)
							else if (node->haveAttr("fmod_value_shift"))
							{
								string const value = node->getAttr("fmod_value_shift");
								shift = (float)std::atof(value.c_str());
							}
#endif              // USE_BACKWARDS_COMPATIBILITY

							pIConnection = static_cast<IConnection*>(new CParameterConnection(pItem->GetId(), mult, shift));

							break;
						}
					case EAssetType::State:
						{
							string valueString = node->getAttr(CryAudio::Impl::Fmod::g_szValueAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
							if (valueString.IsEmpty() && node->haveAttr("fmod_value"))
							{
								valueString = node->getAttr("fmod_value");
							}
#endif              // USE_BACKWARDS_COMPATIBILITY

							auto const valueFloat = static_cast<float>(std::atof(valueString.c_str()));
							pIConnection = static_cast<IConnection*>(new CParameterToStateConnection(pItem->GetId(), type, valueFloat));

							break;
						}
					default:
						{
							break;
						}
					}

					break;
				}
			case EItemType::Bank: // Intentional fall-through.
			case EItemType::Return:
				{
					pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));

					break;
				}
			default:
				{
					break;
				}
			}
		}
	}

	return pIConnection;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CImpl::CreateXMLNodeFromConnection(
	IConnection const* const pIConnection,
	EAssetType const assetType,
	CryAudio::ContextId const contextId)
{
	XmlNodeRef node;

	auto const pItem = static_cast<CItem const*>(GetItem(pIConnection->GetID()));

	if (pItem != nullptr)
	{
		auto const type = pItem->GetType();
		node = GetISystem()->CreateXmlNode(TypeToTag(type));

		switch (type)
		{
		case EItemType::Event:
			{
				node->setAttr(CryAudio::g_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
				auto const pEventConnection = static_cast<CEventConnection const*>(pIConnection);

				if (pEventConnection != nullptr)
				{
					CEventConnection::EActionType const actionType = pEventConnection->GetActionType();

					switch (actionType)
					{
					case CEventConnection::EActionType::Stop:
						{
							node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::Fmod::g_szStopValue);
							break;
						}
					case CEventConnection::EActionType::Pause:
						{
							node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::Fmod::g_szPauseValue);
							break;
						}
					case CEventConnection::EActionType::Resume:
						{
							node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::Fmod::g_szResumeValue);
							break;
						}
					default:
						{
							break;
						}
					}
				}

				break;
			}
		case EItemType::Key:
			{
				node->setAttr(CryAudio::g_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
				auto const pKeyConnection = static_cast<CKeyConnection const*>(pIConnection);

				if (pKeyConnection != nullptr)
				{
					node->setAttr(CryAudio::Impl::Fmod::g_szEventAttribute, pKeyConnection->GetEvent());
				}

				break;
			}
		case EItemType::Snapshot:
			{
				node->setAttr(CryAudio::g_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));
				auto const pEventConnection = static_cast<CSnapshotConnection const*>(pIConnection);

				if ((pEventConnection != nullptr) && (pEventConnection->GetActionType() == CSnapshotConnection::EActionType::Stop))
				{
					node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::Fmod::g_szStopValue);
				}

				break;
			}
		case EItemType::Return:
			{
				node->setAttr(CryAudio::g_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));

				break;
			}
		case EItemType::Parameter:
			{
				node->setAttr(CryAudio::g_szNameAttribute, Utils::GetPathName(pItem, m_rootItem));

				switch (assetType)
				{
				case EAssetType::Parameter: // Intentional fall-through.
				case EAssetType::Environment:
					{
						auto const pParamConnection = static_cast<CParameterConnection const*>(pIConnection);

						if (pParamConnection->GetMultiplier() != CryAudio::Impl::Fmod::g_defaultParamMultiplier)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szMutiplierAttribute, pParamConnection->GetMultiplier());
						}

						if (pParamConnection->GetShift() != CryAudio::Impl::Fmod::g_defaultParamShift)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szShiftAttribute, pParamConnection->GetShift());
						}

						break;
					}
				case EAssetType::State:
					{
						auto const pStateConnection = static_cast<CParameterToStateConnection const*>(pIConnection);

						if (pStateConnection != nullptr)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szValueAttribute, pStateConnection->GetValue());
						}

						break;
					}
				default:
					{
						break;
					}
				}

				break;
			}
		case EItemType::VCA:
			{
				node->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());

				switch (assetType)
				{
				case EAssetType::Parameter:
					{
						auto const pParamConnection = static_cast<CParameterConnection const*>(pIConnection);

						if (pParamConnection->GetMultiplier() != CryAudio::Impl::Fmod::g_defaultParamMultiplier)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szMutiplierAttribute, pParamConnection->GetMultiplier());
						}

						if (pParamConnection->GetShift() != CryAudio::Impl::Fmod::g_defaultParamShift)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szShiftAttribute, pParamConnection->GetShift());
						}

						break;
					}
				case EAssetType::State:
					{
						auto const pStateConnection = static_cast<CParameterToStateConnection const*>(pIConnection);

						if (pStateConnection != nullptr)
						{
							node->setAttr(CryAudio::Impl::Fmod::g_szValueAttribute, pStateConnection->GetValue());
						}

						break;
					}
				default:
					{
						break;
					}
				}

				break;
			}
		case EItemType::Bank:
			{
				node->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());

				if ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0)
				{
					node->setAttr(CryAudio::Impl::Fmod::g_szLocalizedAttribute, CryAudio::Impl::Fmod::g_szTrueValue);
				}

				break;
			}
		default:
			{
				break;
			}
		}

		CountConnections(assetType, type, contextId);
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CImpl::SetDataNode(char const* const szTag, CryAudio::ContextId const contextId)
{
	XmlNodeRef node;

	if (g_connections.find(contextId) != g_connections.end())
	{
		node = GetISystem()->CreateXmlNode(szTag);

		if (g_connections[contextId].events > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szEventsAttribute, g_connections[contextId].events);
		}

		if (g_connections[contextId].parameters > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szParametersAttribute, g_connections[contextId].parameters);
		}

		if (g_connections[contextId].parameterEnvironments > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szParameterEnvironmentsAttribute, g_connections[contextId].parameterEnvironments);
		}

		if (g_connections[contextId].parameterStates > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szParameterStatesAttribute, g_connections[contextId].parameterStates);
		}

		if (g_connections[contextId].snapshots > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szSnapshotsAttribute, g_connections[contextId].snapshots);
		}

		if (g_connections[contextId].returns > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szReturnsAttribute, g_connections[contextId].returns);
		}

		if (g_connections[contextId].vcas > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szVcasAttribute, g_connections[contextId].vcas);
		}

		if (g_connections[contextId].vcaStates > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szVcaStatesAttribute, g_connections[contextId].vcaStates);
		}

		if (g_connections[contextId].banks > 0)
		{
			node->setAttr(CryAudio::Impl::Fmod::g_szBanksAttribute, g_connections[contextId].banks);
		}
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeWriteLibrary()
{
	g_connections.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterWriteLibrary()
{
	g_connections.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::EnableConnection(IConnection const* const pIConnection, bool const isLoading)
{
	auto const pItem = static_cast<CItem*>(GetItem(pIConnection->GetID()));

	if (pItem != nullptr)
	{
		++m_connectionsByID[pItem->GetId()];
		pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsConnected);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DisableConnection(IConnection const* const pIConnection, bool const isLoading)
{
	auto const pItem = static_cast<CItem*>(GetItem(pIConnection->GetID()));

	if (pItem != nullptr)
	{
		int connectionCount = m_connectionsByID[pItem->GetId()] - 1;

		if (connectionCount < 1)
		{
			CRY_ASSERT_MESSAGE(connectionCount >= 0, "Connection count is < 0 during %s", __FUNCTION__);
			connectionCount = 0;
			pItem->SetFlags(pItem->GetFlags() & ~EItemFlags::IsConnected);
		}

		m_connectionsByID[pItem->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructConnection(IConnection const* const pIConnection)
{
	delete pIConnection;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeReload()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnBeforeReload();
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterReload()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnAfterReload();
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

	s_programmerSoundEvents.clear();
	m_itemCache.clear();
	m_rootItem.Clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetImplInfo(SImplInfo& implInfo)
{
	SetLocalizedAssetsPath();

	implInfo.name = m_implName.c_str();
	implInfo.folderName = CryAudio::Impl::Fmod::g_szImplFolderName;
	implInfo.projectPath = m_projectPath.c_str();
	implInfo.assetsPath = m_assetsPath.c_str();
	implInfo.localizedAssetsPath = m_localizedAssetsPath.c_str();
	implInfo.flags = (
		EImplInfoFlags::SupportsProjects |
		EImplInfoFlags::SupportsTriggers |
		EImplInfoFlags::SupportsParameters |
		EImplInfoFlags::SupportsSwitches |
		EImplInfoFlags::SupportsStates |
		EImplInfoFlags::SupportsEnvironments |
		EImplInfoFlags::SupportsPreloads);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLocalizedAssetsPath()
{
	if (ICVar const* const pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		g_language = pCVar->GetString();

		if (!g_language.empty())
		{
			m_localizedAssetsPath = PathUtil::GetLocalizationFolder().c_str();
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += g_language;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CRY_AUDIO_DATA_ROOT;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CryAudio::Impl::Fmod::g_szImplFolderName;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CryAudio::g_szAssetsFolderName;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CImpl::CreatePlaceholderItem(string const& name, EItemType const type, bool const isLocalized, CItem* const pParent)
{
	ControlId const id = Utils::GetId(type, name, pParent, m_rootItem);

	auto pItem = static_cast<CItem*>(GetItem(id));

	if (pItem == nullptr)
	{
		EItemFlags const flags = isLocalized ? (EItemFlags::IsPlaceHolder | EItemFlags::IsLocalized) : EItemFlags::IsPlaceHolder;
		pItem = new CItem(name, id, type, flags);

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
			pFoundChild = CreatePlaceholderItem(token, EItemType::Folder, false, pItem);
		}

		pItem = pFoundChild;
		token = path.Tokenize("/", start);
	}

	return pItem;
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
