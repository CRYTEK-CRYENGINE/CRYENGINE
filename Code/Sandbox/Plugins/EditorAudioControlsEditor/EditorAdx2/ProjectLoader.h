// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"

class XmlNodeRef;

namespace ACE
{
namespace Impl
{
namespace Adx2
{
class CImpl;

class CProjectLoader final
{
public:

	CProjectLoader() = delete;
	CProjectLoader(CProjectLoader const&) = delete;
	CProjectLoader(CProjectLoader&&) = delete;
	CProjectLoader& operator=(CProjectLoader const&) = delete;
	CProjectLoader& operator=(CProjectLoader&&) = delete;

	explicit CProjectLoader(
		string const& projectPath,
		string const& assetsPath,
		string const& localizedAssetsPath,
		CItem& rootItem,
		ItemCache& itemCache,
		CImpl const& impl);

private:

	CItem* CreateItem(
		string const& name,
		EItemType const type,
		CItem* const pParent,
		EItemFlags const flags,
		EPakStatus const pakStatus = EPakStatus::None,
		string const& filePath = "");

	void LoadGlobalSettings(string const& folderPath, CItem& parent);
	void ParseGlobalSettingsFile(XmlNodeRef const& node, CItem& parent, EItemType const type);
	void ParseBusSettings(XmlNodeRef const& node, CItem& parent);
	void ParseBusesAndSnapshots(XmlNodeRef const& node);

	void LoadWorkUnits(string const& folderPath, CItem& parent);
	void LoadWorkUnitFile(string const& folderPath, CItem& parent);
	void ParseWorkUnitFile(XmlNodeRef const& node, CItem& parent);

	void LoadBinaries(string const& folderPath, bool const isLocalized, CItem& parent);
	void RemoveEmptyFolder(CItem* const pEditorFolder);

	CImpl const& m_impl;
	CItem&       m_rootItem;
	ItemCache&   m_itemCache;
	CItem*       m_pBusesFolder;
	CItem*       m_pSnapShotsFolder;
};
} // namespace Adx2
} // namespace Impl
} // namespace ACE
