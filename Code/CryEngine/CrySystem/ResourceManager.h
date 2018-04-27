// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ResourceManager.cpp
//  Version:     v1.00
//  Created:     8/02/2010 by Timur.
//  Description: Interface to the Resource Manager
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_RESOURCE_MANAGER_H_
#define _CRY_RESOURCE_MANAGER_H_

#pragma once

#include <CrySystem/File/IResourceManager.h>
#include "AsyncPakManager.h"

//////////////////////////////////////////////////////////////////////////
// IResource manager interface
//////////////////////////////////////////////////////////////////////////
class CResourceManager : public IResourceManager, public ISystemEventListener, public ICryPakFileAcesssSink
{
public:
	CResourceManager();

	void Init();
	void Shutdown();

	bool IsStreamingCachePak(const char* filename) const;

	//////////////////////////////////////////////////////////////////////////
	// IResourceManager interface implementation.
	//////////////////////////////////////////////////////////////////////////
	void           PrepareLevel(const char* sLevelFolder, const char* sLevelName);
	void           UnloadLevel();
	IResourceList* GetLevelResourceList();
	bool           LoadLevelCachePak(const char* sPakName, const char* sBindRoot, bool bOnlyDuringLevelLoading);
	void           UnloadLevelCachePak(const char* sPakName);
	bool           LoadModeSwitchPak(const char* sPakName, const bool multiplayer);
	void           UnloadModeSwitchPak(const char* sPakName, const char* sResourceListName, const bool multiplayer);
	bool           LoadMenuCommonPak(const char* sPakName);
	void           UnloadMenuCommonPak(const char* sPakName, const char* sResourceListName);
	bool           LoadPakToMemAsync(const char* pPath, bool bLevelLoadOnly);
	void           UnloadAllAsyncPaks();
	bool           LoadLayerPak(const char* sLayerName);
	void           UnloadLayerPak(const char* sLayerName);
	void           UnloadAllLevelCachePaks(bool bLevelLoadEnd);
	void           GetMemoryStatistics(ICrySizer* pSizer);
	bool           LoadFastLoadPaks(bool bToMemory);
	void           UnloadFastLoadPaks();
	CTimeValue     GetLastLevelLoadTime() const;
	void           GetLayerPakStats(SLayerPakStats& stats, bool bCollectAllStats) const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// ISystemEventListener interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// ICryPakFileAcesssSink interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void ReportFileOpen(FILE* in, const char* szFullPath);
	//////////////////////////////////////////////////////////////////////////

	// Per frame update of the resource manager.
	void          Update();

	CryPathString GetCurrentLevelCacheFolder() const { return m_currentLevelCacheFolder; };
	void          SaveRecordedResources(bool bTotalList = false);

private:

	//////////////////////////////////////////////////////////////////////////

	CryPathString m_currentLevelCacheFolder;

	struct SOpenedPak
	{
		CryFixedStringT<ICryPak::g_nMaxPath> filename;
		bool bOnlyDuringLevelLoading;
	};
	std::vector<SOpenedPak>                           m_openedPaks;

	CAsyncPakManager                                  m_AsyncPakManager;

	string                                            m_sLevelFolder;
	string                                            m_sLevelName;
	bool                                              m_bLevelTransitioning;

	bool                                              m_bRegisteredFileOpenSink;
	bool                                              m_bOwnResourceList;

	CTimeValue                                        m_beginLevelLoadTime;
	CTimeValue                                        m_lastLevelLoadTime;

	_smart_ptr<IResourceList>                         m_pSequenceResourceList;

	CryCriticalSection                                recordedFilesLock;
	std::vector<string>                               m_recordedFiles;

	std::vector<CryFixedStringT<ICryPak::g_nMaxPath>> m_fastLoadPakPaths;
};

#endif //_CRY_RESOURCE_MANAGER_H_
