// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BreakagePlayback.h"
#include "ServerContextView.h"
#include "NetCVars.h"

CBreakagePlayback::CBreakagePlayback(CClientContextView* pView, SNetClientBreakDescriptionPtr pBreakOps)
	: m_pView(pView)
	, m_pBreakOps(pBreakOps)
	, m_ofs(0)
{
	++g_objcnt.breakagePlayback;
}

CBreakagePlayback::~CBreakagePlayback()
{
	SCOPED_GLOBAL_LOCK;
	--g_objcnt.breakagePlayback;
	for (int i = 0; i < m_pBreakOps->size(); i++)
	{
		if ((*m_pBreakOps)[i])
		{
			NetWarning("[brk] uncollected item idx=%d netid=%s", i, (*m_pBreakOps)[i].GetText());
			if (m_pView->Parent())
				m_pView->Parent()->NetAddSendable(new CSimpleNetMessage<SSimpleObjectIDParams>(SSimpleObjectIDParams((*m_pBreakOps)[i]), CServerContextView::SkippedCollectedObject), 0, 0, 0);
		}
	}
}

void CBreakagePlayback::SpawnedEntity(int idx, EntityId id)
{
	SCOPED_GLOBAL_LOCK;
	if (idx >= m_pBreakOps->size() || idx < 0 || !(*m_pBreakOps)[idx])
		return;
	if (m_gotBreakOps.size() <= idx)
		m_gotBreakOps.resize(idx + 1, 0);
	m_pView->BoundCollectedObject((*m_pBreakOps)[idx], id);
	NetLog("[brk] collected index %d as %d", idx, id);
	(*m_pBreakOps)[idx] = SNetObjectID();
	m_gotBreakOps[idx] = id;
}

EntityId CBreakagePlayback::GetEntityIdForIndex(int idx)
{
	if (idx < 0 || idx >= m_gotBreakOps.size())
		return 0;
	return m_gotBreakOps[idx];
}
