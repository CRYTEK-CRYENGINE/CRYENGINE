// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IImpl.h>

#include "Item.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
static Platforms s_platforms;

class CDataPanel;

class CImpl final : public IImpl
{
public:

	CImpl();
	virtual ~CImpl() override;

	// IImpl
	virtual QWidget*       CreateDataPanel() override;
	virtual void           DestroyDataPanel() override;
	virtual void           Reload(bool const preserveConnectionStatus = true) override;
	virtual void           SetPlatforms(Platforms const& platforms) override { s_platforms = platforms; }
	virtual IItem*         GetItem(ControlId const id) const override;
	virtual CryIcon const& GetItemIcon(IItem const* const pIItem) const override;
	virtual QString const& GetItemTypeName(IItem const* const pIItem) const override;
	virtual string const&  GetName() const override                                         { return m_implName; }
	virtual string const&  GetFolderName() const override                                   { return m_implFolderName; }
	virtual char const*    GetAssetsPath() const override                                   { return m_assetsPath.c_str(); }
	virtual char const*    GetProjectPath() const override                                  { return m_projectPath.c_str(); }
	virtual void           SetProjectPath(char const* const szPath) override;
	virtual bool           SupportsProjects() const override                                { return true; }
	virtual bool           IsSystemTypeSupported(EAssetType const assetType) const override { return true; }
	virtual bool           IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const override;
	virtual EAssetType     ImplTypeToAssetType(IItem const* const pIItem) const override;
	virtual ConnectionPtr  CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) override;
	virtual ConnectionPtr  CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType) override;
	virtual XmlNodeRef     CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType) override;
	virtual void           EnableConnection(ConnectionPtr const pConnection, bool const isLoading) override;
	virtual void           DisableConnection(ConnectionPtr const pConnection, bool const isLoading) override;
	virtual void           OnAboutToReload() override;
	virtual void           OnReloaded() override;
	virtual void           OnSelectConnectedItem(ControlId const id) const override;
	virtual void           OnFileImporterOpened() override {}
	virtual void           OnFileImporterClosed() override {}
	// ~IImpl

	CItem const& GetRootItem() const { return m_rootItem; }

	void         Serialize(Serialization::IArchive& ar);

private:

	void Clear();

	// Generates the ID of the item given its full path name.
	ControlId GenerateID(string const& name, bool isLocalized, CItem* pParent) const;
	// Convenience function to form the full path name.
	// Items can have the same name if they're under different parents so knowledge of the parent name is needed.
	// Localized items live in different areas of disk so we also need to know if its localized.
	ControlId GenerateID(string const& fullPathName) const;

	using ConnectionIds = std::map<ControlId, int>;

	CItem               m_rootItem { "", s_aceInvalidId, EItemType::None };
	ItemCache           m_itemCache; // cache of the items stored by id for faster access
	ConnectionIds       m_connectionsByID;
	CryAudio::SImplInfo m_implInfo;
	string              m_implName;
	string              m_implFolderName;
	string              m_projectPath;
	string const        m_assetsPath;
	char const* const   m_szUserSettingsFile;
	CDataPanel*         m_pDataPanel;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
