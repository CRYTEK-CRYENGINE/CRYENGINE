// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Impl.h"

#include "Common.h"
#include "EventConnection.h"
#include "ProjectLoader.h"
#include "DataPanel.h"
#include "Utils.h"

#include <QtUtil.h>
#include <CryAudioImplPortAudio/GlobalData.h>
#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySystem/XML/IXml.h>
#include <DragDrop.h>

#include <QDirIterator>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
constexpr uint32 g_itemPoolSize = 2048;
constexpr uint32 g_eventConnectionPoolSize = 2048;

//////////////////////////////////////////////////////////////////////////
bool HasDirValidData(QDir const& dir)
{
	bool hasValidData = false;

	if (dir.exists())
	{
		QDirIterator itFiles(dir.path(), (QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot));

		while (itFiles.hasNext())
		{
			QFileInfo const& fileInfo(itFiles.next());

			if (fileInfo.isFile())
			{
				hasValidData = true;
				break;
			}
		}

		if (!hasValidData)
		{
			QDirIterator itDirs(dir.path(), (QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot));

			while (itDirs.hasNext())
			{
				QDir const& folder(itDirs.next());

				if (HasDirValidData(folder))
				{
					hasValidData = true;
					break;
				}
			}
		}
	}

	return hasValidData;
}

