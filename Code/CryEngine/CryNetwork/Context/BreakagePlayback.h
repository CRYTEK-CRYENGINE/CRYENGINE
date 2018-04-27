// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BREAKAGEPLAYBACK_H__
#define __BREAKAGEPLAYBACK_H__

#pragma once

#include "ClientContextView.h"

class CBreakagePlayback : public INetBreakagePlayback
{
public:
	CBreakagePlayback(CClientContextView* pView, SNetClientBreakDescriptionPtr pBreakOps);
	~CBreakagePlayback();

	virtual void     SpawnedEntity(int idx, EntityId id);
	virtual EntityId GetEntityIdForIndex(int idx);

private:
	_smart_ptr<CClientContextView> m_pView;
	SNetClientBreakDescriptionPtr  m_pBreakOps;
	DynArray<EntityId>             m_gotBreakOps;

	size_t                         m_ofs;
};

#endif
