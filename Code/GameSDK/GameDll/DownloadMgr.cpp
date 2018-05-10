// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** DownloadMgr.cpp
** Mark Tully
** 6/5/10
******************************************************************************/

/*
	allows game data files to be downloaded from a http server via CryNetwork's
	CCryTCPService module

	not to be confused with CDownloadManager in CrySystem, which is windows only
	and doesn't go through the crynetwork abstraction API

	CDownloadManager should probably be replaced with this one
*/

#include "StdAfx.h"
#include "DownloadMgr.h"
#include <CryString/StringUtils.h>
#include "Utility/StringUtils.h"
#include "Utility/CryWatch.h"
#include <CryCore/TypeInfo_impl.h>
#include "GameCVars.h"
#include "GameRules.h"
#include "Network/Lobby/GameLobby.h"
#include <CrySystem/ZLib/IZLibCompressor.h>
#include "PatchPakManager.h"

enum								{ k_localizedResourceSlop=5 };								// add a bit of slop to allow localised resources to be added without contributing to fragmentation by causing the vector to realloc. arbitrary number, not critical this is accurate
enum								{ k_httpHeaderSize=512 };											// should be large enough to hold the HTTP response and info on the payload length
static const float	k_downloadableResourceHTTPTimeout=8.0f;		// time server has to not respond for before the download is marked as a fail

#if DOWNLOAD_MGR_DBG
static const char		*k_dlm_list="dlm_list";
static const char		*k_dlm_cat="dlm_cat";
static const char		*k_dlm_purge="dlm_purge";

static AUTOENUM_BUILDNAMEARRAY(k_downloadableResourceStateNames,DownloadableResourceStates);

#endif

static const char *k_platformPrefix="pc";

CDownloadableResource::CDownloadableResource() :
	m_port(0),
	m_maxDownloadSize(0),
	m_broadcastedState(k_notBroadcasted),
	m_pService(NULL),
	m_pBuffer(NULL),
	m_bufferUsed(0),
	m_bufferSize(0),
	m_contentLength(0),
	m_contentOffset(0),
	m_state(k_notStarted),
	m_abortDownload(false),
	m_doingHTTPParse(false)
#if defined(DEDICATED_SERVER)
	, m_numTimesUpdateCheckFailed(0)
#endif
{
	m_listeners.reserve(8);
	m_listenersToRemove.reserve(8);
}

CDownloadableResource::~CDownloadableResource()
{
	if (m_doingHTTPParse)
	{
		ReleaseHTTPParser();
	}
	CRY_ASSERT_MESSAGE((m_state&k_callbackInProgressMask)==0,"Deleting a resource which is being downloaded - CRASH LIKELY!");	// shouldn't happen due to ref counting
	SAFE_DELETE_ARRAY(m_pBuffer);
}

void CDownloadableResource::Reset()
{
	UpdateRemoveListeners();
	stl::free_container(m_listenersToRemove);
}

CDownloadMgr::CDownloadMgr() :
	REGISTER_GAME_MECHANISM(CDownloadMgr)
#if defined(DEDICATED_SERVER)
	, m_roundsUntilQuit(0)
	, m_timeSinceLastShutdownMessage(0)
	, m_isShutingDown(false)
#endif
{
#if DOWNLOAD_MGR_DBG
	REGISTER_COMMAND(k_dlm_list, (ConsoleCommandFunc)DbgList, 0, "Lists all the resources the download mgr knows about and their state");
	REGISTER_COMMAND(k_dlm_cat, (ConsoleCommandFunc)DbgCat, 0, "Outputs the specified resource as text to the console");
	REGISTER_COMMAND(k_dlm_purge, (ConsoleCommandFunc)DbgPurge, 0, "Removes any downloaded data for the specified resource");
#endif
#if defined(DEDICATED_SERVER)
	m_lastUpdateTime = gEnv->pTimer->GetAsyncTime();
#endif
}

CDownloadMgr::~CDownloadMgr()
{
#if DOWNLOAD_MGR_DBG
	IConsole		*pCon=gEnv->pConsole;
	if (pCon)
	{
		pCon->RemoveCommand(k_dlm_list);
		pCon->RemoveCommand(k_dlm_cat);
		pCon->RemoveCommand(k_dlm_purge);
	}
#endif

#if defined(DEDICATED_SERVER)
	CPatchPakManager *pPatchPakManager = g_pGame->GetPatchPakManager();
	if (pPatchPakManager)
	{
		pPatchPakManager->UnregisterPatchPakManagerEventListener(this);
	}
#endif
}

void CDownloadMgr::Reset()
{
	for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
		(*iter)->Reset();
}

void CDownloadMgr::DispatchCallbacks()
{
	for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
	{
		CDownloadableResourcePtr	pPtr=*iter;

		pPtr->DispatchCallbacks();
	}
}

void CDownloadableResource::BroadcastFail()
{
	m_broadcastedState=CDownloadableResource::k_broadcastedFail;

#if DOWNLOAD_MGR_DBG
	m_downloadFinished=gEnv->pTimer->GetAsyncCurTime();
#endif

	const int numElements = m_listeners.size();

	INDENT_LOG_DURING_SCOPE (true, "Informing %d listeners that '%s' data has failed to download", numElements, this->GetDescription());

	for(int i=0; i < numElements; i++)
	{
		INDENT_LOG_DURING_SCOPE (true, "Listener #%d (%p)...", i, m_listeners[i]);
		m_listeners[i]->DataFailedToDownload(this);
	}
}

void CDownloadableResource::BroadcastSuccess()
{
	m_broadcastedState=CDownloadableResource::k_broadcastedSuccess;

#if DOWNLOAD_MGR_DBG
	m_downloadFinished=gEnv->pTimer->GetAsyncCurTime();
#endif

	const int numElements = m_listeners.size();

	INDENT_LOG_DURING_SCOPE (true, "Informing %d listener(s) that '%s' data has been downloaded successfully (%d bytes)", numElements, this->GetDescription(), m_contentLength);

	for(int i=0; i < numElements; i++)
	{
		INDENT_LOG_DURING_SCOPE (true, "Telling listener #%d (%p)...", i, m_listeners[i]);
		m_listeners[i]->DataDownloaded(this);
	}
}

