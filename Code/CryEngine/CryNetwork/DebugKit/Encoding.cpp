// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Encoding.h"

#if ENABLE_DEBUG_KIT

	#include "Context/SessionID.h"
	#include "Network.h"

CEncoding::CEncoding(CDebugOutput* pDebugOutput)
	: m_isInUse(false), m_pOutput(pDebugOutput), m_enabled(false), m_shouldEnable(false), m_isEncoding(false)
{
}

CEncoding::~CEncoding()
{
	NET_ASSERT(!m_isInUse);
}

void CEncoding::BeginPacket(TNetChannelID channel, bool isEncoding, uint32 uniqueId)
{
	if (m_enabled)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
		NET_ASSERT(!m_isInUse);

		m_uniqueId = uniqueId;
		m_isEncoding = isEncoding;
		m_channel = channel;
		m_coding.resize(0);
		m_annotation.resize(0);
	}
	m_isInUse = true;
}

void CEncoding::Annotation(const string& annotation)
{
	if (m_enabled)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
		NET_ASSERT(m_isInUse);

		m_annotation.push_back(SAnnotation());
		m_annotation.back().index = m_coding.size();
		m_annotation.back().annotation = annotation;
	}
}

void CEncoding::Coding(uint64 nTot, uint32 nLow, uint32 nSym)
{
	if (m_enabled)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
		if (!m_isInUse)
			return;

		SCoding v = { nTot, nLow, nSym };
		m_coding.push_back(v);
	}
}

void CEncoding::EndPacket(bool bSent)
{
	if (m_enabled)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
		NET_ASSERT(m_isInUse);

		if (bSent && m_channel)
		{
			m_pOutput->Lock();

			m_pOutput->Write('e');

			m_pOutput->Write(m_uniqueId);
			m_pOutput->Write(m_isEncoding);
			m_pOutput->Write(m_channel);

			m_pOutput->Write((uint32)m_coding.size());
			for (size_t i = 0; i < m_coding.size(); i++)
			{
				m_pOutput->Write(m_coding[i].nTot);
				m_pOutput->Write(m_coding[i].nLow);
				m_pOutput->Write(m_coding[i].nSym);
			}
			m_pOutput->Write((uint32)m_annotation.size());
			for (size_t i = 0; i < m_annotation.size(); i++)
			{
				m_pOutput->Write((uint32)m_annotation[i].index);
				m_pOutput->Write(m_annotation[i].annotation);
			}

			m_pOutput->Unlock();
		}
	}

	m_isInUse = false;
	m_enabled = m_shouldEnable;
}

void CEncoding::SetSessionID(const CSessionID& session)
{
	m_sessionID = session;
}

void CEncoding::SetEnabled(bool enable)
{
	if (m_isInUse)
		m_shouldEnable = enable;
	else
		m_shouldEnable = m_enabled = enable;
}

#endif
