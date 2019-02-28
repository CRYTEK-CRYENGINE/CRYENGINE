// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "History.h"
#include "NetCVars.h"

bool CHistory::CSyncContext::NeedToSync(CHistory* pHistory, const SSyncContext& ctx)
{
	return PrepareToSync(pHistory, ctx) && m_pHistory->NeedToSync(ctx, this, m_item);
}

bool CHistory::CSyncContext::PrepareToSync(CHistory* pHistory, const SSyncContext& ctx)
{
	NET_ASSERT(!m_pHistory);
	m_pHistory = pHistory;
	m_pElem = 0;

	m_item = m_pHistory->GetItem(ctx.index, ctx.pViewObjEx->historyElems);
	m_pElem = 0;
	m_resizesWhenElemTaken = m_item.GetResizeCount() - 1; // force the fetch since we haven't actually searched for the elem yet

	return m_pHistory->CanSync(ctx, this, m_item);
}

void CHistory::Flush(const SSyncContext& ctx)
{
#if LOG_INCOMING_MESSAGES | LOG_OUTGOING_MESSAGES
	if (CNetCVars::Get().LogNetMessages & 32)
		NetLog("Flush");
#endif
	CRegularlySyncedItem item(GetItem(ctx.index, ctx.pViewObjEx->historyElems));
	item.Flush();
}

EHistorySendResult CHistory::CSyncContext::Send(const SSendContext& ctx)
{
	NET_ASSERT(m_pHistory);
	if (SElem* pElem = GetPrevElem(ctx))
		m_item.RemoveOlderThan(m_pElem->seq);
	uint32 newValue;
	if (m_pHistory->SendCurrentValue(ctx, this, newValue))
	{
		m_item.AddElem(ctx.currentSeq, newValue);
		return eHSR_Ok;
	}
	else
	{
		return eHSR_Failed;
	}
}

void CHistory::Ack(const SSyncContext& ctx, bool ack)
{
	CRegularlySyncedItem item(GetItem(ctx.index, ctx.pViewObjEx->historyElems));
	item.AckUpdate(ctx.currentSeq, ack);
}

CHistory::SElem* CHistory::FindPrevElem(const SSyncContext& ctx)
{
	CRegularlySyncedItem item(GetItem(ctx.index, ctx.pViewObjEx->historyElems));
	return item.FindPrevElem(ctx.basisSeq, ctx.currentSeq);
}

void CHistory::Reset()
{
	CRegularlySyncedItems::Reset();
	SHistoricalEvent event(eHE_Reset);
	HandleEvent(event);
}