void CDownloadableResource::DispatchCallbacks()
{
	UpdateRemoveListeners();

	if (m_broadcastedState==CDownloadableResource::k_notBroadcasted)
	{
		if (m_state==CDownloadableResource::k_dataAvailable)
		{
			BroadcastSuccess();
		}
		else if (m_state&CDownloadableResource::k_dataPermanentFailMask)
		{
			BroadcastFail();
		}
	}
}

void CDownloadMgr::Update(
	float					inDt)
{
	// perform callback broadcasts from the main thread
	DispatchCallbacks();

#if defined(DEDICATED_SERVER)
	if (gEnv->IsDedicated())
	{
		if (m_roundsUntilQuit > 0)
		{
			m_timeSinceLastShutdownMessage += inDt;
			if (m_timeSinceLastShutdownMessage > g_pGameCVars->g_shutdownMessageRepeatTime)
			{
				m_timeSinceLastShutdownMessage = 0.0f;
				stack_string msg = g_pGameCVars->g_shutdownMessage->GetString();
				stack_string numrounds;
				numrounds.Format("%d", m_roundsUntilQuit);
				msg.replace("\\1", numrounds.c_str());

				CGameRules* pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
				{
					pGameRules->SendTextMessage(eTextMessageServer, msg, eRMI_ToAllClients);
				}
				/*CGameLobby* pLobby = g_pGame->GetGameLobby();
				if(pLobby)
				{
					pLobby->SendChatMessage(false, msg.c_str());
				}*/
				CGameLobby* pLobby = g_pGame->GetGameLobby();
				if(pLobby)
				{
					int numPlayers = pLobby->GetNumberOfExpectedClients();
					if (numPlayers == 0)
					{
						// If there are no players left then just shutdown straight away
						InitShutdown();
					}
				}
			}
		}
		else if (m_isShutingDown)
		{
			bool shutdown = true;
			CGameLobby* pLobby = g_pGame->GetGameLobby();
			gEnv->pSystem->Quit();
		}
		else
		{
			CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
			float deltaSeconds = currentTime.GetDifferenceInSeconds(m_lastUpdateTime);
			if (deltaSeconds > g_pGameCVars->g_dataRefreshFrequency * 3600)
			{
				CheckUpdates("datapatch");
				//CheckUpdates("playlists");
				//CheckUpdates("variants");
				m_lastUpdateTime = currentTime;
			}
		}

		for (TResourceVector::iterator iter=m_refreshResources.begin(); iter!=m_refreshResources.end(); )
		{
			CDownloadableResourcePtr pNewRes=*iter;
			CDownloadableResource::TState state = pNewRes->GetState();
			if ((state & CDownloadableResource::k_callbackInProgressMask) == 0)
			{
				CDownloadableResourcePtr pRes = FindResourceByName(pNewRes->GetDescription());
				if (pRes)
				{
					if (state == CDownloadableResource::k_dataAvailable)
					{
						char *pData;
						char *pNewData;
						int dataLength;
						int newDataLength;
						pRes->GetRawData(&pData, &dataLength);
						pNewRes->GetRawData(&pNewData, &newDataLength);
						if (dataLength != newDataLength || pData == NULL || memcmp(pData, pNewData, dataLength) != 0)
						{
							// New downloaded content doesn't match previous content
							OnResourceChanged(pRes, pNewRes);
						}
						else
						{
							// New content downloaded successfully and matches previous content
							pRes->m_numTimesUpdateCheckFailed = 0;
						}
					}
					else
					{
						assert(state & CDownloadableResource::k_dataPermanentFailMask);
						if (pRes->GetState() == CDownloadableResource::k_dataAvailable)
						{
							// Unable to download new content, but previous content exists
							if (state & CDownloadableResource::k_dataSoftFailMask)
							{
								// Unable to contact the server because of timeout or DNS fail, this might recover by itself so try a few more times
								pRes->m_numTimesUpdateCheckFailed++;
								if (pRes->m_numTimesUpdateCheckFailed > g_pGameCVars->g_allowedDataPatchFailCount)
								{
									OnResourceChanged(pRes, pNewRes);
								}
							}
							else
							{
								// Server was contacted but either the file was missing or other HTTP error, unlikely to recover by itself
								OnResourceChanged(pRes, pNewRes);
							}
						}
					}
				}

				iter = m_refreshResources.erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
#endif
}

#if defined(DEDICATED_SERVER)
void CDownloadMgr::WeNeedToQuit()
{
	CryLog("CDownloadMgr::WeNeedToQuit() quitOnNewDataFound=%d", g_pGameCVars->g_quitOnNewDataFound);
	if (gEnv->IsDedicated() && g_pGameCVars->g_quitOnNewDataFound)
	{
		int numPlayers = 0;
		CGameLobby* pLobby = g_pGame->GetGameLobby();
		if (pLobby)
		{
			numPlayers = pLobby->GetNumberOfExpectedClients();
			pLobby->QueueSessionUpdate();
		}
		if (g_pGameCVars->g_quitNumRoundsWarning > 0 && numPlayers > 0)
		{
			m_roundsUntilQuit = g_pGameCVars->g_quitNumRoundsWarning;
		}
		else
		{
			InitShutdown();
		}
	}
}

void CDownloadMgr::OnResourceChanged(
	CDownloadableResourcePtr	pOldResouce,
	CDownloadableResourcePtr	pNewRes)
{
	CryLog("CDownloadMgr::OnResourceChanged() so calling WeNeedToQuit()");
	WeNeedToQuit();
}

void CDownloadMgr::InitShutdown()
{
	// Kick all players and wait until stats finished writing then quit
	CryLogAlways("Server is shutting down now to apply data patch");
	m_isShutingDown = true;
	m_roundsUntilQuit = 0;
	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	if (pLobby)
	{
		ICryMatchMaking *pMatchmaking = pLobby->GetMatchMaking();
		if (pMatchmaking)
		{
			CGameLobby* pGameLobby = g_pGame->GetGameLobby();
			if (pGameLobby)
			{
				const SSessionNames &sessionNames = pGameLobby->GetSessionNames();
				const int namesCount = sessionNames.m_sessionNames.size();
				for(int i = 0; i < namesCount; i++)
				{
					pMatchmaking->Kick(&sessionNames.m_sessionNames[i].m_userId, eDC_Kicked);
				}
			}
		}
	}
}
#endif

void CDownloadableResource::UpdateRemoveListeners()
{
	for (CDownloadableResource::TListenerVector::iterator listenerIter(m_listenersToRemove.begin()), end(m_listenersToRemove.end()); listenerIter != end; ++listenerIter)
	{
		stl::find_and_erase(m_listeners, (*listenerIter));
	}

	m_listenersToRemove.clear();
}

void CDownloadableResource::LoadConfig(
	XmlNodeRef				inNode)
{
	XmlString	str;

	inNode->getAttr("port",m_port);
	inNode->getAttr("maxSize",m_maxDownloadSize);

#define ReadXMLStr(key,out)		if (inNode->getAttr(key,str)) { out=str.c_str(); }
	ReadXMLStr("server",m_server);
	ReadXMLStr("name",m_descName);
	ReadXMLStr("prefix",m_urlPrefix);
	if (inNode->getAttr("url",str))
	{
		m_url.Format("%s/%s",k_platformPrefix,str.c_str());
	}
#undef ReadXMLStr

#ifndef _RELEASE
	CryLog ("Loaded downloadable resource config (server='%s', name='%s', URL prefix='%s', URL='%s', port=%d, max size=%d)", m_server.c_str(), m_descName.c_str(), m_urlPrefix.c_str(), m_url.c_str(), m_port, m_maxDownloadSize);
#endif
}

bool CDownloadableResource::Validate()
{
	bool		ok=true;

	if (m_port==0 ||
		m_maxDownloadSize==0 ||
		m_server.length()==0 ||
		m_descName.length()==0 ||
		m_url.length()==0)
	{
		ok=false;
	}

	return ok;
}

void CDownloadMgr::Init(
	const char    *pTCPServicesDescriptionFile,
	const char		*pInResourceDescriptionsFile)
{
	m_resources.clear();

	XmlNodeRef		node=GetISystem()->LoadXmlFromFile(pTCPServicesDescriptionFile);

	if (!node || strcmp("TCPServices",node->getTag()))
	{
		GameWarning("Failed to load TCP services config file %s", pTCPServicesDescriptionFile);
		return;
	}

	CDownloadableResource	defaultResource;
	XmlNodeRef						defaultXML(0);

	for (int i=0, n=node->getChildCount(); i<n; i++)
	{
		XmlNodeRef				x=node->getChild(i);
		
		if (x && strcmp(x->getTag(),"TCPService")==0)
		{
			const char			*config=NULL;
			if (x->getAttr("config",&config) && strcmp(config,g_pGameCVars->g_downloadMgrDataCentreConfig)==0)
			{
				defaultXML=x;
				break;
			}
		}
	}

	if (defaultXML)
	{
		defaultResource.LoadConfig(defaultXML);
	}

	node = GetISystem()->LoadXmlFromFile( pInResourceDescriptionsFile );

	XmlNodeRef				resources=node->findChild("resources");
	if (resources)
	{
		int numResources=resources->getChildCount();

		m_resources.reserve(numResources+k_localizedResourceSlop);

		for (int i=0; i<numResources; ++i)
		{
			bool						ok=false;
			CDownloadableResourcePtr	pParse=new CDownloadableResource(defaultResource);

			if (pParse)
			{
				pParse->LoadConfig(resources->getChild(i));
				ok=pParse->Validate();
			}

			if (!ok)
			{
				GameWarning("Error loading resource description %d %s from %s",i,pParse->m_descName.c_str(),pInResourceDescriptionsFile);
			}
			else
			{
				m_resources.push_back(pParse);
			}
		}
	}

#if defined(DEDICATED_SERVER)
	CPatchPakManager *pPatchPakManager = g_pGame->GetPatchPakManager();
	CRY_ASSERT(pPatchPakManager);
	pPatchPakManager->RegisterPatchPakManagerEventListener(this);
#endif
}

// IPatchPakManagerListener
void CDownloadMgr::UpdatedPermissionsNowAvailable()
{
#if defined(DEDICATED_SERVER)
	CryLog("CDownloadManager::UpdatedPermissionsNowAvailable() so calling WeNeedToQuit()");
	WeNeedToQuit();
#endif
}

// stores received data into more permanent buffer
bool CDownloadableResource::StoreData(
	const char						*pInData,
	size_t							inDataLen)
{
	bool							finishedWithStream=false;
	int								newUsed=0;
	int								newSize=0;
	char*							pNewBuffer=NULL;

	if ( pInData && inDataLen )
	{
		newUsed = m_bufferUsed + inDataLen;

		if ( newUsed > m_bufferSize)
		{
			int						expectedSize=m_contentLength+m_contentOffset;

			if (newUsed<=m_maxDownloadSize)
			{
				// Reallocate buffer
				// don't allocate a buffer smaller than the http header size, and if we parsed the header don't reallocate to smaller than the data size quoted in the content header
				newSize = max(newUsed, max((int)k_httpHeaderSize,expectedSize));
				pNewBuffer = new char[ newSize ];

				if ( m_pBuffer )
				{
					memcpy( pNewBuffer, m_pBuffer, m_bufferUsed );
				}
			}
			else
			{
				m_state=k_failedReplyContentTooLong;
				finishedWithStream=true;
			}
		}
		else
		{
			// Append to old buffer.
			pNewBuffer = m_pBuffer;
		}

		if ( pNewBuffer )
		{
			memcpy( pNewBuffer + m_bufferUsed, pInData, inDataLen );
			m_bufferUsed = newUsed;

			if ( pNewBuffer != m_pBuffer )
			{
				m_bufferSize = newSize;
				SAFE_DELETE_ARRAY( m_pBuffer );
				m_pBuffer = pNewBuffer;
			}
		}
	}

	return finishedWithStream;
}

bool CDownloadableResource::ReceiveHTTPHeader(
	bool							inReceivedEndOfStream)
{
	bool							finishedWithStream=false;

	if (m_state==k_awaitingHTTPResponse)
	{
		// see if we've received the http header yet
		static const char	k_httpOKv10[]="HTTP/1.0 200";
		static const char	k_httpOKv11[]="HTTP/1.1 200";
		static const char	k_httpContentLength[]="Content-Length: ";

		if (m_bufferUsed>=k_httpHeaderSize || inReceivedEndOfStream)
		{
			bool			badHeader=true;
			bool waitingOnContentLength=false;

			// check for http/ok response
			if (m_bufferUsed>=(sizeof(k_httpOKv10)-1) && (memcmp(k_httpOKv10,m_pBuffer,sizeof(k_httpOKv10)-1)==0 || memcmp(k_httpOKv11,m_pBuffer,sizeof(k_httpOKv11)-1)==0))
			{
				// check for data payload information
				char		lengthStr[12];
				const char	*contentLength=CryStringUtils::strnstr(m_pBuffer,k_httpContentLength,m_bufferUsed);

				if (contentLength && ((contentLength-m_pBuffer)+sizeof(lengthStr)) < (size_t)m_bufferUsed)
				{
					size_t	found=cry_copyStringUntilFindChar(lengthStr,contentLength+sizeof(k_httpContentLength)-1,sizeof(lengthStr),'\n');

					if (found!=0)
					{
						int	len=atoi(lengthStr);

						if (len>=0 && len<m_maxDownloadSize)
						{
							m_contentLength=len;

							// now scan for end of header, which is denoted by two CRLFs in http protocol
							for (const char *iter=contentLength; iter<(m_pBuffer+m_bufferUsed-3); ++iter)
							{
								if (iter[0]=='\r' && iter[1]=='\n' && iter[2]=='\r' && iter[3]=='\n')			// some bad http servers don't do CRLF, but do LF LF instead. we won't use those servers
								{
									m_contentOffset=int(iter+4-m_pBuffer);
									badHeader=false;
									break;
								}
							}
						}
						else
						{
							m_state=k_failedReplyContentTooLong;
							finishedWithStream=true;
						}
					}
				}
				else if (!contentLength)
				{ 
					if (inReceivedEndOfStream)
					{
						// we have no content length returned derive content length manually now we've received the entire stream

						int contentOffset=0;

						// now scan for end of header, which is denoted by two CRLFs in http protocol
						for (const char *iter=m_pBuffer; iter<(m_pBuffer+m_bufferUsed-3); ++iter)
						{
							if (iter[0]=='\r' && iter[1]=='\n' && iter[2]=='\r' && iter[3]=='\n')			// some bad http servers don't do CRLF, but do LF LF instead. we won't use those servers
							{
								contentOffset=int(iter+4-m_pBuffer);
								break;
							}
						}

						if (contentOffset > 0)
						{
							int currentContentLength = m_bufferUsed - contentOffset;
							if (currentContentLength > 0 && currentContentLength < m_maxDownloadSize)
							{
								m_contentLength = currentContentLength;
								m_contentOffset = contentOffset;
								badHeader=false;
							}
							else
							{
								m_state=k_failedReplyContentTooLong;
								finishedWithStream=true;
							}
						}
					}
					else
					{
						badHeader=false;
						waitingOnContentLength=true;
					}
				}
			}

			if (m_state&k_awaitingHTTPResponse)
			{
				if (badHeader)
				{
					m_state=k_failedReplyHasBadHeader;
					finishedWithStream=true;
				}
				else
				{
					if (!waitingOnContentLength)
					{
						m_state=k_awaitingPayload;
					}
				}
			}
		}
	}

	return finishedWithStream;
}

// static
// called when a response is receieved from the http server
// parses http header and receives expected bytes from server
bool CDownloadableResource::ReceiveDataCallback(
	ECryTCPServiceResult	res,
	void*									pArg,
	STCPServiceDataPtr		pUpload,
	const char*						pData,
	size_t								dataLen,
	bool									endOfStream )
{
	bool										finishedWithStream=false;
	CDownloadableResource		*pResource=static_cast<CDownloadableResource*>(pArg);

	if (!(pResource->m_state & k_callbackInProgressMask))
	{
		// If we end up in this function with any permanent fail, we should have already released so don't do it again.
		// This should fix a double release crash.
		finishedWithStream = true;
	}
	else
	{
		if (pResource->m_abortDownload)
		{
			pResource->m_state=k_failedAborted;
			pResource->Release();		// balances AddRef() in StartDownloading()
			finishedWithStream=true;
		}
		else if (res==eCTCPSR_Ok )
		{
			finishedWithStream=pResource->StoreData(pData,dataLen);

			if (!finishedWithStream)
			{
				finishedWithStream=pResource->ReceiveHTTPHeader(endOfStream);
			}

			if (pResource->m_state==k_awaitingPayload && (pResource->m_bufferUsed-pResource->m_contentOffset)>=pResource->m_contentLength)
			{
				pResource->m_state=k_dataAvailable;
			}

			if (pUpload->m_quietTimer>k_downloadableResourceHTTPTimeout)
			{
				pResource->m_state=k_failedReplyTimedOut;
				finishedWithStream=true;
			}

			if (endOfStream || finishedWithStream || pResource->m_state==k_dataAvailable)
			{
				if ((pResource->m_state&(k_dataPermanentFailMask|k_dataAvailable))==0)
				{
					pResource->m_state=k_failedReplyContentTruncated;
				}
				finishedWithStream=true;
				pResource->Release();	// balances AddRef() in StartDownloading()
			}
		}
		else
		{
			pResource->m_state=k_failedServerUnreachable;
			pResource->Release();		// balances AddRef() in StartDownloading()
			finishedWithStream=true;
		}
	}

	return finishedWithStream;
}

CDownloadableResourcePtr CDownloadMgr::FindResourceByName(
	const char			*inResourceName)
{
	CDownloadableResourcePtr	pResult=NULL;

	for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
	{
		CDownloadableResourcePtr		pPtr=*iter;

		if (pPtr->m_descName==inResourceName)
		{
			pResult=pPtr;
			break;
		}
	}

	return pResult;
}

void CDownloadMgr::PurgeLocalizedResourceByName(
	const char									*inResourceName)
{
	CDownloadableResourcePtr		templateResource=FindResourceByName(inResourceName);

	if (templateResource)
	{
		for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
		{
			CDownloadableResourcePtr		pPtr=*iter;

			if (pPtr->m_isLocalisedInstanceOf==templateResource)
			{
				pPtr->CancelDownload();
				m_resources.erase(iter);

				// there should only be one instance per localised template, so we can stop looking now
				break;
			}
		}
	}
}

// finds the localised resource with the name specified
// this will create a localised instance of a resource from a template if it does not yet exist
// if the language has changed since the last time this was called, it will free the previous
// localised instance before creating the new one
CDownloadableResourcePtr CDownloadMgr::FindLocalizedResourceByName(
	const char									*inResourceName)
{
	CDownloadableResourcePtr		templateResource=FindResourceByName(inResourceName);
	CDownloadableResourcePtr		result=NULL;

	if (templateResource)
	{
		CryFixedStringT<128>			url=templateResource->m_url;
		ICVar											*pLanguage=gEnv->pConsole->GetCVar("g_language");

		CRY_ASSERT_MESSAGE(pLanguage,"can't find language cvar??");
		if (pLanguage)
		{
			url.replace("%LANGUAGE%",pLanguage->GetString());

			for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
			{
				CDownloadableResourcePtr		pPtr=*iter;

				if (pPtr->m_isLocalisedInstanceOf==templateResource)
				{
					if (pPtr->m_url==url)
					{
						result=pPtr;
						break;
					}
					else
					{
						// found an existing localised instance of this template, which isn't for the current language - we should purge this as that language is no longer selected
						pPtr->CancelDownload();
						m_resources.erase(iter);

						// there should only be one instance per localised template, so we can stop looking now
						break;
					}
				}
			}

			if (!result)
			{
				result=new CDownloadableResource();

				if (result)
				{
					result->m_urlPrefix=templateResource->m_urlPrefix;
					result->m_url=url;
					result->m_server=templateResource->m_server;
					result->m_descName.Format("%s_%s",templateResource->m_descName.c_str(),pLanguage->GetString());		// don't want to reuse name of template resource - so make a unique one
					result->m_port=templateResource->m_port;
					result->m_maxDownloadSize=templateResource->m_maxDownloadSize;
					result->m_isLocalisedInstanceOf=templateResource;

					m_resources.push_back(result);
				}
			}
		}
	}

	return result;
}

// counter part of InitHTTPParser() - call before releasing the resource if you have called InitHTTPParser() on it
void CDownloadableResource::ReleaseHTTPParser()
{
	if (m_doingHTTPParse)
	{
		if (m_state&k_callbackInProgressMask)
		{
			// not completed - set to error state
			m_state=k_failedAborted;
			Release();		// balanced against AddRef() in InitHTTPParser()
		}
		m_doingHTTPParse=false;
	}
}

// this is a way of initialising a downloadble resource if you want to use it to parse http replies
void CDownloadableResource::InitHTTPParser()
{
	assert(m_state==k_notStarted);
	assert(!m_doingHTTPParse);
	static const int	k_maxDownloadSizeForHTTPHeader=4*1024;
	m_maxDownloadSize=k_maxDownloadSizeForHTTPHeader;
	m_state=k_awaitingHTTPResponse;
	m_doingHTTPParse=true;
	AddRef();
#if DOWNLOAD_MGR_DBG
	m_downloadStarted=gEnv->pTimer->GetAsyncCurTime();
#endif
}

void CDownloadableResource::GetUserAgentString(CryFixedStringT<64> &ioAgentStr)
{
	const char *gameTitle="GameSDK";
	const char *version="1.0";

	if (gEnv->IsDedicated())
	{
		ioAgentStr.Format("%sDediPC/%s", gameTitle, version);
	}
	else
	{
		ioAgentStr.Format("%sPC/%s", gameTitle, version);
	}
}

void CDownloadableResource::StartDownloading()
{
	if (m_state==k_notStarted)
	{
		if (m_port==0)
		{
			GameWarning("Error tried to start downloading on unconfigured CDownloadableResource");
			m_state=k_failedInternalError;
		}
		else
		{
			STCPServiceDataPtr		pTransaction(NULL);
			ICryLobby* pLobby = gEnv->pNetwork->GetLobby();

			if (!pLobby)
			{
				GameWarning("Error: No Lobby available - CDownloadableResource");
				return;
			}

			ICryLobbyService* pLobbyService = pLobby->GetLobbyService();

			if(pLobbyService)
				m_pService=gEnv->pNetwork->GetLobby()->GetLobbyService()->GetTCPService(m_server,m_port,m_urlPrefix);

			if (m_pService)
			{
				static const int					MAX_HEADER_SIZE=400;
				CryFixedStringT<MAX_HEADER_SIZE>	httpHeader;
				CryFixedStringT<64> agentStr;

				GetUserAgentString(agentStr);

				// For HTTP 1.0.
				/*httpHeader.Format(
				"GET /%s%s HTTP/1.0\n"
				"\n",
				m_urlPrefix.c_str(),
				m_url.c_str());*/

				// For HTTP 1.1. Needed to download data from servers that are "multi-homed"
				httpHeader.Format(
					"GET /%s%s HTTP/1.1\n"
					"Pragma: no-cache\n"
					"User-Agent: %s\n"
					"Host: %s:%d\n"
					"\n",
					m_urlPrefix.c_str(),
					m_url.c_str(),
					agentStr.c_str(),
					m_server.c_str(),
					m_port);

				pTransaction=new STCPServiceData();

				if (pTransaction)
				{
					int		len=httpHeader.length();
					pTransaction->length=len;
					pTransaction->pData=new char[len];
					if (pTransaction->pData)
					{
						memcpy(pTransaction->pData,httpHeader.c_str(),len);
						pTransaction->tcpServReplyCb=ReceiveDataCallback;
						pTransaction->pUserArg=this;		// do ref counting manually for callback data
						this->AddRef();
#if DOWNLOAD_MGR_DBG
						m_downloadStarted=gEnv->pTimer->GetAsyncCurTime();
#endif
						m_state=k_awaitingHTTPResponse;
						if (!m_pService->UploadData(pTransaction))
						{
							this->Release();
							pTransaction=NULL;
						}
					}
					else
					{
						pTransaction=NULL;
					}
				}
			}

			if (!pTransaction)
			{
				m_state=k_failedInternalError;
			}
		}
	}
}

bool CDownloadableResource::Purge()
{
	bool		ok=false;

	if (!(m_state&k_callbackInProgressMask))
	{
		m_state=k_notStarted;
		m_broadcastedState=k_notBroadcasted;
		m_abortDownload=false;

		SAFE_DELETE_ARRAY(m_pBuffer);
		m_bufferUsed=m_bufferSize=0;
		m_contentLength=m_contentOffset=0;

		ok=true;
	}

	return ok;
}

void CDownloadableResource::SetDownloadInfo(const char* pUrl, const char* pUrlPrefix, const char* pServer, const int port, const int maxDownloadSize, const char* pDescName/*=NULL*/)
{
	CRY_ASSERT(pUrl && pServer && maxDownloadSize>0);

	CRY_ASSERT_MESSAGE(m_port==0,"You cannot call CDownloadableResource::SetDownloadInfo() twice on the same resource");		// not unless the code is strengthened anyway
	CRY_ASSERT_MESSAGE(port!=0,"You cannot request a download from port 0");		// invalid generally, but also we use 0 as not initialized here

	m_port = port;
	m_maxDownloadSize = maxDownloadSize;

	m_server = pServer;
	m_descName = pDescName ? pDescName : pUrl;
	m_urlPrefix = pUrlPrefix;
	m_url = pUrl;
}

CDownloadableResource::TState CDownloadableResource::GetRawData(
	char				**pOutData,
	int					*pOutLen)
{
	StartDownloading();

	if (m_state==k_dataAvailable)
	{
		*pOutData=m_pBuffer+m_contentOffset;
		*pOutLen=m_contentLength;
	}
	else
	{
		*pOutData=NULL;
		*pOutLen=0;
	}

	return m_state;
}

// func returns true if the data is correctly encrypted and signed. caller is then responsible for calling delete[] on the returned data buffer
// returns false if there is a problem, caller has no data to free if false is returned
bool CDownloadableResource::DecryptAndCheckSigning(
	const char	*pInData,
	int					inDataLen,
	char				**pOutData,
	int					*pOutDataLen,
	const char	*pDecryptionKey,
	int					decryptionKeyLength,
	const char	*pSigningSalt,
	int					signingSaltLength)
{
	char							*pOutput=NULL;
	int								outDataLen=0;
	bool							valid=false;

	if (inDataLen>16)
	{
		INetwork					*pNet=GetISystem()->GetINetwork();
		IZLibCompressor		*pZLib=GetISystem()->GetIZLibCompressor();
		TCipher						cipher=pNet->BeginCipher((uint8*)pDecryptionKey,decryptionKeyLength);
		if (cipher)
		{
			pOutput=new char[inDataLen];

			pNet->Decrypt(cipher,(uint8*)pOutput,(const uint8*)pInData,inDataLen);

			// validate cksum to check for successful decryption and for data signing
			SMD5Context		ctx;
			char					digest[16];

			pZLib->MD5Init(&ctx);
			pZLib->MD5Update(&ctx,pSigningSalt,signingSaltLength);
			pZLib->MD5Update(&ctx,pOutput,inDataLen-16);
			pZLib->MD5Final(&ctx,digest);

			if (memcmp(digest,pOutput+inDataLen-16,16)!=0)
			{
				CryLog("MD5 fail on downloaded data");
			}
			else
			{
				CryLog("data passed decrypt and MD5 checks");
				valid=true;
			}

			pNet->EndCipher(cipher);
		}
	}

	if (valid)
	{
		*pOutData=pOutput;
		*pOutDataLen=inDataLen-16;
	}
	else
	{
		if (pOutput)
		{
			delete [] pOutput;
		}
		*pOutData=NULL;
		*pOutDataLen=0;
	}

	return valid;
}

CDownloadableResource::TState CDownloadableResource::GetDecryptedData(
	char				*pBuffer,
	int					*pOutLen,
	const char	*pDecryptionKey,
	int					decryptionKeyLength,
	const char	*pSigningSalt,
	int					signingSaltLength)
{
	StartDownloading();

	bool success = false;
	if (m_state==k_dataAvailable)
	{
		char* pEncryptedBuffer = m_pBuffer+m_contentOffset;
		int encryptedLength = m_contentLength;
		char* pDecryptedBuffer = NULL;
		int decryptedLength = 0;
		if (DecryptAndCheckSigning(pEncryptedBuffer, encryptedLength, &pDecryptedBuffer, &decryptedLength,
			pDecryptionKey, decryptionKeyLength, pSigningSalt, signingSaltLength))
		{
			unsigned long destSize = *pOutLen;
			int error = gEnv->pCryPak->RawUncompress(pBuffer, &destSize, pDecryptedBuffer, (unsigned long)decryptedLength);

			if (error == 0)
			{
				*pOutLen = (int)destSize;
				success = true;
			}
			else
			{
				CryLog("Failed to uncompress data");
			}

			delete [] pDecryptedBuffer;
			pDecryptedBuffer = NULL;
		}
	}
	if (!success)
	{
		*pOutLen=0;
	}

	return m_state;
}

CDownloadableResource::TState CDownloadableResource::GetProgress(
	int					*outBytesDownloaded,
	int					*outTotalBytesToDownload)
{
	if (m_state&(k_dataAvailable|k_awaitingPayload))
	{
		*outBytesDownloaded=m_bufferUsed-m_contentOffset;
		*outTotalBytesToDownload=m_contentLength;
	}
	else
	{
		*outBytesDownloaded=*outTotalBytesToDownload=0;
	}

	return m_state;
}

void CDownloadableResource::AddDataListener(
	IDataListener	*pInListener)
{
	CRY_ASSERT_MESSAGE(std::find(m_listeners.begin(), m_listeners.end(), pInListener) == m_listeners.end(), "A downloadable resource should not have two instances of the same listener");
	m_listeners.push_back(pInListener);

	switch (m_broadcastedState)
	{
		case k_broadcastedSuccess:
			pInListener->DataDownloaded(this);
			break;

		case k_broadcastedFail:
			pInListener->DataFailedToDownload(this);
			break;

		default:
			break;
	}

	StartDownloading();
}

void CDownloadableResource::RemoveDataListener(
	IDataListener	*pInListener)
{
	// these are nothing to worry about - it is not uncommon to unregister twice if there is an additional unregister in a destructor
	// better do unreg twice than have a dangling ptr
//	CRY_ASSERT_MESSAGE(std::find(m_listenersToRemove.begin(), m_listenersToRemove.end(), pInListener) == m_listenersToRemove.end(), "Trying to remove a data listener twice!");
//	CRY_ASSERT_MESSAGE(std::find(m_listeners.begin(), m_listeners.end(), pInListener) != m_listeners.end(), "trying to remove a listener thats not in the listners list");
	m_listenersToRemove.push_back(pInListener);
}

IDataListener::~IDataListener()
{
#if DOWNLOAD_MGR_DBG
	if (g_pGame)
	{
		CDownloadMgr		*pMgr=g_pGame->GetDownloadMgr();
		if (pMgr)
		{
			CRY_ASSERT_MESSAGE(!pMgr->IsListenerListening(this),"IDataListener is being deleted but it is still registered as a listener - bad code!");
		}
	}
#endif

}

// only call from the main thread as it will broadcast to listeners which are not required to be thread safe
void CDownloadableResource::CancelDownload()
{
	// it is possible to have a callback in flight and for it to have broadcasted, if it has been cancelled previously
	if ((m_state&k_callbackInProgressMask) && (m_broadcastedState==k_notBroadcasted))
	{
		m_abortDownload=true;		// will abort on next callback received from network thread
		UpdateRemoveListeners();
		BroadcastFail();				// broadcast the fail now
	}
}

void CDownloadMgr::WaitForDownloadsToFinish(const char** resources, int numResources, float timeout)
{
	CDownloadableResourcePtr* pResources=new CDownloadableResourcePtr[numResources];
	for (int i=0; i<numResources; ++i)
	{
		CDownloadableResourcePtr pRes = FindResourceByName(resources[i]);
		if (pRes)
		{
			pRes->StartDownloading();
		}
		pResources[i] = pRes;
	}
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
	while (true)
	{
		bool allFinished = true;
		for (int i=0; i<numResources; ++i)
		{
			CDownloadableResourcePtr pRes = pResources[i];
			if (pRes)
			{
				CDownloadableResource::TState state = pRes->GetState();
				if (state & CDownloadableResource::k_callbackInProgressMask)
				{
					allFinished = false;
					break;
				}
			}
		}
		CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		if (allFinished || currentTime.GetDifferenceInSeconds(startTime) > timeout)
		{
			break;
		}
		CrySleep(100);
	};
	delete [] pResources;
	DispatchCallbacks();
}

#if defined(DEDICATED_SERVER)
void CDownloadMgr::OnReturnToLobby()
{
	if (gEnv->IsDedicated() && g_pGameCVars->g_quitOnNewDataFound)
	{
		if (m_roundsUntilQuit > 0)
		{
			--m_roundsUntilQuit;
			int numPlayers = g_pGame->GetGameLobby()->GetNumberOfExpectedClients();
			if (m_roundsUntilQuit == 0 || numPlayers == 0)
			{
				InitShutdown();
			}
			else
			{
				CryLogAlways("Server is shutting down in %d rounds to apply data patch", m_roundsUntilQuit);
			}
		}
	}
}

void CDownloadMgr::CheckUpdates(const char *pInResourceName)
{
	CDownloadableResourcePtr pRes = FindResourceByName(pInResourceName);
	if (pRes)
	{
		for (TResourceVector::iterator iter=m_refreshResources.begin(); iter!=m_refreshResources.end(); ++iter)
		{
			if (strcmp((*iter)->GetDescription(), pInResourceName) == 0)
			{
				// Already checking for updates, don't need to do it again
				return;
			}
		}
		CDownloadableResourcePtr pNewRes = new CDownloadableResource();
		pNewRes->SetDownloadInfo(pRes->GetURL(), pRes->GetURLPrefix(), pRes->GetServer(), pRes->GetPort(), pRes->GetMaxSize(), pRes->GetDescription());
		pNewRes->StartDownloading();
		m_refreshResources.push_back(pNewRes);
	}
}

bool CDownloadMgr::IsWaitingToShutdown()
{
	return m_roundsUntilQuit > 0 || m_isShutingDown;
}
#endif

#if DOWNLOAD_MGR_DBG

bool CDownloadMgr::IsListenerListening(
	IDataListener		*inListener)
{
	bool						found=false;

	for (TResourceVector::iterator iter=m_resources.begin(); iter!=m_resources.end(); ++iter)
	{
		CDownloadableResourcePtr		pPtr=*iter;

		if (std::find(pPtr->m_listeners.begin(),pPtr->m_listeners.end(),inListener)!=pPtr->m_listeners.end())			// if in listener list
		{
			if (std::find(pPtr->m_listenersToRemove.begin(),pPtr->m_listenersToRemove.end(),inListener)==pPtr->m_listenersToRemove.end())		// and not in remove list
			{
				found=true;
				break;
			}
		}
	}

	return found;
}

// static
string CDownloadableResource::GetStateAsString(
	TState					inState)
{
	return AutoEnum_GetStringFromBitfield(inState,k_downloadableResourceStateNames,CRY_ARRAY_COUNT(k_downloadableResourceStateNames));
}

// static
void CDownloadMgr::DbgList(
	IConsoleCmdArgs			*pInArgs)
{
	CDownloadMgr			*pDlm=static_cast<CGame*>(g_pGame)->GetDownloadMgr();
	int						count=0;

	for (TResourceVector::iterator iter=pDlm->m_resources.begin(); iter!=pDlm->m_resources.end(); ++iter)
	{
		CDownloadableResourcePtr	pPtr=*iter;
		const char								*ss=CDownloadableResource::GetStateAsString(pPtr->m_state).c_str();

		float				timeElapsed=0.0f;
		if (!(pPtr->m_state&CDownloadableResource::k_notStarted))
		{
			if (pPtr->m_state&CDownloadableResource::k_dataAvailable)
			{
				timeElapsed=(pPtr->m_downloadFinished-pPtr->m_downloadStarted).GetSeconds();
			}
			else
			{
				timeElapsed=(CTimeValue(gEnv->pTimer->GetAsyncCurTime())-pPtr->m_downloadStarted).GetSeconds();
			}
		}

		CryLogAlways("Resource %d : %s : state %s    : content %d B / %d B (%.2f bytes/sec) (%.2f elapsed)",
			count,pPtr->m_descName.c_str(), ss, pPtr->m_bufferUsed-pPtr->m_contentOffset, pPtr->m_contentLength, pPtr->GetTransferRate(),timeElapsed);

		count++;
	}

	CryLogAlways("%d resources",count);
}

// static
void CDownloadMgr::DbgPurge(
	IConsoleCmdArgs				*pInArgs)
{
	CDownloadMgr				*pDlm=static_cast<CGame*>(g_pGame)->GetDownloadMgr();

	if (pInArgs->GetArgCount()==2)
	{
		string						resName=pInArgs->GetArg(1);
		CDownloadableResourcePtr	pRes=pDlm->FindResourceByName(resName);

		if (!pRes)
		{
			CryLogAlways("Unknown resource '%s'",resName.c_str());
		}
		else
		{
			string		wasState=pRes->GetStateAsString();
			if (pRes->Purge())
			{
				CryLogAlways("Resource '%s' purged, was in state %s",resName.c_str(),wasState.c_str());
			}
			else
			{
				CryLogAlways("Resource '%s' NOT purged, callback in progress, in state %s",resName.c_str(),wasState.c_str());
			}
		}
	}
	else
	{
		CryLogAlways("usage: dlm_purge <resourcename>");
	}
}

// static
void CDownloadMgr::DbgCat(
	IConsoleCmdArgs				*pInArgs)
{
	CDownloadMgr				*pDlm=static_cast<CGame*>(g_pGame)->GetDownloadMgr();

	if (pInArgs->GetArgCount()==2)
	{
		string						resName=pInArgs->GetArg(1);
		CDownloadableResourcePtr	pRes=pDlm->FindResourceByName(resName);

		if (!pRes)
		{
			CryLogAlways("Unknown resource '%s'",resName.c_str());
		}
		else
		{
			char								*pPtr;
			int									size;
			CDownloadableResource::TState		state=pRes->GetRawData(&pPtr,&size);

			if (state==CDownloadableResource::k_dataAvailable)
			{
				char		*pMyBigBuffer=new char[size+1];
				if (pMyBigBuffer)
				{
					memcpy(pMyBigBuffer,pPtr,size);
					pMyBigBuffer[size]=0;
					CryLogAlways("%s",pMyBigBuffer);
					delete [] pMyBigBuffer;
					// ... i don't even want to think about what all the above just did to the memory state...

					CryLogAlways("Resource '%s' %d bytes downloaded",pRes->m_descName.c_str(),size);
				}
				else
				{
					CryLogAlways("Out of mem");
				}
			}
			else
			{
				CryLogAlways("Resource '%s' state %s",pRes->m_descName.c_str(),pRes->GetStateAsString().c_str());
			}
		}
	}
	else
	{
		CryLogAlways("usage: dlm_cat <resourcename>");
	}
}

void CDownloadableResource::DebugContentsToString(CryFixedStringT<512> &outStr)
{
	const char *ss=CDownloadableResource::GetStateAsString(m_state).c_str();
	float				timeElapsed=0.0f;
	if (!(m_state&k_notStarted))
	{
		if (m_state&k_dataAvailable)
		{
			timeElapsed=(m_downloadFinished-m_downloadStarted).GetSeconds();
		}
		else
		{
			timeElapsed=(CTimeValue(gEnv->pTimer->GetAsyncCurTime())-m_downloadStarted).GetSeconds();
		}
	}
	outStr.Format("Resource %s : state %s    : content %d B / %d B (%.2f bytes/sec) (elapsed %.2f sec)",
					 m_descName.c_str(), ss, m_bufferUsed-m_contentOffset, m_contentLength, GetTransferRate(),timeElapsed);
}

void CDownloadableResource::DebugWatchContents()
{
	CryFixedStringT<512> debugString;

	DebugContentsToString(debugString);
	CryWatch(debugString.c_str());
}

float CDownloadableResource::GetTransferRate()
{
	float		trate=0.0f;

	if (!(m_state&k_notStarted))
	{
		if (m_state&k_dataAvailable)
		{
			float		took=(m_downloadFinished-m_downloadStarted).GetSeconds();

			if (took>0.0f)
			{
				trate=float(m_bufferUsed)/float(took);
			}
		}
		else
		{
			CTimeValue now=gEnv->pTimer->GetAsyncCurTime();
			float			taken=(now-m_downloadStarted).GetSeconds();

			if (taken>0.0f)
			{
				trate=float(m_bufferUsed)/float(taken);
			}
		}
	}

	return trate;
}

#endif
