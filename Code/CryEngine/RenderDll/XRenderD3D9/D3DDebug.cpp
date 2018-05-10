// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "D3DDebug.h"

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)

CD3DDebug::CD3DDebug()
{
	m_pd3dDebugQueue = NULL;
	m_nNumCurrBreakOnIDs = 0;
}

CD3DDebug::~CD3DDebug()
{
	Release();
}

bool CD3DDebug::Init(ID3D11Device* pD3DDevice)
{
	Release();
	if (!pD3DDevice)
		return false;

	if (FAILED(pD3DDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&m_pd3dDebugQueue)))
		return false;

	m_pd3dDebugQueue->PushEmptyStorageFilter();
	m_pd3dDebugQueue->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, "Application D3D Debug Layer initialized");

	return true;
}

void CD3DDebug::Release()
{
	if (m_pd3dDebugQueue)
	{
		m_pd3dDebugQueue->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, "Application D3D Debug Layer deinitialized");
		m_pd3dDebugQueue->PopStorageFilter();
	}

	SAFE_RELEASE(m_pd3dDebugQueue);
}

void CD3DDebug::Update(ESeverityCombination muteSeverity, const char* strMuteMsgList, const char* strBreakOnMsgList)
{
	if (!m_pd3dDebugQueue)
		return;

	m_pd3dDebugQueue->ClearStorageFilter();

	/////////////////////////
	// Severity based mute //
	/////////////////////////
	D3D11_MESSAGE_SEVERITY severityList[4] = { D3D11_MESSAGE_SEVERITY_INFO };
	UINT nNumSeverities = 0;
	switch (muteSeverity)
	{
	case ESeverity_Info:
		severityList[0] = D3D11_MESSAGE_SEVERITY_INFO;
		nNumSeverities = 1;
		break;
	case ESeverity_InfoWarning:
		severityList[0] = D3D11_MESSAGE_SEVERITY_INFO;
		severityList[1] = D3D11_MESSAGE_SEVERITY_WARNING;
		nNumSeverities = 2;
		break;
	case ESeverity_InfoWarningError:
		severityList[0] = D3D11_MESSAGE_SEVERITY_INFO;
		severityList[1] = D3D11_MESSAGE_SEVERITY_WARNING;
		severityList[2] = D3D11_MESSAGE_SEVERITY_ERROR;
		nNumSeverities = 3;
		break;
	case ESeverity_All:
		severityList[0] = D3D11_MESSAGE_SEVERITY_INFO;
		severityList[1] = D3D11_MESSAGE_SEVERITY_WARNING;
		severityList[2] = D3D11_MESSAGE_SEVERITY_ERROR;
		severityList[3] = D3D11_MESSAGE_SEVERITY_CORRUPTION;
		nNumSeverities = 4;
		break;
	}

	///////////////////
	// ID based mute //
	///////////////////
	D3D11_MESSAGE_ID msgIDsList[MAX_NUM_DEBUG_MSG_IDS] = { D3D11_MESSAGE_ID_UNKNOWN };
	UINT nNumIDs = ParseIDs(strMuteMsgList, msgIDsList);

#if CRY_PLATFORM_DURANGO
	// Optional CopyOverlap in 11_1 is reported unsupported, but XDK's debug layer is broken (all optional features should be on, but are reported "off", even to the debug-layer)
	// Additionally, the debug layer says there is a overlap, but's clearly not the case
	msgIDsList[nNumIDs++] = D3D11_MESSAGE_ID_COPYSUBRESOURCEREGION_INVALIDSOURCE /*281*/;
#endif

	D3D11_INFO_QUEUE_FILTER filterQueue;
	ZeroStruct(filterQueue);
	filterQueue.DenyList.pSeverityList = severityList;
	filterQueue.DenyList.NumSeverities = nNumSeverities;
	filterQueue.DenyList.pIDList = msgIDsList;
	filterQueue.DenyList.NumIDs = nNumIDs;
	m_pd3dDebugQueue->AddStorageFilterEntries(&filterQueue);

	////////////////////////////
	// Break on functionality //
	////////////////////////////
	// First disable break for old entries
	for (UINT i = 0; i < m_nNumCurrBreakOnIDs; ++i)
		m_pd3dDebugQueue->SetBreakOnID(m_arrBreakOnIDsList[i], FALSE);
	m_nNumCurrBreakOnIDs = 0;

	const int nVal = atoi(strBreakOnMsgList);
	if (nVal == -1)
	{
		// Break on all errors
		m_pd3dDebugQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
		m_pd3dDebugQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	}
	else
	{
		// Break on specified messages
		m_pd3dDebugQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
		m_pd3dDebugQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
		m_nNumCurrBreakOnIDs = ParseIDs(strBreakOnMsgList, m_arrBreakOnIDsList);
		for (UINT i = 0; i < m_nNumCurrBreakOnIDs; ++i)
		{
			m_pd3dDebugQueue->SetBreakOnID(m_arrBreakOnIDsList[i], TRUE);
		}
	}
}

UINT CD3DDebug::ParseIDs(const char* strMsgIDList, D3D11_MESSAGE_ID arrMsgList[MAX_NUM_DEBUG_MSG_IDS]) const
{
	const size_t nLen = strlen(strMsgIDList);
	UINT nIDs = 0;
	const char* pStart = strMsgIDList;
	char* pEnd;
	UINT nValID = 1;
	while (nValID != 0 && nIDs < MAX_NUM_DEBUG_MSG_IDS)
	{
		nValID = UINT(strtol(pStart, &pEnd, 10));
		if (nValID)
		{
			arrMsgList[nIDs] = D3D11_MESSAGE_ID(nValID);
			pStart = pEnd;
			nIDs++;
		}
	}

	return nIDs;
}

string CD3DDebug::GetLastMessage()
{
	string res;

	ID3D11InfoQueue* pQueue = GetDebugInfoQueue();
	if (pQueue)
	{
		const UINT64 numMsg = pQueue->GetNumStoredMessages();
		if (numMsg)
		{
			const UINT64 lastMsg = numMsg - 1;
			SIZE_T msgLen = 0;
			if (SUCCEEDED(pQueue->GetMessage(lastMsg, 0, &msgLen)))
			{
				D3D11_MESSAGE* pMsg = (D3D11_MESSAGE*) malloc(msgLen);
				if (SUCCEEDED(pQueue->GetMessage(lastMsg, pMsg, &msgLen)))
				{
					const char* pFmt = 0;
					switch (pMsg->Severity)
					{
					case D3D11_MESSAGE_SEVERITY_CORRUPTION:
						pFmt = "D3D11 Corruption #%d: ";
						break;
					case D3D11_MESSAGE_SEVERITY_ERROR:
						pFmt = "D3D11 Error #%d: ";
						break;
					case D3D11_MESSAGE_SEVERITY_WARNING:
						pFmt = "D3D11 Warning #%d: ";
						break;
					case D3D11_MESSAGE_SEVERITY_INFO:
						pFmt = "D3D11 Info #%d: ";
						break;
					}

					char buf[32];
					cry_sprintf(buf, pFmt, pMsg->ID);

					res += buf;
					res += pMsg->pDescription;
				}
				free(pMsg);
			}
		}
	}

	if (res.empty())
		res = string("No message queued. Debug runtime might be inactive or not installed.");

	return res;
}

#endif //!CAFE && !CRY_USE_OPENGL
