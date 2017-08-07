// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/XML/IXml.h>
#include <ACETypes.h>
#include <IAudioSystemEditor.h>
#include "AudioSystemControl_wwise.h"

namespace ACE
{

class CAudioWwiseLoader
{
public:
	CAudioWwiseLoader(const string& projectPath, const string& soundbanksPath, IAudioSystemItem& root);

private:
	void              LoadSoundBanks(string const& folderPath, bool const bLocalized, IAudioSystemItem& parent);
	void              LoadFolder(string const& folderPath, string const& folderName, IAudioSystemItem& parent);
	void              LoadWorkUnitFile(const string& filePath, IAudioSystemItem& parent);
	void              LoadXml(XmlNodeRef root, IAudioSystemItem& parent);
	IAudioSystemItem* CreateItem(const string& name, EWwiseItemTypes type, IAudioSystemItem& pParent);
	void              LoadEventsMetadata(const string& soundbanksPath);

	IAudioSystemItem* GetControlByName(const string& name, bool bIsLocalised = false, IAudioSystemItem* pParent = nullptr) const;

	void              BuildFileCache(const string& folderPath);

private:

	struct SEventInfo
	{
		float maxRadius;
	};

	typedef std::map<uint32, SEventInfo>        EventsInfoMap;
	typedef std::map<CID, IAudioSystemItem*>    ControlsCache;
	typedef std::map<uint32, string>            FilesCache;
	typedef std::map<uint32, IAudioSystemItem*> Items;

	EventsInfoMap     m_eventsInfoMap;
	IAudioSystemItem& m_root;
	ControlsCache     m_controlsCache;
	string            m_projectRoot;

	// This maps holds the items with the internal IDs given in the Wwise files.
	Items m_items;

	// Cache with the file path to each work unit file
	FilesCache m_filesCache;

	// List of already loaded work unit files
	std::set<uint32> m_filesLoaded;
};
}
