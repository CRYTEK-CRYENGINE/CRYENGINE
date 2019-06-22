// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"

class XmlNodeRef;

namespace ACE
{
namespace Impl
{
namespace Wwise
{
class CProjectLoader final
{
public:

	CProjectLoader() = delete;
	CProjectLoader(CProjectLoader const&) = delete;
	CProjectLoader(CProjectLoader&&) = delete;
	CProjectLoader& operator=(CProjectLoader const&) = delete;
	CProjectLoader& operator=(CProjectLoader&&) = delete;

	explicit CProjectLoader(string const& projectPath, string const& soundbanksPath, string const& localizedBanksPath, CItem& rootItem, ItemCache& itemCache);

private:

	void   LoadSoundBanks(string const& folderPath, bool const isLocalized, CItem& parent);
	void   LoadFolder(string const& folderPath, string const& folderName, CItem& parent);
	void   LoadWorkUnitFile(string const& filePath, CItem& parent, EPakStatus const pakStatus);
	void   LoadXml(XmlNodeRef const& rootNode, CItem& parent, EPakStatus const pakStatus);
	CItem* CreateItem(string const& name, EItemType const type, CItem& pParent, EPakStatus const pakStatus);

	void   BuildFileCache(string const& folderPath);

private:

	using FilesCache = std::map<uint32, string>;
	using Items = std::map<uint32, CItem*>;

	CItem&       m_rootItem;
	ItemCache&   m_itemCache;
	string const m_projectPath;

	// This maps holds the items with the internal IDs given in the Wwise files.
	Items m_items;

	// Cache with the file path to each work unit file
	FilesCache m_filesCache;

	// List of already loaded work unit files
	std::set<uint32> m_filesLoaded;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
