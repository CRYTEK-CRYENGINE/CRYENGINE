// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** DownloadMgr.h
** 6/5/10
******************************************************************************/

#ifndef __DOWNLOADMGR_H__
#define __DOWNLOADMGR_H__

#include <CryLobby/ICryTCPService.h>
#include <CryString/CryFixedString.h>
#include "AutoEnum.h"
#include "GameMechanismManager/GameMechanismBase.h"
#include "IPatchPakManagerListener.h"

#if _DEBUG
#define DOWNLOAD_MGR_DBG	1
#endif


class CDownloadableResource;
typedef _smart_ptr<CDownloadableResource> CDownloadableResourcePtr;

struct IDataListener
{
	virtual void								DataDownloaded(
																CDownloadableResourcePtr		inResource)=0;
	virtual void								DataFailedToDownload(
																CDownloadableResourcePtr		inResource)=0;

	virtual											~IDataListener();
};

class CDownloadableResource : public CMultiThreadRefCount
{
	friend class CDownloadMgr;

	public:
		typedef uint32							TState;

#define DownloadableResourceStates(f) \
			f(k_notStarted) \
			f(k_failedServerUnreachable) \
			f(k_failedInternalError) \
			f(k_failedReplyHasBadHeader) \
			f(k_failedReplyContentTooLong) \
			f(k_failedReplyContentTruncated) \
			f(k_failedUnknownResource) \
			f(k_failedAborted) \
			f(k_failedReplyTimedOut) \
			f(k_awaitingHTTPResponse) \
			f(k_awaitingPayload) \
			f(k_dataAvailable)

		AUTOENUM_BUILDFLAGS_WITHZERO(DownloadableResourceStates,k_invalidState);

		static const TState					k_callbackInProgressMask				= (k_awaitingHTTPResponse|k_awaitingPayload);
		static const TState					k_dataPermanentFailMask					= (k_failedReplyTimedOut|k_failedServerUnreachable|k_failedInternalError|k_failedReplyHasBadHeader|k_failedReplyContentTooLong|k_failedReplyContentTruncated|k_failedUnknownResource|k_failedAborted);
		static const TState					k_dataSoftFailMask							= (k_failedReplyTimedOut|k_failedServerUnreachable);

	protected:
		typedef std::vector<IDataListener*>		TListenerVector;

		TListenerVector							m_listeners;
		TListenerVector							m_listenersToRemove;

		CryFixedStringT<32>         m_serviceConfigName;
		CryFixedStringT<128>				m_urlPrefix;
		CryFixedStringT<128>				m_url;
		CryFixedStringT<32>					m_descName;
		CryFixedStringT<64>					m_server;
		int													m_port;
		int													m_maxDownloadSize;

		enum EListenerBroadcastedState
		{
			k_notBroadcasted,
			k_broadcastedSuccess,
			k_broadcastedFail
		};
		EListenerBroadcastedState		m_broadcastedState;

		ICryTCPServicePtr						m_pService;
		CDownloadableResourcePtr		m_isLocalisedInstanceOf;

		////////////////
		// all these are modifiable from another thread via receive data callback and should only be modified from main thread
		// if the callback is not in flight
		// pBuffer should not be read whilst the callback is in flight as it may reallocate the buffer
		char												*m_pBuffer;
		int													m_bufferUsed;
		int													m_bufferSize;
		int													m_contentLength;			// size and offset into the buffer of where the content starts
		int													m_contentOffset;
		TState											m_state;
		////////////////

		bool												m_abortDownload;			// set from main thread, read from callback thread
		bool												m_doingHTTPParse;			// set when the downloadable resource is in the middle of a InitHTTPParser() / ReleaseHTTPParser() pair
#if defined(DEDICATED_SERVER)
		int													m_numTimesUpdateCheckFailed;
#endif
		
		void												LoadConfig(
																	XmlNodeRef				inNode);
		bool												Validate();

		void												UpdateRemoveListeners();
		void												BroadcastSuccess();
		void												BroadcastFail();

		bool												StoreData(
																	const	char						*pInData,
																	size_t								inDataLen);
		bool												ReceiveHTTPHeader(
																	bool								inReceivedEndOfStream);

		bool												DecryptAndCheckSigning(
																	const char						*pInData,
																	int										inDataLen,
																	char									**pOutData,
																	int										*pOutDataLen,
																	const char						*pDecryptionKey,
																	int										decryptionKeyLength,
																	const char						*pSigningSalt,
																	int										signingSaltLength);

	public:
		static bool									ReceiveDataCallback(
																	ECryTCPServiceResult	res,
																	void*									pArg,
																	STCPServiceDataPtr		pUpload,
																	const char*						pData,
																	size_t								dataLen,
																	bool									endOfStream );

		void												InitHTTPParser();
		void												ReleaseHTTPParser();
	
	protected:

#if DOWNLOAD_MGR_DBG
		CTimeValue									m_downloadStarted;
		CTimeValue									m_downloadFinished;

		static string								GetStateAsString(
																	TState								inState);
		string											GetStateAsString()			{ return GetStateAsString(m_state); }
#endif

	public:
																CDownloadableResource();
																~CDownloadableResource();
		void												Reset();

		// set the download information
		void												SetDownloadInfo(const char* pUrl, const char* pUrlPrefix, const char* pServer, const int port, const int maxDownloadSize, const char* pDescName=NULL);


		// sets the agent string for any http requests we issue
		void												GetUserAgentString(CryFixedStringT<64> &ioAgentStr);

		// starts the data downloading
		// does nothing if the data is already downloaded / downloading
		void												StartDownloading();

