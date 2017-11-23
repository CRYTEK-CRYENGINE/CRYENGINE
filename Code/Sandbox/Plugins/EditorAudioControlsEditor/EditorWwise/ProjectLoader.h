// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ImplControls.h"

#include <CrySystem/XML/IXml.h>
#include <SystemTypes.h>

namespace ACE
{
namespace Wwise
{
class CProjectLoader final
{
public:

	CProjectLoader(string const& projectPath, string const& soundbanksPath, CImplItem& root);

private:

	void       LoadSoundBanks(string const& folderPath, bool const isLocalized, CImplItem& parent);
	void       LoadFolder(string const& folderPath, string const& folderName, CImplItem& parent);
	void       LoadWorkUnitFile(string const& filePath, CImplItem& parent);
	void       LoadXml(XmlNodeRef const root, CImplItem& parent);
	CImplItem* CreateItem(string const& name, EImpltemType const type, CImplItem& pParent);
	void       LoadEventsMetadata(string const& soundbanksPath);

	CImplItem* GetControlByName(string const& name, bool const isLocalised = false, CImplItem const* const pParent = nullptr) const;

	void       BuildFileCache(string const& folderPath);

private:

	struct SEventInfo
	{
		float maxRadius;
	};

	typedef std::map<uint32, SEventInfo> EventsInfoMap;
	typedef std::map<CID, CImplItem*>    ControlsCache;
	typedef std::map<uint32, string>     FilesCache;
	typedef std::map<uint32, CImplItem*> Items;

	EventsInfoMap m_eventsInfoMap;
	CImplItem&    m_root;
	ControlsCache m_controlsCache;
	string        m_projectPath;

	// This maps holds the items with the internal IDs given in the Wwise files.
	Items m_items;

	// Cache with the file path to each work unit file
	FilesCache m_filesCache;

	// List of already loaded work unit files
	std::set<uint32> m_filesLoaded;
};
} // namespace Wwise
} // namespace ACE
