// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetworkInspector.h"
#include <CryRenderer/IRenderer.h>
#include <CrySystem/ITextModeConsole.h>
#include "NetCVars.h"

#if ENABLE_DEBUG_KIT

CNetworkInspector* CNetworkInspector::ms_pInstance = NULL;

const char* GetNameForCompareFrequentNetMessageType(int id)
{
	switch (id)
	{
	case eCFNMT_TotSize:
		return "TotSize";
	case eCFNMT_MaxSize:
		return "MaxSize";
	case eCFNMT_AverageSize:
		return "AverageSize";
	case eCFNMT_LastSize:
		return "LastSize";
	case eCFNMT_MessageCount:
		return "MessageCount";
	case eCFNMT_AverageLatency:
		return "AverageLatency";
	case eCFNMT_MaxLatency:
		return "MaxLatency";
	case eCFNMT_MinLatency:
		return "MinLatency";
	case eCFNMT_LastLatency:
		return "LastLatency";
	}
	return 0;
}

void CNetworkInspector::AddMessage(const char* description, float sizeInBytes, float queuingLatency, const char* additionalText)
{
	NetMessage tempMsg(description, sizeInBytes, queuingLatency, additionalText);

	if (!PutMessageInShowcase(tempMsg))
	{
		m_vMessages.push_back(tempMsg);
		if (m_vMessages.size() > (TICKER_END - TICKER_START) / ROW_SIZE)
			m_vMessages.erase(m_vMessages.begin());
	}
}

void CNetworkInspector::Update()
{
	m_lastDumpTime = gEnv->pTimer->GetFrameStartTime();
	if (m_vMessages.size() > 0)
	{
		if (m_lastDumpTime.GetMilliSeconds() - m_lastMessageDeleted.GetMilliSeconds() > 500)
		{
			m_vMessages.erase(m_vMessages.begin());
			m_lastMessageDeleted = m_lastDumpTime;
		}
	}
	RefreshShowcase();
	FillShowcase();
	DumpToScreen();
}

string CNetworkInspector::CreateOutputText(NetMessage& msg)
{
	bool dedicated = gEnv->IsDedicated();
	int descLength = dedicated ? 20 : 35;
	int targetLength = 30;

	string output;
	if (!dedicated)
		output.append("Message: ");
	int offset = msg.m_description.length() - descLength;
	if (msg.m_sizeInBytes == 0)  //special message (additional data, no size)
		output.append(msg.m_description.c_str());
	else if (offset >= 0)
	{
		output.append(msg.m_description.c_str(), descLength);
	}
	else
	{
		output.append(msg.m_description);
	}

	if (msg.m_sizeInBytes > 0)
		output += string().Format(" sz: %.1f", msg.m_sizeInBytes);
	if (msg.m_queuingLatency > 0)
		output += string().Format(" lat: %.1fms", msg.m_queuingLatency);
	if (msg.m_additionalText.length() > 0)
		// here additional text can be dumped to screen
		output.append(msg.m_additionalText);

	return output;
}

string CNetworkInspector::CreateOutputText(FrequentNetMessage& msg)
{
	string output = CreateOutputText(msg.m_message);

	if (msg.m_averageSize > 0)
	{
		output += string().Format("  sz av/mn/mx: %.1f %.1f %.1f", msg.m_averageSize, msg.m_iMinSize, msg.m_iMaxSize);
	}
	if (msg.m_averageQueuingLatency > 0)
	{
		output += string().Format(" lat av: %.1f", msg.m_averageQueuingLatency);
	}
	return output;
}

