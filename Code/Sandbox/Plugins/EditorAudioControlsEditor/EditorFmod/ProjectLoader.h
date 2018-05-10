// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"

#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CImpl;

class CProjectLoader final
{
public:

	explicit CProjectLoader(string const& projectPath, string const& soundbanksPath, CItem& rootItem, ItemCache& itemCache, CImpl const& impl);

	CProjectLoader() = delete;

private:

	CItem* CreateItem(string const& name, EItemType const type, CItem* const pParent, string const& filePath = "");

	void   LoadBanks(string const& folderPath, bool const isLocalized, CItem& parent);
	void   ParseFolder(string const& folderPath, CItem& editorFolder, CItem& parent);
	void   ParseFile(string const& filepath, CItem& parent);
	void   RemoveEmptyMixerGroups();
	void   RemoveEmptyEditorFolders(CItem* const pEditorFolder);

	CItem* GetContainer(string const& id, EItemType const type, CItem& parent);
	CItem* LoadContainer(XmlNodeRef const pNode, EItemType const type, string const& relationshipParamName, CItem& parent);
	CItem* LoadSnapshotGroup(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadFolder(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadMixerGroup(XmlNodeRef const pNode, CItem& parent);

	CItem* LoadItem(XmlNodeRef const pNode, EItemType const type, CItem& parent);
	CItem* LoadEvent(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadSnapshot(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadReturn(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadParameter(XmlNodeRef const pNode, CItem& parent);
	CItem* LoadVca(XmlNodeRef const pNode, CItem& parent);

	using ItemIds = std::map<string, CItem*>;

	CImpl const&        m_impl;
	CItem&              m_rootItem;
	ItemCache&          m_itemCache;
	ItemIds             m_containerIds;
	ItemIds             m_snapshotGroupItems;
	std::vector<CItem*> m_emptyMixerGroups;
	string const        m_projectPath;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE

