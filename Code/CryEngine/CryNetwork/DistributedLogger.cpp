// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Config.h"

#if ENABLE_DISTRIBUTED_LOGGER
	#include "DistributedLogger.h"
	#include "Network.h"

static const float POLL_INTERVAL_FOUND_ONE = 59;
static const float POLL_INTERVAL_FOUND_NONE = 9;
static const float POLL_TIME = 1;

static TNetAddress addr(const char* s)
{
	TNetAddressVec temp;
	CNameRequestPtr p = RESOLVER.RequestNameLookup(s);
	p->Wait();
	p->GetResult(temp);
	return temp.empty() ? TNetAddress() : temp[0];
}

CDistributedLogger::CDistributedLogger() : m_pSocket(OpenSocket(addr("0.0.0.0:0"), eSF_BroadcastSend)), m_inFlush(false)
{
	InitBuffer();
	m_pSocket->SetListener(this);
	gEnv->pLog->AddCallback(this);
}

CDistributedLogger::~CDistributedLogger()
{
	m_pSocket->SetListener(0);
	Flush();
	gEnv->pLog->RemoveCallback(this);
}

void CDistributedLogger::OnWriteToConsole(const char* sText, bool bNewLine)
{
}

void CDistributedLogger::OnWriteToFile(const char* sText, bool bNewLine)
{
	if (m_inFlush)
		return;

	size_t len = strlen(sText);
	if (len > MAX_PKT_SIZE - 5)
		return;

	if (m_bufferPos + len > MAX_PKT_SIZE)
		Flush();

	memcpy(m_buffer + m_bufferPos, sText, len);
	m_bufferPos += len;
}

void CDistributedLogger::Update(CTimeValue now)
{
	if (!m_pSocket)
		return;

	if (m_inPoll)
		Update_InPoll(now);
	else
		Update_BetweenPoll(now);
}

void CDistributedLogger::Update_BetweenPoll(CTimeValue now)
{
	float pollInterval = m_listeners.empty() ? POLL_INTERVAL_FOUND_NONE : POLL_INTERVAL_FOUND_ONE;
	if (now - m_lastPoll > pollInterval)
	{
		for (std::map<TNetAddress, bool>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
			iter->second = false;

		char* pkt = "?";
		m_pSocket->Send((uint8*)pkt, strlen(pkt), addr("255.255.255.255:61705"));
		m_inPoll = true;
		m_lastPoll = now;
	}
}

void CDistributedLogger::OnPacket(const TNetAddress& addr, TMemHdl hdl)
{
	// if anyone can explain why i would get a response from 0.0.0.0:0...
	// i would love to be enlightened... craig
	if (const SIPv4Addr* pAddr = addr.GetPtr<SIPv4Addr>())
		if (!pAddr->addr || !pAddr->port)
			return;

	m_listeners[addr] = true;
}

void CDistributedLogger::Update_InPoll(CTimeValue now)
{
	if (now - m_lastPoll > POLL_TIME)
	{
		std::vector<std::map<TNetAddress, bool>::iterator> deadListeners;
		for (std::map<TNetAddress, bool>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
			if (!iter->second)
				deadListeners.push_back(iter);
		while (!deadListeners.empty())
		{
			m_listeners.erase(deadListeners.back());
			deadListeners.pop_back();
		}

		m_lastPoll = now;
		m_inPoll = false;
	}
}

void CDistributedLogger::InitBuffer()
{
	m_buffer[0] = uint8('*');
	m_bufferPos = 1;
}

void CDistributedLogger::Flush()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	if (!m_pSocket)
		return;
	NET_ASSERT(!m_inFlush);
	m_inFlush = true;
	for (std::map<TNetAddress, bool>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		m_pSocket->Send((uint8*)m_buffer, m_bufferPos, iter->first);
	InitBuffer();
	m_inFlush = false;
}

#endif