void CNetworkInspector::DumpToScreen()
{
	float col[] = { 0, 1, 0, 1 };
	int row = 0;
	DrawLabel(TICKER_START - 6 * ROW_SIZE, 1, 1, col, 1, "Occasional Messages", 3);
	std::vector<NetMessage>::iterator it = m_vMessages.begin();
	while (row < (TICKER_END - TICKER_START) / ROW_SIZE && it != m_vMessages.end())
	{
		string text = CreateOutputText(*it);
		DrawLabel(TICKER_START, 1.0f, row, col, 0, text.c_str(), 1);
		++row;
		++it;
	}

	float col2[] = { 1, 1, 0, 1 };
	row = 0;
	DrawLabel(SHOWCASE_START - 6 * ROW_SIZE, 1, 1, col2, 1, "Frequent Messages", 3);
	std::vector<FrequentNetMessage>::iterator fIT = m_vFreqMessages.begin();

	ITextModeConsole* pTMC = gEnv->pSystem->GetITextModeConsole();
	if (pTMC)
	{
		const char* name = GetNameForCompareFrequentNetMessageType(CNetCVars::Get().NetInspector);
		if (!name)
			name = "none";
		char buf[128];
		cry_sprintf(buf, "Sort order: %s", name);
		pTMC->PutText(0, 0, buf);
	}
	while (row < (SHOWCASE_END - SHOWCASE_START) / ROW_SIZE && fIT != m_vFreqMessages.end())
	{
		string text = CreateOutputText(*fIT);
		DrawLabel(SHOWCASE_START, 1.0f, row, col2, 0.2f, text.c_str(), 1);
		if (pTMC)
			pTMC->PutText(0, 1 + row, text.c_str());
		++row;
		++fIT;
	}
}

void CNetworkInspector::DrawLabel(float startPos, float col, int row, float* color, float glow, const char* szText, float fScale) const
{
	const float ColumnSize = COL_SIZE;
	const float RowSize = ROW_SIZE;

	char msg[256];
	cry_strcpy(msg, szText);
	if (strlen(szText) + 1 > sizeof(msg))
	{
		memcpy(msg + sizeof(msg) - 4, "...", 4);
	}

	if (glow > 0.1f)
	{
		float glowColor[] = { color[0], color[1], color[2], glow };
		gEnv->pAuxGeomRenderer->Draw2dLabel(ColumnSize * col + 1, startPos + RowSize * row + 1, fScale * 1.2f, &glowColor[0], false, "%s", msg);
	}
	gEnv->pAuxGeomRenderer->Draw2dLabel(ColumnSize * col, startPos + RowSize * row, fScale * 1.2f, color, false, "%s", msg);
}

void CNetworkInspector::RefreshShowcase()
{
	if (m_vFreqMessages.size() == 0)
		return;

	std::vector<FrequentNetMessage>::iterator frqIT;

	//kill old messages
	for (frqIT = m_vFreqMessages.begin(); frqIT != m_vFreqMessages.end(); ++frqIT)
	{
		uint32 seconds = (uint32)(m_lastDumpTime.GetSeconds() - (*frqIT).m_lastTime.GetSeconds());
		if (!seconds)
			continue;
		if (seconds > 5)
		{
			m_vFreqMessages.erase(frqIT);
			RefreshShowcase();
			return;
		}
	}
}

void CNetworkInspector::FillShowcase()
{
	std::vector<NetMessage>::iterator msgIT;
	std::vector<NetMessage>::iterator it;

	//find messages, that belong in the showcase
	for (msgIT = m_vMessages.begin(); msgIT != m_vMessages.end(); ++msgIT)
	{
		NetMessage msg = *msgIT;
		NetMessage temp = msg;
		int amount = 1;
		for (it = msgIT + 1; it != m_vMessages.end(); ++it)
		{
			temp = *it;
			if (temp.Compare(msg))
				amount++;

			if (amount > 5)
			{
				m_vFreqMessages.push_back(msg);
				break;
			}
		}
	}
	std::sort(m_vFreqMessages.begin(), m_vFreqMessages.end(), CompareFrequentNetMessage(CNetCVars::Get().NetInspector));
	while (m_vFreqMessages.size() > (SHOWCASE_END - SHOWCASE_START) * 1.5f / ROW_SIZE)
		m_vFreqMessages.erase((m_vFreqMessages.end() - 1));
}

bool CNetworkInspector::PutMessageInShowcase(NetMessage& msg)
{

	//compare messages in showcase and increment if fitting
	std::vector<FrequentNetMessage>::iterator frqIT;
	for (frqIT = m_vFreqMessages.begin(); frqIT != m_vFreqMessages.end(); ++frqIT)
	{
		if ((*frqIT).m_message.Compare(msg))
		{
			(*frqIT).Inc(msg);
			return true;
		}
	}
	//if place left - push message nevertheless
	if (m_vFreqMessages.size() < (SHOWCASE_END - SHOWCASE_START) / ROW_SIZE * 2)
	{
		m_vFreqMessages.push_back(FrequentNetMessage(msg));
	}

	return false;
}

#endif
