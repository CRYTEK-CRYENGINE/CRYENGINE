// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RcLoader.h"
#include "CallRcTask.h"

CRcInOrderCaller::CRcInOrderCaller()
	: m_nextTimestamp(0)
	, m_filePath()
	, m_pUserData(nullptr)
{}

CRcInOrderCaller::~CRcInOrderCaller()
{}

void CRcInOrderCaller::SetAssignCallback(const Callback& callback)
{
	m_onAssign = callback;
}

void CRcInOrderCaller::SetDiscardCallback(const Callback& callback)
{
	m_onDiscard = callback;
}

QString CRcInOrderCaller::GetFilePath() const
{
	return m_filePath;
}

void* CRcInOrderCaller::GetUserData() const
{
	return m_pUserData;
}

void CRcInOrderCaller::CallRc(const QString& filePath, void* pUserData, ITaskHost* pTaskHost, const QString& options, const QString& message)
{
	std::unique_ptr<SPayload> pPayload(new SPayload());
	pPayload->m_filePath = filePath;
	pPayload->m_timestamp = m_nextTimestamp++;
	pPayload->m_pUserData = pUserData;

	auto callback = [this](CCallRcTask* pTask)
	{
		std::unique_ptr<SPayload> pPayload((SPayload*)pTask->GetUserData());
		if (pPayload->m_timestamp + 1 != m_nextTimestamp && m_onDiscard)
		{
			m_onDiscard(this, pPayload->m_filePath, pPayload->m_pUserData);

			return;
		}

		if (pTask->Succeeded() && m_onAssign)
		{
			m_onAssign(this, pPayload->m_filePath, pPayload->m_pUserData);

			m_filePath = pPayload->m_filePath;
			m_pUserData = pPayload->m_pUserData;
			m_timestamp = m_nextTimestamp;
		}
		else if (m_onDiscard)
		{
			m_onDiscard(this, pPayload->m_filePath, pPayload->m_pUserData);
		}
	};

	CCallRcTask::Launch(filePath, pPayload.release(), callback, pTaskHost, options, message);
}

uint64 CRcInOrderCaller::GetNextTimestamp() const
{
	return m_nextTimestamp;
}

uint64 CRcInOrderCaller::GetTimpestamp() const
{
	return m_timestamp;
}

