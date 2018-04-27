// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Allows game data to be patched via xml patch files downloaded
					over the internet

	-------------------------------------------------------------------------
	History:
	- 13:12:2010  : Created by Mark Tully

*************************************************************************/

#include "StdAfx.h"
#include "DataPatchDownloader.h"
#include <CryString/StringUtils.h>
#include "GameCVars.h"
#include "Network/Lobby/GameLobbyData.h"
#include "RevertibleConfigLoader.h"

// SECRET
#define DECRYPTION_KEY														"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define SIGNING_SALT															"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
// SECRET

CDataPatchDownloader::CDataPatchDownloader() :
	m_pListener(NULL),
	m_patchCRC(0),
	m_patchId(0),
	m_pFileBeingPatched(NULL),
	m_patchingEnabled(false),
	m_bNeedWeaponSystemReload(true)
{
	CDownloadableResourcePtr			res=GetDownloadableResource();

	if (res)
	{
		res->AddDataListener(this);
	}
}

CDataPatchDownloader::~CDataPatchDownloader()
{
	CDownloadableResourcePtr			res=GetDownloadableResource();

	if (res)
	{
		res->RemoveDataListener(this);
	}

	//Disable patching of the XMLs by XMLUtil
	SetPatchingEnabled(false);
}

void CDataPatchDownloader::AddListener(
	IDataPatcherListener				*pInListener)
{
	if (pInListener)
	{
		CRY_ASSERT_MESSAGE(m_pListener==NULL || (pInListener==m_pListener),"Adding multiple listeners to CDataPatcher is not currently implemented");		// not needed currently
		m_pListener=pInListener;
		CDownloadableResourcePtr	pRes=GetDownloadableResource();
		if (pRes)
		{
			if (pRes->HasBroadcastedResults())
			{
				if (pRes->GetState()==CDownloadableResource::k_dataAvailable)
				{
					pInListener->DataPatchAvailable();
				}
				else
				{
					pInListener->DataPatchNotAvailable();
				}
			}
		}
	}
}

void CDataPatchDownloader::RemoveListener(
	IDataPatcherListener				*pInListener)
{
	if (pInListener==m_pListener)
	{
		m_pListener=NULL;
	}
}

void CDataPatchDownloader::DataDownloaded(
	CDownloadableResourcePtr		inResource)
{
	const int bufferSize = 1024*1024;
	char* pBuffer = new char[bufferSize];
	int dataLength = bufferSize;

	inResource->GetDecryptedData(pBuffer,&dataLength,DECRYPTION_KEY,int(sizeof(DECRYPTION_KEY)-1),SIGNING_SALT,int(sizeof(SIGNING_SALT)-1));

	if (dataLength > 0)
	{
		IXmlParser		*pParser=GetISystem()->GetXmlUtils()->CreateXmlParser();

		m_patchCRC=CCrc32::Compute(pBuffer,dataLength);
		m_patchXML=pParser->ParseBuffer(pBuffer,dataLength,false);
		if (!m_patchXML)
		{
			m_patchCRC = 0;
			CryLog("Failed to parse game data patch xml");
		}
		else
		{
			m_patchXML->getAttr("patchid",m_patchId);
			int matchmakingVersion = 0;
			if (m_patchXML->getAttr("matchmakingversion",matchmakingVersion))
			{
				if (matchmakingVersion != g_pGameCVars->g_MatchmakingVersion)
				{
					// Unload the patch XML, because it doesn't match the matchmaking version
					CryLog("Game data patch matchmaking version (%d) does not match this build's matchmaking version (%d)", matchmakingVersion, g_pGameCVars->g_MatchmakingVersion);
					m_patchCRC = 0;
					m_patchId = 0;
					m_patchXML = NULL;
				}
			}
		}

		pParser->Release();
	}

	delete [] pBuffer;

	// if it downloads the patch is marked as available, regardless of whether it parsed / passed checks
	// if it fails parsing or checks, it will end up with a CRC of 0 and so cause the game to follow paths
	// of not having a patch
	if (m_pListener)
	{
		m_pListener->DataPatchAvailable();
	}
}

void CDataPatchDownloader::DataFailedToDownload(
	CDownloadableResourcePtr		inResource)
{
	// nothing to do, leave CRC at default value
	CryLog("Failed to download data patch");
	if (m_pListener)
	{
		m_pListener->DataPatchNotAvailable();
	}
}

void CDataPatchDownloader::CancelDownload()
{
	CDownloadableResourcePtr pRes=GetDownloadableResource();
	if (pRes)
	{
		pRes->CancelDownload();
	}
}

CDownloadableResourcePtr CDataPatchDownloader::GetDownloadableResource()
{
	CDownloadableResourcePtr	pResult(NULL);
	CDownloadMgr							*pMgr=g_pGame->GetDownloadMgr();
	if (pMgr)
	{
		pResult=pMgr->FindResourceByName("datapatch");
	}
	return pResult;
}

