// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   NetworkInspector.h
//  Version:     v1.00
//  Created:     19/12/2005 by Jan Müller
//  Compilers:   Visual Studio.NET 2003
//  Description: The NetworkInspector gets messages from the network.
//							Once per frame the messages are automatically sorted and
//							rendered on the screen including their size and target address.
//							This class is a singleton and belongs to CNetwork.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef NETWORK_INSPECTOR_H
#define NETWORK_INSPECTOR_H

#pragma once

#include "Config.h"

#if ENABLE_DEBUG_KIT

//where should the output be written ..?
const static float SHOWCASE_START = 70.0f;
const static float SHOWCASE_END = 350.0f;
const static float TICKER_START = 400.0f;
const static float TICKER_END = 550.0f;

const static float ROW_SIZE = 9.5f;
const static float COL_SIZE = 5.0f;

//this is a basic message
struct NetMessage
{
	string m_description;
	string m_additionalText;
	float  m_sizeInBytes;
	float  m_queuingLatency;

	ILINE NetMessage(const char* description, float sizeInBytes, float queuingLatency, const char* additionalText) :
		m_description(description), m_sizeInBytes(sizeInBytes), m_queuingLatency(queuingLatency)
	{
		if (additionalText != NULL)
			m_additionalText = string(additionalText);
	}

	bool Compare(NetMessage& msg)
	{
		if (msg.m_description.compare(m_description) == 0)
			return true;
		return false;
	}
};
//this is a frequently occurring message
struct FrequentNetMessage
{
	NetMessage m_message;
	float      m_iMinSize, m_iMaxSize, m_iMessageCount;
	CTimeValue m_lastTime;
	float      m_averageSize;
	float      m_lastMsgSize;
	float      m_totalQueuingLatency;
	float      m_averageQueuingLatency;
	float      m_maxQueuingLatency, m_minQueuingLatency;

	FrequentNetMessage(NetMessage& msg) : m_message(msg)
	{
		m_iMinSize = m_message.m_sizeInBytes;
		m_iMaxSize = m_message.m_sizeInBytes;
		m_iMessageCount = 1;
		m_averageSize = m_message.m_sizeInBytes;
		m_totalQueuingLatency = m_message.m_queuingLatency;
		m_minQueuingLatency = m_maxQueuingLatency = m_averageQueuingLatency = m_message.m_queuingLatency;
		m_lastTime = gEnv->pTimer->GetFrameStartTime();
		m_lastMsgSize = m_message.m_sizeInBytes;
	}

	void Inc(NetMessage& msg)
	{
		if (m_message.m_sizeInBytes > (uint32(1) << 31))
		{
			m_message.m_sizeInBytes = msg.m_sizeInBytes;
			m_iMessageCount = 1;
			return;
		}
		m_iMessageCount++;
		m_lastMsgSize = msg.m_sizeInBytes;
		m_message.m_sizeInBytes += m_lastMsgSize;
		m_message.m_queuingLatency = msg.m_queuingLatency;
		if (msg.m_queuingLatency >= 0)
			m_totalQueuingLatency += msg.m_queuingLatency;
		m_averageQueuingLatency = m_totalQueuingLatency / m_iMessageCount;
		m_maxQueuingLatency = max(m_maxQueuingLatency, msg.m_queuingLatency);
		m_minQueuingLatency = min(m_minQueuingLatency, msg.m_queuingLatency);
		if (m_message.m_sizeInBytes == 0)
			m_message.m_additionalText = msg.m_additionalText;
		m_averageSize = m_message.m_sizeInBytes / m_iMessageCount;
		if (m_iMinSize > msg.m_sizeInBytes)
			m_iMinSize = msg.m_sizeInBytes;
		else if (m_iMaxSize < msg.m_sizeInBytes)
			m_iMaxSize = msg.m_sizeInBytes;
		m_lastTime = gEnv->pTimer->GetFrameStartTime();
	}
};

enum ECompareFrequentNetMessageType
{
	eCFNMT_TotSize        = 10,
	eCFNMT_MaxSize        = 11,
	eCFNMT_AverageSize    = 12,
	eCFNMT_LastSize       = 13,
	eCFNMT_MessageCount   = 20,
	//eCFNMT_MessageFreq = 21,
	eCFNMT_MinLatency     = 30,
	eCFNMT_MaxLatency     = 31,
	eCFNMT_AverageLatency = 32,
	eCFNMT_LastLatency    = 33
};

const char* GetNameForCompareFrequentNetMessageType(int id);

struct CompareFrequentNetMessage
{
	int key;

	CompareFrequentNetMessage(int key) { this->key = key; }

	bool operator()(const FrequentNetMessage& a, const FrequentNetMessage& b) const
	{
		switch (key)
		{
		case eCFNMT_TotSize:
			return a.m_message.m_sizeInBytes > b.m_message.m_sizeInBytes;
		case eCFNMT_MaxSize:
			return a.m_iMaxSize > b.m_iMaxSize;
		case eCFNMT_AverageSize:
			return a.m_averageSize > b.m_averageSize;
		case eCFNMT_LastSize:
			return a.m_lastMsgSize > b.m_lastMsgSize;
		case eCFNMT_MessageCount:
			return a.m_iMessageCount > b.m_iMessageCount;
		case eCFNMT_AverageLatency:
			return a.m_averageQueuingLatency > b.m_averageQueuingLatency;
		case eCFNMT_MaxLatency:
			return a.m_maxQueuingLatency > b.m_maxQueuingLatency;
		case eCFNMT_MinLatency:
			return a.m_minQueuingLatency < b.m_minQueuingLatency;
		case eCFNMT_LastLatency:
			return a.m_message.m_queuingLatency > b.m_message.m_queuingLatency;
		}
		return false;
	}
};

class CNetworkInspector
{

public:
	//adds a message to the queue - if the message should contain special data, define a unique description, set sizeInBytes to 0 (!) and put the data in additionalText
	void AddMessage(const char* description, float sizeInBytes = 0, float queuingLatency = -1, const char* additionalText = NULL);

	//updates and renders the inspector
	void Update();

	//this is a singleton!
private:
	static CNetworkInspector* ms_pInstance;
public:
	static ILINE CNetworkInspector* CreateInstance()
	{
		if (NULL == ms_pInstance)
			ms_pInstance = new CNetworkInspector;
		return ms_pInstance;
	}
	static void ReleaseInstance()
	{
		NET_ASSERT(ms_pInstance != 0);
		if (ms_pInstance != 0)
		{
			ms_pInstance->Release();
			ms_pInstance = 0;
		}
	}

private:

	CNetworkInspector()
	{
		m_lastDumpTime = gEnv->pTimer->GetFrameStartTime();
	}

	~CNetworkInspector() {}

	void   Release() { delete this; }
	//writes text lines on screen
	void   DrawLabel(float startPos, float col, int row, float* color, float glow, const char* szText, float fScale) const;
	//creates a text line from a message
	string CreateOutputText(NetMessage& msg);

	string CreateOutputText(FrequentNetMessage& msg);
	//deletes old (not so frequent) messages
	void   RefreshShowcase();
	//finds frequent messages and sorts them by size
	void   FillShowcase();
	//checks, whether a message is already in the showcase
	bool   PutMessageInShowcase(NetMessage& msg);
	//renders all messages on screen
	void   DumpToScreen();

	std::vector<NetMessage>         m_vMessages;
	std::vector<FrequentNetMessage> m_vFreqMessages;

	CTimeValue                      m_lastDumpTime;
	CTimeValue                      m_lastMessageDeleted;

};

#endif

#endif
