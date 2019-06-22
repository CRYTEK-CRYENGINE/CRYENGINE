// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include "AudioTableInfo.h"

class XmlNodeRef;

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

	CProjectLoader() = delete;
	CProjectLoader(CProjectLoader const&) = delete;
	CProjectLoader(CProjectLoader&&) = delete;
	CProjectLoader& operator=(CProjectLoader const&) = delete;
	CProjectLoader& operator=(CProjectLoader&&) = delete;

	explicit CProjectLoader(
		string const& projectPath,
		string const& banksPath,
		string const& localizedBanksPath,
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

	void   LoadBanks(string const& folderPath, bool const isLocalized, CItem& parent);
	void   ParseFolder(string const& folderPath, CItem& editorFolder, CItem& parent);
	void   ParseFile(string const& filepath, CItem& parent, EPakStatus const pakStatus);
	void   ParseKeysFile(string const& filePath, CItem& parent);
	void   RemoveEmptyMixerGroups();
	void   RemoveEmptyEditorFolders(CItem* const pEditorFolder);

	CItem* GetContainer(string const& id, EItemType const type, CItem& parent, EPakStatus const pakStatus);
	void   LoadContainer(XmlNodeRef const& node, EItemType const type, uint32 const relationshipParamId, CItem& parent, EPakStatus const pakStatus);
	void   LoadSnapshotGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadFolder(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadMixerGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);

	CItem* LoadItem(XmlNodeRef const& node, EItemType const type, CItem& parent, EPakStatus const pakStatus);
	CItem* LoadEvent(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadSnapshot(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadReturn(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadParameter(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadVca(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus);
	void   LoadAudioTable(XmlNodeRef const& node);
	void   LoadKeys(CItem& parent);
	void   LoadKeysFromFiles(CItem& parent, string const& filePath);

	using ItemIds = std::map<string, CItem*>;

	CImpl const&        m_impl;
	CItem&              m_rootItem;
	ItemCache&          m_itemCache;
	ItemIds             m_containerIds;
	ItemIds             m_snapshotGroupItems;
	std::vector<CItem*> m_emptyMixerGroups;
	string const        m_projectPath;
	AudioTableInfos     m_audioTableInfos;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
