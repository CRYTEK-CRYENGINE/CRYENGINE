// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include "../Common/IImpl.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
class CDataPanel;

class CImpl final : public IImpl
{
public:

	CImpl(CImpl const&) = delete;
	CImpl(CImpl&&) = delete;
	CImpl& operator=(CImpl const&) = delete;
	CImpl& operator=(CImpl&&) = delete;

	CImpl();
	virtual ~CImpl() override;

	// IImpl
	virtual void           Initialize(SImplInfo& implInfo, ExtensionFilterVector& extensionFilters, QStringList& supportedFileTypes) override;
	virtual QWidget*       CreateDataPanel() override;
	virtual void           DestroyDataPanel() override;
	virtual void           Reload(SImplInfo& implInfo) override;
	virtual IItem*         GetItem(ControlId const id) const override;
	virtual CryIcon const& GetItemIcon(IItem const* const pIItem) const override;
	virtual QString const& GetItemTypeName(IItem const* const pIItem) const override;
	virtual void           SetProjectPath(char const* const szPath) override;
	virtual bool           IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const override;
	virtual EAssetType     ImplTypeToAssetType(IItem const* const pIItem) const override;
	virtual IConnection*   CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) override;
	virtual IConnection*   CreateConnectionFromXMLNode(XmlNodeRef const& node, EAssetType const assetType) override;
	virtual XmlNodeRef     CreateXMLNodeFromConnection(IConnection const* const pIConnection, EAssetType const assetType, CryAudio::ContextId const contextId) override;
	virtual XmlNodeRef     SetDataNode(char const* const szTag, CryAudio::ContextId const contextId) override;
	virtual void           OnBeforeWriteLibrary() override;
	virtual void           OnAfterWriteLibrary() override;
	virtual void           EnableConnection(IConnection const* const pIConnection, bool const isLoading) override;
	virtual void           DisableConnection(IConnection const* const pIConnection, bool const isLoading) override;
	virtual void           DestructConnection(IConnection const* const pIConnection) override;
	virtual void           OnBeforeReload() override;
	virtual void           OnAfterReload() override;
	virtual void           OnSelectConnectedItem(ControlId const id) const override;
	virtual void           OnFileImporterOpened() override                                                                 {}
	virtual void           OnFileImporterClosed() override                                                                 {}
	virtual bool           CanDropExternalData(QMimeData const* const pData) const override                                { return false; }
	virtual bool           DropExternalData(QMimeData const* const pData, FileImportInfos& fileImportInfos) const override { return false; }
	virtual ControlId      GenerateItemId(QString const& name, QString const& path, bool const isLocalized) override       { return g_invalidControlId; }
	// ~IImpl

	CItem const& GetRootItem() const { return m_rootItem; }

	void         Serialize(Serialization::IArchive& ar);

private:

	void   Clear();
	void   SetImplInfo(SImplInfo& implInfo);
	void   SetLocalizedAssetsPath();
	CItem* CreatePlaceholderItem(string const& name, EItemType const type, bool const isLocalized, CItem* const pParent);

	using ConnectionIds = std::map<ControlId, int>;

	CItem             m_rootItem { "", g_invalidControlId, EItemType::None };
	ItemCache         m_itemCache;   // cache of the items stored by id for faster access
	ConnectionIds     m_connectionsByID;
	CDataPanel*       m_pDataPanel;
	string            m_implName;
	string            m_projectPath;
	string const      m_assetsPath;
	string            m_localizedAssetsPath;
	char const* const m_szUserSettingsFile;

};
} // namespace Adx2
} // namespace Impl
} // namespace ACE
