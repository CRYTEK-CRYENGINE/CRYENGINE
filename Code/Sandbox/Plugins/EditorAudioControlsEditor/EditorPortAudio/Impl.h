// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IImpl.h>

#include "Item.h"
#include "Settings.h"
#include "ItemModel.h"

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
class CImpl final : public IImpl
{
public:

	CImpl();
	virtual ~CImpl() override;

	// IImpl
	virtual void           Reload(bool const preserveConnectionStatus = true) override;
	virtual void           SetPlatforms(Platforms const& platforms) override {}
	virtual IItemModel*    GetItemModel() const override                     { return m_pItemModel; }
	virtual IItem*         GetItem(ControlId const id) const override;
	virtual CryIcon const& GetItemIcon(IItem const* const pIItem) const override;
	virtual QString const& GetItemTypeName(IItem const* const pIItem) const override;
	virtual string const&  GetName() const override             { return m_implName; }
	virtual string const&  GetFolderName() const override       { return m_implFolderName; }
	virtual ISettings*     GetSettings() override               { return &m_settings; }
	virtual bool           IsFileImportAllowed() const override { return true; }
	virtual bool           IsSystemTypeSupported(EAssetType const assetType) const override;
	virtual bool           IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const override;
	virtual EAssetType     ImplTypeToAssetType(IItem const* const pIItem) const override;
	virtual ConnectionPtr  CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) override;
	virtual ConnectionPtr  CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType) override;
	virtual XmlNodeRef     CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType) override;
	virtual void           EnableConnection(ConnectionPtr const pConnection) override;
	virtual void           DisableConnection(ConnectionPtr const pConnection) override;
	// ~IImpl

private:

	void      Clear();
	void      CreateItemCache(CItem const* const pParent);
	ControlId GetId(string const& name) const;

	static string const s_itemNameTag;
	static string const s_pathNameTag;
	static string const s_eventConnectionTag;
	static string const s_sampleConnectionTag;
	static string const s_connectionTypeTag;
	static string const s_panningEnabledTag;
	static string const s_attenuationEnabledTag;
	static string const s_attenuationDistMin;
	static string const s_attenuationDistMax;
	static string const s_volumeTag;
	static string const s_loopCountTag;

	using ItemCache = std::map<ControlId, CItem*>;
	using ConnectionIds = std::map<ControlId, int>;

	CItem               m_rootItem { "", s_aceInvalidId, EItemType::None };
	ItemCache           m_itemCache; // cache of the items stored by id for faster access
	ConnectionIds       m_connectionsByID;
	CSettings           m_settings;
	CryAudio::SImplInfo m_implInfo;
	string              m_implName;
	string              m_implFolderName;
	CItemModel* const   m_pItemModel;
};
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
