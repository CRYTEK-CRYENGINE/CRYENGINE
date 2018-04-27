// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Handles the downloading and managing of various patch paks

	-------------------------------------------------------------------------
	History:
	- 31:01:2012  : Created by James Bamford

*************************************************************************/

#ifndef __PATCHPAKMANAGER_H__
#define __PATCHPAKMANAGER_H__


#if _MSC_VER > 1000
# pragma once
#endif

#include "DownloadMgr.h"
#include <CryCore/Containers/CryFixedArray.h>
#include <CryCore/Platform/IPlatformOS.h>
#include "IPatchPakManagerListener.h"

#if defined(_RELEASE) 
	#define DEBUG_LATE_DOWNLOADING_OF_PATCH_PAKS 0
#else
	#define DEBUG_LATE_DOWNLOADING_OF_PATCH_PAKS 0
#endif

class CPatchPakMemoryBlock : public ICustomMemoryBlock
{
public:
	CPatchPakMemoryBlock( void *pMem, int size )
	{
		m_pData=pMem;
		m_nSize=size;
	}

	virtual ~CPatchPakMemoryBlock()
	{
		//do nothing, we don't own the mem
	}

	virtual void * GetData() { return m_pData; }
	virtual int GetSize() { return m_nSize; }

	virtual void CopyMemoryRegion( void *pOutputBuffer,size_t nOffset,size_t nSize );

private:
	void* m_pData;
	size_t m_nSize;
};

class CPatchPakManager : public IDataListener, public IPlatformOS::IPlatformListener
{
public:
	CPatchPakManager();
	~CPatchPakManager();

	void Update(float frameTime);
	void BlockingUpdateTillDone();

	// IPlatformOS::IPlatformListener
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event);
	// ~IPlatformOS::IPlatformListener

	// IDataListener
	virtual void DataDownloaded(CDownloadableResourcePtr inResource);
	virtual void DataFailedToDownload(CDownloadableResourcePtr inResource);
	// ~IDataListener

	void RequestPermissionsXMLDownload();
	void UnloadPatchPakFiles();
	uint32 GetXOROfPatchPakCRCs();
	int GetPatchPakVersion() { return m_patchPakVersion; }

	bool HasPatchingSucceeded() const;
	bool AreUpdatedPermissionsAvailable() { return m_updatedPermissionsAvailable; }
	int  TimeThatUpdatedPermissionsBecameAvailable() { return m_timeThatUpdatedPermissionsAvailable; }
	bool IsOkToCachePaks()	{ return m_okToCachePaks; } // TODO - use me in the frontend to popup the storage device or warning about having to have storage

	void RegisterPatchPakManagerEventListener(IPatchPakManagerListener *pListener);
	void UnregisterPatchPakManagerEventListener(IPatchPakManagerListener *pListener);
	void EventUpdatedPermissionsAvailable();

	void VersionMismatchErrorOccurred();

