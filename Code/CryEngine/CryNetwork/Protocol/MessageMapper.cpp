// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  maps messages to their definitions
   -------------------------------------------------------------------------
   History:
   - 08/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "MessageMapper.h"

CMessageMapper::CMessageMapper()
{
}

void CMessageMapper::Reset(size_t nReservedMessages,
                           const SNetProtocolDef* pProtocols,
                           size_t nProtocols)
{
	SSinkMessages msgs;
	msgs.nEnd = nReservedMessages;

	for (size_t i = 0; i < nProtocols; i++)
	{
		msgs.nStart = msgs.nEnd;
		msgs.nEnd = msgs.nStart + pProtocols[i].nMessages;
		msgs.pFirst = pProtocols[i].vMessages;
		msgs.pLast = pProtocols[i].vMessages + pProtocols[i].nMessages;
		m_messages.push_back(msgs);

		for (size_t j = 0; j < pProtocols[i].nMessages; j++)
			m_nameToDef[pProtocols[i].vMessages[j].description] = &pProtocols[i].vMessages[j];
	}

	// Build the ID and definition mapping.
	if (m_messages.empty())
		return;
	const int nMessageIDs = GetNumberOfMsgIds();
	uint32 i = 0;
	m_IdMap.clear();
	m_DefMap.resize(0);
	m_DefMap.reserve(nMessageIDs);
	m_DefMapBias = m_messages[0].nStart;
	for (TMsgVec::const_iterator it = m_messages.begin(), itEnd = m_messages.end();
	     it != itEnd; ++it)
	{
		for (const SNetMessageDef* pDef = it->pFirst; pDef != it->pLast; ++pDef)
			m_DefMap.push_back(pDef);
	}
	std::sort(m_DefMap.begin(), m_DefMap.end(), SNetMessageDef::ComparePointer());
	i = 0;
	for (TDefMap::const_iterator it = m_DefMap.begin(), itEnd = m_DefMap.end();
	     it != itEnd; ++it)
	{
		m_IdMap[*it] = i++ + m_DefMapBias;
	}

	/*
	   NetLog("----------------- MESSAGE MAP");
	   for (TDefMap::iterator it = m_DefMap.begin(); it != m_DefMap.end(); ++it)
	   {
	    NetLog("%s %d", (*it)->description, it-m_DefMap.begin());
	   }
	 */
}

uint32 CMessageMapper::GetMsgId(const SNetMessageDef* def) const
{
	NET_ASSERT(def);
#if 0
	for (TMsgVec::iterator i = m_messages.begin(); i != m_messages.end(); ++i)
	{
		if (i->pFirst <= def && def < i->pLast)
			return i->nStart + def - i->pFirst;
	}
	CryFatalError("Message def %s (%p) does not belong to this protocol", def->description, def);
#else
	TIdMap::const_iterator it = m_IdMap.find(def);
	if (it == m_IdMap.end())
	{
		CryFatalError("Message def %s (%p) does not belong to this protocol", def->description, def);
		return 0;
	}
	return it->second;
#endif
}

const SNetMessageDef* CMessageMapper::GetDispatchInfo(uint32 id, size_t* pnWhichProtocol)
{
	NET_ASSERT(!m_messages.empty() && "CMessageMapper not initialized");
	NET_ASSERT(id >= m_messages[0].nStart && "Trying to get a def for a reserved message");
#if 0
	for (TMsgVec::iterator i = m_messages.begin(); i != m_messages.end(); ++i)
	{
		if (id < i->nEnd)
		{
			if (pnWhichProtocol)
				*pnWhichProtocol = i - m_messages.begin();
			return i->pFirst + id - i->nStart;
		}
	}
	CryFatalError("No such message id %d", id);
#else
	NET_ASSERT(id >= m_DefMapBias);
	if (id - m_DefMapBias > m_DefMap.size())
	{
		NetLog("No such message id %d", id);
		return NULL;
	}
	const SNetMessageDef* const pDef = m_DefMap[id - m_DefMapBias];
	if (pnWhichProtocol != NULL)
	{
		for (TMsgVec::const_iterator it = m_messages.begin(), itEnd = m_messages.end();
		     it != itEnd; ++it)
		{
			const SSinkMessages& pSinkMsg = *it;
			if (pDef >= pSinkMsg.pFirst && pDef < pSinkMsg.pLast)
			{
				*pnWhichProtocol = it - m_messages.begin();
				break;
			}
		}
	}
	return pDef;
#endif
}

uint32 CMessageMapper::GetNumberOfMsgIds() const
{
	return m_messages.empty() ?
	       1 :
	       (uint32)(m_messages.back().nStart + m_messages.back().pLast - m_messages.back().pFirst);
}

bool CMessageMapper::LookupMessage(const char* name, SNetMessageDef const** pDef, size_t* pnSubProtocol)
{
	*pDef = stl::find_in_map(m_nameToDef, name, NULL);
	if (*pDef)
	{
		for (TMsgVec::iterator i = m_messages.begin(); i != m_messages.end(); ++i)
		{
			if (i->pFirst <= *pDef && i->pLast > *pDef)
			{
				*pnSubProtocol = i - m_messages.begin();
				return true;
			}
		}
		NET_ASSERT(!"Should never get here!");
	}
	return false;
}