void CDataPatchDownloader::SetPatchingEnabled(bool inEnable)
{
	m_patchingEnabled=(m_patchCRC!=0) && inEnable;

	if( m_patchingEnabled )
	{
		//Place a copy in CSystem to perform the XML patching. A copy is kept here for CVAR patching as that requires the Game pointer
		gEnv->pSystem->GetXmlUtils()->SetXMLPatcher(&m_patchXML);
	}
	else
	{
		//Clear the existing XML patcher
		gEnv->pSystem->GetXmlUtils()->SetXMLPatcher(NULL);
	}
}

void CDataPatchDownloader::AssertPatchDownloaded()
{
	CDownloadableResourcePtr	pRes=GetDownloadableResource();
	if (!pRes || (pRes->GetState()&(CDownloadableResource::k_dataAvailable|CDownloadableResource::k_dataPermanentFailMask))==0)
	{
		CRY_ASSERT_MESSAGE(0,"Attempted to access data patch before it has been downloaded - could lead to a race condition with some clients getting the data and others not");
	}
}

void CDataPatchDownloader::ApplyCVarPatch()
{
	// Open the file regardless of whether or not we're going to use it - makes sure the file is in the MP mode
	// switch pak
  FILE * f = gEnv->pCryPak->FOpen( "Scripts/DataPatcher/patchablecvars.txt", "rt" );
  if (!f)
	{
		// Failed to open whitelist - don't patch any cvars
    return;
	}

	if (m_patchingEnabled)
	{
		AssertPatchDownloaded();

		if (m_patchXML)
		{
			std::vector<string> cvarsWhiteList;
			cvarsWhiteList.reserve(64);

			// Parse the cvars white list - code taken from CVarListProcessor.cpp

		  const int BUFSZ = 4096;
		  char buf[BUFSZ];

		  size_t nRead;
		  string cvar;
		  bool comment = false;
		  do
		  {
				cvar.resize(0);
				buf[0]='\0';
		    nRead = gEnv->pCryPak->FRead( buf, BUFSZ, f );

		    for (size_t i=0; i<nRead; i++)
		    {
		      char c = buf[i];
		      if (comment)
		      {
		        if (c == '\r' || c == '\n')
		          comment = false;
		      }
		      else
		      {
		        if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '.'))
		        {
		          cvar += c;
		        }
		        else if (c == '\t' || c == '\r' || c == '\n' || c == ' ')
		        {
		          if (ICVar * pV = gEnv->pConsole->GetCVar(cvar.c_str()))
							{
								cvarsWhiteList.push_back(cvar);
							}
		          cvar.resize(0);
		        }
		        else if (c == '#')
		        {
		          comment = true;
		        }
		      }
		    }
		  }
		  while (nRead != 0);

			// Now apply the patch
			const int numAllowedCVars = cvarsWhiteList.size();

			const int numChildren = m_patchXML->getChildCount();
			for (int i = 0; i < numChildren; i ++)
			{
				XmlNodeRef xmlChild = m_patchXML->getChild(i);

				if (xmlChild->isTag("cvarpatch"))
				{
					const int numCVars = xmlChild->getChildCount();

					const char *pCVar = NULL;
					const char *pValue = NULL;

					for (int cvarIndex = 0; cvarIndex < numCVars; ++ cvarIndex)
					{
						XmlNodeRef xmlCVar = xmlChild->getChild(cvarIndex);
						
						if (xmlCVar->isTag("cvar"))
						{
							if (xmlCVar->getAttr("name", &pCVar) && xmlCVar->getAttr("value", &pValue))
							{
								bool bAllowed = false;

								for (int allowedIndex = 0; allowedIndex < numAllowedCVars; ++ allowedIndex)
								{
									string &allowedCVar = cvarsWhiteList[allowedIndex];
									if (!stricmp(allowedCVar.c_str(), pCVar))
									{
										bAllowed = true;
										break;
									}
								}

								if (bAllowed)
								{
									CryLog("ApplyCVarPatch() patching cvar '%s' to '%s'", pCVar, pValue);
									g_pGame->GetGameModeCVars().ApplyAndStoreCVar(pCVar, pValue);
								}
								else
								{
									CryLog("ApplyCVarPatch() cvar '%s' not allowed - ignoring", pCVar);
								}
							}
						}
					}

					break;
				}
			}
		}
	}

	gEnv->pCryPak->FClose( f );
}

#if DATA_PATCH_DEBUG
void CDataPatchDownloader::LoadPatchFromFile(const char *szFilename)
{
	const int bufferSize = 1024*1024;
	char* pBuffer = new char[bufferSize];

	FILE *pFile = fopen(szFilename, "rb");
	if(pFile)
	{
		int bytesRead = fread(pBuffer, 1, bufferSize, pFile);

		IXmlParser *pParser=GetISystem()->GetXmlUtils()->CreateXmlParser();

		m_patchCRC=CCrc32::Compute(pBuffer,bytesRead);
		m_patchXML=pParser->ParseBuffer(pBuffer,bytesRead,false);

		gEnv->pSystem->GetXmlUtils()->SetXMLPatcher(&m_patchXML);

		fclose(pFile);
	}

	delete [] pBuffer;
}
#endif //DATA_PATCH_DEBUG