		// returns the current state
		// does not set the resource downloading or change its state in any way
		TState											GetState() const				{ return m_state; }

		// returns the downloadable data for this resource in raw form
		// starts the resource downloading if it is not available currently
		TState											GetRawData(
																	char									**pOutData,
																	int										*pOutLen);

		// returns the downloadable data for this resource in decrypted form
		// starts the resource downloading if it is not available currently
		TState											GetDecryptedData(
																	char									*pBuffer,
																	int										*pLength,
																	const char						*pDecryptionKey,
																	int										decryptionKeyLength,
																	const char						*pSigningSalt,
																	int										signingSaltLength);

		// returns the download progress of this resource
		TState											GetProgress(
																	int										*outBytesDownloaded,
																	int										*outTotalBytesToDownload);

		// returns whether the download had broadcasted results yet
		bool												HasBroadcastedResults() { return (m_broadcastedState!=k_notBroadcasted); }

		// returns download description
		const char*									GetDescription()	{ return m_descName.c_str(); }

		// returns download URL
		const char*									GetURL()					{ return m_url.c_str(); }

		// returns download URL Prefix
		const char*									GetURLPrefix()		{ return m_pService ? m_pService->GetURLPrefix() : ""; }

		// returns download Server
		const char*									GetServer()				{ return m_pService ? m_pService->GetServerName() : ""; }

		// returns download Port
		int													GetPort()					{ return m_pService ? m_pService->GetPort() : 0; }

		// returns download Max Size
		int													GetMaxSize()			{ return m_maxDownloadSize; }

#if DOWNLOAD_MGR_DBG
		// returns the transfer rate in bytes/sec
		float												GetTransferRate();

		// Log the contents to a string
		void												DebugContentsToString(CryFixedStringT<512> &outStr);

		// CryWatch the contents
		void												DebugWatchContents();
#endif

		// adds a listener to events for this resource
		// if the data is already downloaded or has already permanently failed the listener will be called immediately
		// also starts resource downloading if not already downloaded
		// all callbacks are issued on the main thread
		void												AddDataListener(
																	IDataListener					*pInListener);
		void												RemoveDataListener(
																	IDataListener					*pInListener);

		// removes downloaded data. fails if currently mid download. returns whether was successful
		bool												Purge();

		// cancels the download if it's in progress
		// will broadcast the failure to any listeners immediately
		// only call this from the main thread
		// if the resource is not downloading, it will do nothing
		void												CancelDownload();

		// if using a bespoke CDownloadableResource that is not managed by the download mgr, you must call this
		// for the listeners to receive the callbacks
		// if the resource is obtained from CDownloadMgr::FindResourceByName() you do not need to call this
		// only call this from the main thread
		void												DispatchCallbacks();
};

class CDownloadMgr : public CGameMechanismBase, public IPatchPakManagerListener
{
	protected:
		typedef std::vector<CDownloadableResourcePtr>	TResourceVector;

		TResourceVector							m_resources;
#if defined(DEDICATED_SERVER)
		TResourceVector							m_refreshResources;
		CTimeValue									m_lastUpdateTime;
		int													m_roundsUntilQuit;
		float												m_timeSinceLastShutdownMessage;
		bool												m_isShutingDown;
#endif

		static bool									ReceiveDataCallback(
																	ECryTCPServiceResult	res,
																	void*									pArg,
																	const char*						pData,
																	size_t								dataLen,
																	bool									endOfStream );
#if DOWNLOAD_MGR_DBG
		static void									DbgList(
																	IConsoleCmdArgs				*pInArgs);
		static void									DbgCat(
																	IConsoleCmdArgs				*pInArgs);
		static void									DbgPurge(
																	IConsoleCmdArgs				*pInArgs);
#endif

		void												DispatchCallbacks();
		void												UpdateRemoveListeners();
		virtual void								Update(
																	float									inDt);
#if defined(DEDICATED_SERVER)
		void												OnResourceChanged(
																	CDownloadableResourcePtr	pOldResouce,
																	CDownloadableResourcePtr	pNewRes);
		void												InitShutdown();

		// something has happened where by this instance of a server is out of date and should quit
		void												WeNeedToQuit();
#endif
	public:
																CDownloadMgr();
																~CDownloadMgr();
		void												Reset();

		void												Init(
		                              const char            *pTCPServicesDescriptionFile,
																	const char						*pInResourceDescriptionsFile);

		// IPatchPakManagerListener
		virtual void UpdatedPermissionsNowAvailable();
		// ~IPatchPakManagerListener


		// returns the passed resource if it exists, NULL if none
		CDownloadableResourcePtr		FindResourceByName(
																	const char						*pInResourceName);
		CDownloadableResourcePtr		FindLocalizedResourceByName(
																	const char						*pInResourceName);

		// will purge all localized instances of the named localizable resource - freeing their memory
		void												PurgeLocalizedResourceByName(
																	const char						*inResourceName);

		// wait for downloads of specified resources to finish, warning this will stall the main thread
		// until either the downloads finish or the timeout is reached
		void												WaitForDownloadsToFinish(const char** resources, int numResources, float timeout);

#if defined(DEDICATED_SERVER)
		// called when a dedicated server returns to the lobby
		void												OnReturnToLobby();

		// check for updates to the specified resource
		void												CheckUpdates(const char *pInResourceName);

		// is waiting for the game to shudown in order to apply data patch
		bool												IsWaitingToShutdown();
#endif

#if DOWNLOAD_MGR_DBG
		bool												IsListenerListening(
																	IDataListener					*inListener);
#endif
};

#endif // __DOWNLOADMGR_H__

