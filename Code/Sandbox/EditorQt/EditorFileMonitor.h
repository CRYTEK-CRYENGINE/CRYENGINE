// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySystem/File/IFileChangeMonitor.h>
#include "Util/FileChangeMonitor.h"

//DO NOT USE, DEPRECATED !!
//This is currently only used by manneqin and hardcoded behavior in cpp file...
class CEditorFileMonitor
	: public IFileChangeMonitor
	  , public CFileChangeMonitorListener
	  , public IEditorNotifyListener
{
public:
	CEditorFileMonitor();
	~CEditorFileMonitor();

	//DO NOT USE, DEPRECATED !!
	bool RegisterListener(IFileChangeListener* pListener, const char* filename) override;
	bool RegisterListener(IFileChangeListener* pListener, const char* folderRelativeToGame, const char* ext) override;
	bool UnregisterListener(IFileChangeListener* pListener) override;

	// from CFileChangeMonitorListener
	void OnFileMonitorChange(const SFileChangeInfo& rChange) override;

	// from IEditorNotifyListener
	void OnEditorNotifyEvent(EEditorNotifyEvent ev) override;
private:

	void MonitorDirectories();
	// File Change Monitor stuff
	struct SFileChangeCallback
	{
		IFileChangeListener* pListener;
		string              item;
		string              extension;

		SFileChangeCallback()
			: pListener(NULL)
		{}

		SFileChangeCallback(IFileChangeListener* pListener, const char* item, const char* extension)
			: pListener(pListener)
			, item(item)
			, extension(extension)
		{}
	};

	std::vector<SFileChangeCallback> m_vecFileChangeCallbacks;
};

