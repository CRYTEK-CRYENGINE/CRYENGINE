// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DownloadManager.h"
#include "HTTPDownloader.h"

#if CRY_PLATFORM_WINDOWS

//-------------------------------------------------------------------------------------------------
CDownloadManager::CDownloadManager()
	: m_pSystem(0)
{
}

//-------------------------------------------------------------------------------------------------
CDownloadManager::~CDownloadManager()
{
}

//-------------------------------------------------------------------------------------------------
void CDownloadManager::Create(ISystem* pSystem)
{
	m_pSystem = pSystem;
}

//-------------------------------------------------------------------------------------------------
CHTTPDownloader* CDownloadManager::CreateDownload()
{
	CHTTPDownloader* pDL = new CHTTPDownloader;

	m_lDownloadList.push_back(pDL);

	pDL->Create(m_pSystem, this);

	return pDL;
}

//-------------------------------------------------------------------------------------------------
void CDownloadManager::RemoveDownload(CHTTPDownloader* pDownload)
{
	std::list<CHTTPDownloader*>::iterator it = std::find(m_lDownloadList.begin(), m_lDownloadList.end(), pDownload);

	if (it != m_lDownloadList.end())
	{
		m_lDownloadList.erase(it);
	}
}

//-------------------------------------------------------------------------------------------------
void CDownloadManager::Update()
{
	std::list<CHTTPDownloader*>::iterator it = m_lDownloadList.begin();

	while (it != m_lDownloadList.end())
	{
		CHTTPDownloader* pDL = *it;

		switch (pDL->GetState())
		{
		case HTTP_STATE_NONE:
		case HTTP_STATE_WORKING:
			++it;
			continue;
		case HTTP_STATE_COMPLETE:
			pDL->OnComplete();
			break;
		case HTTP_STATE_ERROR:
			pDL->OnError();
			break;
		case HTTP_STATE_CANCELED:
			pDL->OnCancel();
			break;
		}

		it = m_lDownloadList.erase(it);

		pDL->Release();
	}
}

//-------------------------------------------------------------------------------------------------
void CDownloadManager::Release()
{
	for (std::list<CHTTPDownloader*>::iterator it = m_lDownloadList.begin(); it != m_lDownloadList.end(); )
	{
		CHTTPDownloader* pDL = *it;

		it = m_lDownloadList.erase(it);

		pDL->Release();
	}

	delete this;
}

#endif