protected:

	typedef std::vector<IPatchPakManagerListener*> TPatchPakManagerEventListenersVec;
	TPatchPakManagerEventListenersVec m_eventListeners;

	static const int k_maxHttpHeaderSize=1024;
	static const int k_maxPatchPaks=10;
	
	enum EMgrState
	{
		eMS_waiting_to_initial_get_permissions=0,
		eMS_initial_get_permissions_requested,
		eMS_initial_permissions_downloaded,
		eMS_patches_failed_to_download_in_time,
		eMS_patches_failed_to_download_in_time_but_now_all_cached_or_downloaded,
		eMS_patches_installed,
		eMS_patches_installed_permissions_requested,
		eMS_patches_installed_permissions_downloaded,
	};

	enum EVersionMismatchFault
	{
		eVMF_unknown=0,
		eVMF_our_fault,
		eVMF_their_fault
	};

	typedef _smart_ptr<CPatchPakMemoryBlock> CPatchPakMemoryBlockPtr;
	struct SPatchPakData
	{
		enum EState
		{
			es_Uninitialised=0,
			eS_Downloading,
			es_Downloaded,
			es_FailedToDownload,
			es_DownloadedButCorrupt,
			es_Cached,
			es_FailedToOpenFromCache,
			es_PakLoaded,
			es_PakLoadedFromCache,
		};

		float m_showingSaveMessageTimer;
		CDownloadableResourcePtr m_downloadableResource;
		CPatchPakMemoryBlockPtr m_pPatchPakMemBlock;
		uint32 m_actualCRC32;
		CryFixedStringT<32> m_pMD5Str; 
		unsigned char				m_pMD5[16];
		uint32							m_MD5CRC32; // for setting a suitably different matchmaking version
		unsigned int				m_downloadSize;
		CryFixedStringT<32> m_url; 
		CryFixedStringT<128> m_pakBindRoot;
		EState m_state;
		bool m_MD5FileName;
		bool m_showingSaveMessage;

		// type
		// cache to disk



		SPatchPakData();
	};

	typedef CryFixedArray<SPatchPakData, k_maxPatchPaks> TPatchPaksArray;

	void ProcessPatchPaksFromPermissionsXML(XmlNodeRef root);
	int GetPatchPakDataIndexFromDownloadableResource(CDownloadableResourcePtr inResource);
	SPatchPakData *GetPatchPakDataFromDownloadableResource(CDownloadableResourcePtr inResource);

	void StartNewDownload(
		const char *inServerName, 
		const int inPort, 
		const char *inURLPrefix, 
		const char *inURL, 
		const char *inPakBindRoot,
		const int inDownloadSize,
		const char *inMD5, 
		const bool inMD5FileName,
		const char *inDescName);
	bool CheckForNewDownload(
		const char *inServerName, 
		const int inPort, 
		const char *inURLPrefix, 
		const char *inURL, 
		const int inDownloadSize, 
		const char *inMD5, 
		const char *inDescName);
	
	void GeneratePakFileNameFromURLName(CryFixedStringT<64> &outPakFileName, const char *inUrlName);
	void ProcessPermissionsXML(CDownloadableResourcePtr inResource);
	bool CachePakDataToDisk(SPatchPakData *pInPakData);
	void PatchPakDataDownloaded(CDownloadableResourcePtr inResource);
	void OpenPatchPakDataAsPak(SPatchPakData *inPakData);
	void SetState(EMgrState inState);
	void ShowErrorForVersionMismatch(EVersionMismatchFault inWhosFault);

	TPatchPaksArray m_patchPaks;
	CDownloadableResourcePtr m_pPermissionsDownloadableResource;
	int							m_numPatchPaksDownloading;
	int							m_numPatchPaksFailedToDownload;
	int							m_numPatchPaksFailedToCache;
	int							m_patchPakVersion;
	uint32					m_installedPermissionsCRC32;
	ICVar						*m_patchPakEnabled;
	ICVar						*m_patchPakDownloadTimeOut;
	ICVar						*m_patchPakPollTime;
	ICVar						*m_patchPakDebug;
	ICVar						*m_patchPakDediServerMustPatch;
	EMgrState				m_state;
	float						m_pollPermissionsXMLTimer;
	int							m_timeThatUpdatedPermissionsAvailable;
	int							m_userThatIsUsingCachePaks;
	float						m_versionMismatchTimer;
	
	bool						m_enabled;
	bool						m_patchPakRemoved;
	bool						m_blockingUpdateInProgress;
	bool						m_haveParsedPermissionsXML;
	bool						m_failedToDownloadPermissionsXML;
	bool						m_okToCachePaks;					// consoles have requirements to be allowed to patch
	bool						m_lastOkToCachePaks;			// consoles have requirements to be allowed to patch
	bool						m_updatedPermissionsAvailable;
	bool						m_wasInFrontend;
	bool						m_isUsingCachePaks;
	bool						m_versionMismatchOccurred;
};


#endif // __PATCHPAKMANAGER_H__