//////////////////////////////////////////////////////////////////////////
void GetFilesFromDir(QDir const& dir, QString const& folderName, FileImportInfos& fileImportInfos)
{
	if (dir.exists())
	{
		QString const parentFolderName = (folderName.isEmpty() ? (dir.dirName() + "/") : (folderName + dir.dirName() + "/"));

		for (auto const& fileInfo : dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot))
		{
			if (fileInfo.isFile())
			{
				fileImportInfos.emplace_back(fileInfo, s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive), parentFolderName);
			}
		}

		for (auto const& fileInfo : dir.entryInfoList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
		{
			QDir const& folder(fileInfo.absoluteFilePath());
			GetFilesFromDir(folder, parentFolderName, fileImportInfos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pDataPanel(nullptr)
	, m_assetAndProjectPath(CRY_AUDIO_DATA_ROOT "/" +
	                        string(CryAudio::Impl::PortAudio::g_szImplFolderName) +
	                        "/" +
	                        string(CryAudio::g_szAssetsFolderName))
	, m_localizedAssetsPath(m_assetAndProjectPath)
{
}

//////////////////////////////////////////////////////////////////////////
CImpl::~CImpl()
{
	Clear();
	DestroyDataPanel();

	CItem::FreeMemoryPool();
	CEventConnection::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::Initialize(
	SImplInfo& implInfo,
	ExtensionFilterVector& extensionFilters,
	QStringList& supportedFileTypes)
{
	CItem::CreateAllocator(g_itemPoolSize);
	CEventConnection::CreateAllocator(g_eventConnectionPoolSize);

	CryAudio::SImplInfo systemImplInfo;
	gEnv->pAudioSystem->GetImplInfo(systemImplInfo);
	m_implName = systemImplInfo.name.c_str();

	SetImplInfo(implInfo);
	extensionFilters = s_extensionFilters;
	supportedFileTypes = s_supportedFileTypes;
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

	CProjectLoader(m_assetAndProjectPath, m_localizedAssetsPath, m_rootItem, m_itemCache, *this);

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
bool CImpl::IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const
{
	bool isCompatible = false;

	if (assetType == EAssetType::Trigger)
	{
		auto const pItem = static_cast<CItem const* const>(pIItem);
		isCompatible = (pItem->GetType() == EItemType::Event);
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
	return static_cast<IConnection*>(new CEventConnection(pIItem->GetId()));
}

//////////////////////////////////////////////////////////////////////////
IConnection* CImpl::CreateConnectionFromXMLNode(XmlNodeRef const& node, EAssetType const assetType)
{
	IConnection* pIConnection = nullptr;

	if (node.isValid())
	{
		char const* const szTag = node->getTag();

		if ((_stricmp(szTag, CryAudio::Impl::PortAudio::g_szEventTag) == 0) ||
		    (_stricmp(szTag, CryAudio::Impl::PortAudio::g_szSampleTag) == 0) ||
		    (_stricmp(szTag, "PortAudioEvent") == 0) || // Backwards compatibility.
		    (_stricmp(szTag, "PortAudioSample") == 0))  // Backwards compatibility.
		{
			string name = node->getAttr(CryAudio::g_szNameAttribute);
			string path = node->getAttr(CryAudio::Impl::PortAudio::g_szPathAttribute);
			// Backwards compatibility will be removed before March 2019.
#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && node->haveAttr("portaudio_name"))
			{
				name = node->getAttr("portaudio_name");
			}

			if (path.IsEmpty() && node->haveAttr("portaudio_path"))
			{
				path = node->getAttr("portaudio_path");
			}
#endif      // USE_BACKWARDS_COMPATIBILITY
			string const localizedAttribute = node->getAttr(CryAudio::Impl::PortAudio::g_szLocalizedAttribute);
			bool const isLocalized = (localizedAttribute.compareNoCase(CryAudio::Impl::PortAudio::g_szTrueValue) == 0);
			ControlId const id = Utils::GetId(EItemType::Event, name, path, isLocalized);

			auto pItem = static_cast<CItem*>(GetItem(id));

			if (pItem == nullptr)
			{
				EItemFlags const flags = (isLocalized ? (EItemFlags::IsPlaceHolder | EItemFlags::IsLocalized) : EItemFlags::IsPlaceHolder);
				pItem = new CItem(name, id, EItemType::Event, path, flags);
				m_itemCache[id] = pItem;
			}

			if (pItem != nullptr)
			{
				auto const pEventConnection = new CEventConnection(pItem->GetId());
				string actionType = node->getAttr(CryAudio::g_szTypeAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
				if (actionType.IsEmpty() && node->haveAttr("event_type"))
				{
					actionType = node->getAttr("event_type");
				}
#endif        // USE_BACKWARDS_COMPATIBILITY
				pEventConnection->SetActionType(actionType.compareNoCase(CryAudio::Impl::PortAudio::g_szStopValue) == 0 ? CEventConnection::EActionType::Stop : CEventConnection::EActionType::Start);

				int loopCount = 0;
				node->getAttr(CryAudio::Impl::PortAudio::g_szLoopCountAttribute, loopCount);
				loopCount = std::max(0, loopCount);
				pEventConnection->SetLoopCount(static_cast<uint32>(loopCount));

				if (pEventConnection->GetLoopCount() == 0)
				{
					pEventConnection->SetInfiniteLoop(true);
				}

				pIConnection = static_cast<IConnection*>(pEventConnection);
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

	auto const pEventConnection = static_cast<CEventConnection const*>(pIConnection);
	auto const pItem = static_cast<CItem const*>(GetItem(pIConnection->GetID()));

	if ((pItem != nullptr) && (pEventConnection != nullptr) && (assetType == EAssetType::Trigger))
	{
		node = GetISystem()->CreateXmlNode(CryAudio::Impl::PortAudio::g_szEventTag);
		node->setAttr(CryAudio::g_szNameAttribute, pItem->GetName());

		string const& path = pItem->GetPath();

		if (!path.IsEmpty())
		{
			node->setAttr(CryAudio::Impl::PortAudio::g_szPathAttribute, path.c_str());
		}

		if (pEventConnection->GetActionType() == CEventConnection::EActionType::Start)
		{
			node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::PortAudio::g_szStartValue);

			if (pEventConnection->IsInfiniteLoop())
			{
				node->setAttr(CryAudio::Impl::PortAudio::g_szLoopCountAttribute, 0);
			}
			else
			{
				node->setAttr(CryAudio::Impl::PortAudio::g_szLoopCountAttribute, pEventConnection->GetLoopCount());
			}
		}
		else
		{
			node->setAttr(CryAudio::g_szTypeAttribute, CryAudio::Impl::PortAudio::g_szStopValue);
		}

		if ((pItem->GetFlags() & EItemFlags::IsLocalized) != 0)
		{
			node->setAttr(CryAudio::Impl::PortAudio::g_szLocalizedAttribute, CryAudio::Impl::PortAudio::g_szTrueValue);
		}

		++g_connections[contextId];
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CImpl::SetDataNode(char const* const szTag, CryAudio::ContextId const contextId)
{
	XmlNodeRef node;

	if (g_connections.find(contextId) != g_connections.end())
	{
		if (g_connections[contextId] > 0)
		{
			node = GetISystem()->CreateXmlNode(szTag);
			node->setAttr(CryAudio::Impl::PortAudio::g_szEventsAttribute, g_connections[contextId]);
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
void CImpl::OnFileImporterOpened()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnFileImporterOpened();
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnFileImporterClosed()
{
	if (m_pDataPanel != nullptr)
	{
		m_pDataPanel->OnFileImporterClosed();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::CanDropExternalData(QMimeData const* const pData) const
{
	bool hasValidData = false;
	CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

	if (pDragDropData->HasFilePaths())
	{
		QStringList& allFiles = pDragDropData->GetFilePaths();

		for (auto const& filePath : allFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile() && s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive))
			{
				hasValidData = true;
				break;
			}
		}

		if (!hasValidData)
		{
			for (auto const& filePath : allFiles)
			{
				QDir const& folder(filePath);

				if (HasDirValidData(folder))
				{
					hasValidData = true;
					break;
				}
			}
		}
	}

	return hasValidData;
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::DropExternalData(QMimeData const* const pData, FileImportInfos& fileImportInfos) const
{
	CRY_ASSERT_MESSAGE(fileImportInfos.empty(), "Passed container must be empty during %s", __FUNCTION__);

	if (CanDropExternalData(pData))
	{
		CDragDropData const* const pDragDropData = CDragDropData::FromMimeData(pData);

		if (pDragDropData->HasFilePaths())
		{
			QStringList const& allFiles = pDragDropData->GetFilePaths();

			for (auto const& filePath : allFiles)
			{
				QFileInfo const& fileInfo(filePath);

				if (fileInfo.isFile())
				{
					fileImportInfos.emplace_back(fileInfo, s_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
				}
				else
				{
					QDir const& folder(filePath);
					GetFilesFromDir(folder, "", fileImportInfos);
				}
			}
		}
	}

	return !fileImportInfos.empty();
}

//////////////////////////////////////////////////////////////////////////
ControlId CImpl::GenerateItemId(QString const& name, QString const& path, bool const isLocalized)
{
	return Utils::GetId(EItemType::Event, QtUtil::ToString(name), QtUtil::ToString(path), isLocalized);
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
	implInfo.folderName = CryAudio::Impl::PortAudio::g_szImplFolderName;
	implInfo.projectPath = m_assetAndProjectPath.c_str();
	implInfo.assetsPath = m_assetAndProjectPath.c_str();
	implInfo.localizedAssetsPath = m_localizedAssetsPath.c_str();
	implInfo.flags = (
		EImplInfoFlags::SupportsFileImport |
		EImplInfoFlags::SupportsTriggers);
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
			m_localizedAssetsPath += CryAudio::Impl::PortAudio::g_szImplFolderName;
			m_localizedAssetsPath += "/";
			m_localizedAssetsPath += CryAudio::g_szAssetsFolderName;
		}
	}
}
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
