// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include "Common.h"
#include "GenericConnection.h"
#include "ParameterConnection.h"
#include "ParameterToStateConnection.h"
#include "ProjectLoader.h"
#include "DataPanel.h"

#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
constexpr uint32 g_itemPoolSize = 8192;
constexpr uint32 g_genericConnectionPoolSize = 8192;
constexpr uint32 g_parameterConnectionPoolSize = 512;
constexpr uint32 g_parameterToStateConnectionPoolSize = 256;

//////////////////////////////////////////////////////////////////////////
EItemType TagToType(char const* const szTag)
{
	EItemType type = EItemType::None;

	if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szEventTag) == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szFileTag) == 0)
	{
		type = EItemType::SoundBank;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szParameterTag) == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szSwitchGroupTag) == 0)
	{
		type = EItemType::SwitchGroup;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szStateGroupTag) == 0)
	{
		type = EItemType::StateGroup;
	}
	else if (_stricmp(szTag, CryAudio::Impl::Wwise::g_szAuxBusTag) == 0)
	{
		type = EItemType::AuxBus;
	}

	// Backwards compatibility will be removed before March 2019.
#if defined (USE_BACKWARDS_COMPATIBILITY)
	else if (_stricmp(szTag, "WwiseEvent") == 0)
	{
		type = EItemType::Event;
	}
	else if (_stricmp(szTag, "WwiseFile") == 0)
	{
		type = EItemType::SoundBank;
	}
	else if (_stricmp(szTag, "WwiseRtpc") == 0)
	{
		type = EItemType::Parameter;
	}
	else if (_stricmp(szTag, "WwiseSwitch") == 0)
	{
		type = EItemType::SwitchGroup;
	}
	else if (_stricmp(szTag, "WwiseState") == 0)
	{
		type = EItemType::StateGroup;
	}
	else if (_stricmp(szTag, "WwiseAuxBus") == 0)
	{
		type = EItemType::AuxBus;
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
			szTag = CryAudio::Impl::Wwise::g_szEventTag;
			break;
		}
	case EItemType::Parameter:
		{
			szTag = CryAudio::Impl::Wwise::g_szParameterTag;
			break;
		}
	case EItemType::Switch:
		{
			szTag = CryAudio::Impl::Wwise::g_szValueTag;
			break;
		}
	case EItemType::AuxBus:
		{
			szTag = CryAudio::Impl::Wwise::g_szAuxBusTag;
			break;
		}
	case EItemType::SoundBank:
		{
			szTag = CryAudio::Impl::Wwise::g_szFileTag;
			break;
		}
	case EItemType::State:
		{
			szTag = CryAudio::Impl::Wwise::g_szValueTag;
			break;
		}
	case EItemType::SwitchGroup:
		{
			szTag = CryAudio::Impl::Wwise::g_szSwitchGroupTag;
			break;
		}
	case EItemType::StateGroup:
		{
			szTag = CryAudio::Impl::Wwise::g_szStateGroupTag;
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
void CountConnections(
	EAssetType const assetType,
	EItemType const itemType,
	CryAudio::ContextId const contextId)
{
	switch (itemType)
	{
	case EItemType::Event:
		{
			++g_connections[contextId].events;
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
	case EItemType::State:
		{
			++g_connections[contextId].states;
			break;
		}
	case EItemType::Switch:
		{
			++g_connections[contextId].switches;
			break;
		}
	case EItemType::AuxBus:
		{
			++g_connections[contextId].auxBuses;
			break;
		}
	case EItemType::SoundBank:
		{
			++g_connections[contextId].soundBanks;
			break;
		}
	default:
		{
			break;
		}
	}
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
	, m_projectPath(CRY_AUDIO_DATA_ROOT "/wwise_project")
	, m_assetsPath(CRY_AUDIO_DATA_ROOT "/" + string(CryAudio::Impl::Wwise::g_szImplFolderName) + "/" + string(CryAudio::g_szAssetsFolderName))
	, m_localizedAssetsPath(m_assetsPath)
	, m_szUserSettingsFile("%USER%/audiocontrolseditor_wwise.user")
{
}

//////////////////////////////////////////////////////////////////////////
CImpl::~CImpl()
{
	Clear();
	DestroyDataPanel();

	CItem::FreeMemoryPool();
	CGenericConnection::FreeMemoryPool();
	CParameterConnection::FreeMemoryPool();
	CParameterToStateConnection::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Initialize(
	SImplInfo& implInfo,
	ExtensionFilterVector& extensionFilters,
	QStringList& supportedFileTypes)
{
	CItem::CreateAllocator(g_itemPoolSize);
	CGenericConnection::CreateAllocator(g_genericConnectionPoolSize);
	CParameterConnection::CreateAllocator(g_parameterConnectionPoolSize);
	CParameterToStateConnection::CreateAllocator(g_parameterToStateConnectionPoolSize);

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

	CProjectLoader(m_projectPath, m_assetsPath, m_localizedAssetsPath, m_rootItem, m_itemCache);

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
			isCompatible = (implType == EItemType::Event);
			break;
		}
	case EAssetType::Parameter:
		{
			isCompatible = (implType == EItemType::Parameter);
			break;
		}
	case EAssetType::State:
		{
			isCompatible = (implType == EItemType::Switch) || (implType == EItemType::State) || (implType == EItemType::Parameter);
			break;
		}
	case EAssetType::Environment:
		{
			isCompatible = (implType == EItemType::AuxBus) || (implType == EItemType::Parameter);
			break;
		}
	case EAssetType::Preload:
		{
			isCompatible = (implType == EItemType::SoundBank);
			break;
		}
	default:
		{
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
	case EItemType::Event:
		{
			assetType = EAssetType::Trigger;
			break;
		}
	case EItemType::Parameter:
		{
			assetType = EAssetType::Parameter;
			break;
		}
	case EItemType::Switch: // Intentional fall-through.
	case EItemType::State:
		{
			assetType = EAssetType::State;
			break;
		}
	case EItemType::AuxBus:
		{
			assetType = EAssetType::Environment;
			break;
		}
	case EItemType::SoundBank:
		{
			assetType = EAssetType::Preload;
			break;
		}
	case EItemType::StateGroup: // Intentional fall-through.
	case EItemType::SwitchGroup:
		{
			assetType = EAssetType::Switch;
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

	if (pItem->GetType() == EItemType::Parameter)
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
				pIConnection = static_cast<IConnection*>(new CParameterToStateConnection(pItem->GetId()));
				break;
			}
		default:
			{
				pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
				break;
			}
		}
	}
	else
	{
		pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
	}

	return pIConnection;
}

//////////////////////////////////////////////////////////////////////////
IConnection* CImpl::CreateConnectionFromXMLNode(XmlNodeRef const& node, EAssetType const assetType)
{
	IConnection* pIConnection = nullptr;

	if (node.isValid())
	{
		EItemType const type = TagToType(node->getTag());

		if (type != EItemType::None)
		{
			string name = node->getAttr(CryAudio::g_szNameAttribute);
			string localizedAttribute = node->getAttr(CryAudio::Impl::Wwise::g_szLocalizedAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && node->haveAttr("wwise_name"))
			{
				name = node->getAttr("wwise_name");
			}

			if (localizedAttribute.IsEmpty() && node->haveAttr("wwise_localised"))
			{
				localizedAttribute = node->getAttr("wwise_localised");
			}
#endif      // USE_BACKWARDS_COMPATIBILITY
			bool const isLocalized = (localizedAttribute.compareNoCase(CryAudio::Impl::Wwise::g_szTrueValue) == 0);

			CItem* pItem = SearchForItem(&m_rootItem, name, type);

			// If item not found, create a placeholder.
			// We want to keep that connection even if it's not in the middleware.
			// The user could be using the engine without the wwise project
			if (pItem == nullptr)
			{
				ControlId const id = GenerateID(name, isLocalized, &m_rootItem);
				EItemFlags const flags = isLocalized ? (EItemFlags::IsPlaceHolder | EItemFlags::IsLocalized) : EItemFlags::IsPlaceHolder;

				pItem = new CItem(name, id, type, flags);

				m_itemCache[id] = pItem;
			}

			// If it's a switch we actually connect to one of the states within the switch
			if ((type == EItemType::SwitchGroup) || (type == EItemType::StateGroup))
			{
				if (node->getChildCount() == 1)
				{
					XmlNodeRef const childNode = node->getChild(0);

					if (childNode.isValid())
					{
						string childName = childNode->getAttr(CryAudio::g_szNameAttribute);

#if defined (USE_BACKWARDS_COMPATIBILITY)
						if (childName.IsEmpty() && childNode->haveAttr("wwise_name"))
						{
							childName = childNode->getAttr("wwise_name");
						}
#endif            // USE_BACKWARDS_COMPATIBILITY

						CItem* pStateItem = nullptr;
						size_t const numChildren = pItem->GetNumChildren();

						for (size_t i = 0; i < numChildren; ++i)
						{
							auto const pChild = static_cast<CItem* const>(pItem->GetChildAt(i));

							if ((pChild != nullptr) && (pChild->GetName().compareNoCase(childName) == 0))
							{
								pStateItem = pChild;
							}
						}

						if (pStateItem == nullptr)
						{
							ControlId const id = GenerateID(childName);
							pStateItem = new CItem(childName, id, type == EItemType::SwitchGroup ? EItemType::Switch : EItemType::State, EItemFlags::IsPlaceHolder);
							pItem->AddChild(pStateItem);

							m_itemCache[id] = pStateItem;
						}

						pItem = pStateItem;
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] [Wwise] Error reading connection to Wwise control %s", name);
				}
			}

			if (pItem != nullptr)
			{
				if (type == EItemType::Parameter)
				{
					switch (assetType)
					{
					case EAssetType::Parameter: // Intentional fall-through.
					case EAssetType::Environment:
						{

							float mult = CryAudio::Impl::Wwise::g_defaultParamMultiplier;
							float shift = CryAudio::Impl::Wwise::g_defaultParamShift;

							node->getAttr(CryAudio::Impl::Wwise::g_szMutiplierAttribute, mult);
							node->getAttr(CryAudio::Impl::Wwise::g_szShiftAttribute, shift);
#if defined (USE_BACKWARDS_COMPATIBILITY)
							if (node->haveAttr("wwise_value_multiplier"))
							{
								node->getAttr("wwise_value_multiplier", mult);
							}
							if (node->haveAttr("wwise_value_shift"))
							{
								node->getAttr("wwise_value_shift", shift);
							}
#endif              // USE_BACKWARDS_COMPATIBILITY

							pIConnection = static_cast<IConnection*>(new CParameterConnection(pItem->GetId(), mult, shift));

							break;
						}
					case EAssetType::State:
						{
							float value = CryAudio::Impl::Wwise::g_defaultStateValue;

							node->getAttr(CryAudio::Impl::Wwise::g_szValueAttribute, value);
#if defined (USE_BACKWARDS_COMPATIBILITY)
							if (node->haveAttr("wwise_value"))
							{
								node->getAttr("wwise_value", value);
							}
#endif              // USE_BACKWARDS_COMPATIBILITY

							pIConnection = static_cast<IConnection*>(new CParameterToStateConnection(pItem->GetId(), value));

							break;
						}
					default:
						{
							pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
							break;
						}
					}
				}
				else
				{
					pIConnection = static_cast<IConnection*>(new CGenericConnection(pItem->GetId()));
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
		auto const itemType = static_cast<EItemType>(pItem->GetType());

		switch (itemType)
		{
		case EItemType::Switch:      // Intentional fall-through.
		case EItemType::SwitchGroup: // Intentional fall-through.
		case EItemType::State:       // Intentional fall-through.
		case EItemType::StateGroup:
			{
				auto const pParent = static_cast<CItem const* const>(pItem->GetParent());

				if (pParent != nullptr)
				{
					XmlNodeRef const switchNode = GetISystem()->CreateXmlNode(TypeToTag(pParent->GetType()));
					switchNode->setAttr(CryAudio::g_szNameAttribute, pParent->GetName());

					XmlNodeRef const stateNode = switchNode->createNode(CryAudio::Impl::Wwise::g_szValueTag);
					stateNode->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());
					switchNode->addChild(stateNode);

					node = switchNode;
				}

				break;
			}
		case EItemType::Parameter:
			{
				XmlNodeRef connectionNode;
				connectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				connectionNode->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());

				if ((assetType == EAssetType::Parameter) || (assetType == EAssetType::Environment))
				{
					auto const pParameterConnection = static_cast<CParameterConnection const*>(pIConnection);

					float const mult = pParameterConnection->GetMultiplier();

					if (mult != CryAudio::Impl::Wwise::g_defaultParamMultiplier)
					{
						connectionNode->setAttr(CryAudio::Impl::Wwise::g_szMutiplierAttribute, mult);
					}

					float const shift = pParameterConnection->GetShift();

					if (shift != CryAudio::Impl::Wwise::g_defaultParamShift)
					{
						connectionNode->setAttr(CryAudio::Impl::Wwise::g_szShiftAttribute, shift);
					}

				}
				else if (assetType == EAssetType::State)
				{
					auto const pStateConnection = static_cast<CParameterToStateConnection const*>(pIConnection);
					connectionNode->setAttr(CryAudio::Impl::Wwise::g_szValueAttribute, pStateConnection->GetValue());
				}

				node = connectionNode;

				break;
			}
		case EItemType::Event: // Intentional fall-through.
		case EItemType::AuxBus:
			{
				XmlNodeRef connectionNode;
				connectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				connectionNode->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());
				node = connectionNode;

				break;
			}
		case EItemType::SoundBank:
			{
				XmlNodeRef connectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				connectionNode->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());

				if ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0)
				{
					connectionNode->setAttr(CryAudio::Impl::Wwise::g_szLocalizedAttribute, CryAudio::Impl::Wwise::g_szTrueValue);
				}

				node = connectionNode;

				break;
			}
		default:
			{
				break;
			}
		}

		CountConnections(assetType, itemType, contextId);
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
			node->setAttr(CryAudio::Impl::Wwise::g_szEventsAttribute, g_connections[contextId].events);
		}

		if (g_connections[contextId].parameters > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szParametersAttribute, g_connections[contextId].parameters);
		}

		if (g_connections[contextId].parameterEnvironments > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szParameterEnvironmentsAttribute, g_connections[contextId].parameterEnvironments);
		}

		if (g_connections[contextId].parameterStates > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szParameterStatesAttribute, g_connections[contextId].parameterStates);
		}

		if (g_connections[contextId].states > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szStatesAttribute, g_connections[contextId].states);
		}

		if (g_connections[contextId].switches > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szSwitchesAttribute, g_connections[contextId].switches);
		}

		if (g_connections[contextId].auxBuses > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szAuxBusesAttribute, g_connections[contextId].auxBuses);
		}

		if (g_connections[contextId].soundBanks > 0)
		{
			node->setAttr(CryAudio::Impl::Wwise::g_szSoundBanksAttribute, g_connections[contextId].soundBanks);
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
	auto const pItem = static_cast<CItem* const>(GetItem(pIConnection->GetID()));

	if (pItem != nullptr)
	{
		++m_connectionsByID[pItem->GetId()];
		pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsConnected);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DisableConnection(IConnection const* const pIConnection, bool const isLoading)
{
	auto const pItem = static_cast<CItem* const>(GetItem(pIConnection->GetID()));

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

	m_itemCache.clear();
	m_rootItem.Clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetImplInfo(SImplInfo& implInfo)
{
	SetLocalizedAssetsPath();

	implInfo.name = m_implName.c_str();
	implInfo.folderName = CryAudio::Impl::Wwise::g_szImplFolderName;
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
		char const* const szLanguage = pCVar->GetString();

		if (szLanguage != nullptr)
		{
			m_localizedAssetsPath = PathUtil::GetLocalizationFolder().c_str();
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += szLanguage;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CRY_AUDIO_DATA_ROOT;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CryAudio::Impl::Wwise::g_szImplFolderName;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CryAudio::g_szAssetsFolderName;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ControlId CImpl::GenerateID(string const& fullPathName) const
{
	return CryAudio::StringToId(fullPathName);
}

//////////////////////////////////////////////////////////////////////////
ControlId CImpl::GenerateID(string const& name, bool isLocalized, CItem* pParent) const
{
	string pathName = (pParent != nullptr && !pParent->GetName().empty()) ? pParent->GetName() + "/" + name : name;

	if (isLocalized)
	{
		pathName = PathUtil::GetLocalizationFolder() + "/" + pathName;
	}

	return GenerateID(pathName);
}
} // namespace Wwise
} // namespace Impl
} // namespace ACE
