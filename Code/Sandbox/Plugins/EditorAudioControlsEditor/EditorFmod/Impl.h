// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IImpl.h>

#include "Item.h"
#include "Settings.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
static Platforms s_platforms;

class CImpl final : public IImpl
{
public:

	CImpl();
	virtual ~CImpl() override;

	// IImpl
	virtual void          Reload(bool const preserveConnectionStatus = true) override;
	virtual void          SetPlatforms(Platforms const& platforms) override { s_platforms = platforms; }
	virtual IItem*        GetRoot() override                                { return &m_rootItem; }
	virtual IItem*        GetItem(ControlId const id) const override;
	virtual char const*   GetTypeIcon(IItem const* const pIItem) const override;
	virtual string const& GetName() const override;
	virtual string const& GetFolderName() const override;
	virtual ISettings*    GetSettings() override                                           { return &m_settings; }
	virtual bool          IsSystemTypeSupported(EAssetType const assetType) const override { return true; }
	virtual bool          IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const override;
	virtual EAssetType    ImplTypeToAssetType(IItem const* const pIItem) const override;
	virtual ConnectionPtr CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) override;
	virtual ConnectionPtr CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType) override;
	virtual XmlNodeRef    CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType) override;
	virtual void          EnableConnection(ConnectionPtr const pConnection) override;
	virtual void          DisableConnection(ConnectionPtr const pConnection) override;
	// ~IImpl

private:

	void   Clear();
	CItem* CreateItem(string const& name, EItemType const type, CItem* const pParent);
	CItem* GetItemFromPath(string const& fullpath);
	CItem* CreatePlaceholderFolderPath(string const& path);

	using ConnectionIds = std::map<ControlId, int>;

	CItem               m_rootItem { "", s_aceInvalidId, EItemType::None };
	ItemCache           m_itemCache; // cache of the items stored by id for faster access
	ConnectionIds       m_connectionsByID;
	CSettings           m_settings;
	CryAudio::SImplInfo m_implInfo;
	string              m_implName;
	string              m_implFolderName;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
