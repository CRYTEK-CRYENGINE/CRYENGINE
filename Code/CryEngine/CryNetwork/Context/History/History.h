// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HISTORY_H__
#define __HISTORY_H__

#pragma once

#include "RegularlySyncedItem.h"
#include "../SyncContext.h"

enum EHistorySendResult
{
	eHSR_Ok,
	eHSR_NotNeeded,
	eHSR_Failed,
};

enum EHistoricalEvent
{
	eHE_BindAspect,
	eHE_UnbindAspect,
	eHE_Pollute,
	eHE_Reset
};

struct SHistoricalEvent
{
	SHistoricalEvent(EHistoricalEvent he) : event(he), seq(~uint32(0)), index(0xcc), pView(0), pViewObj(0), pViewObjEx(0) {}
	EHistoricalEvent      event;
	uint32                itemId;
	SNetObjectID          objId;
	NetworkAspectID       index;
	uint32                seq;
	CContextView*         pView;
	SContextViewObject*   pViewObj;
	SContextViewObjectEx* pViewObjEx;
};

class CHistory : protected CRegularlySyncedItems
{
public:
	NetworkAspectType indexMask;

	CHistory(uint32 idBase, uint32 idMul) : CRegularlySyncedItems(idBase, idMul)
	{
		++g_objcnt.history;
	}
	virtual ~CHistory()
	{
		--g_objcnt.history;
	}

	void         Ack(const SSyncContext& ctx, bool ack);
	virtual void Flush(const SSyncContext& ctx);
	virtual bool ReadCurrentValue(const SReceiveContext& ctx, bool commit) = 0;
	SElem*       FindPrevElem(const SSyncContext& ctx);
	void         Reset();

	virtual void HandleEvent(const SHistoricalEvent& event) = 0;

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CHistory");

		pSizer->Add(*this);
		CRegularlySyncedItems::GetMemoryStatistics(pSizer);
	}

	class CSyncContext
	{
	public:
		CSyncContext() : m_pHistory(0), m_pElem(0) {}
		// MUST call one and only one of NeedToSync and PrepareToSync
		bool               NeedToSync(CHistory* pHistory, const SSyncContext& ctx);    // return true if we have new data
		bool               PrepareToSync(CHistory* pHistory, const SSyncContext& ctx); // return true if we could sync even if we wanted to
		EHistorySendResult Send(const SSendContext& ctx);

		SElem*             GetPrevElem(const SSyncContext& ctx)
		{
			uint32 curResizeCount = m_item.GetResizeCount();
			if (m_resizesWhenElemTaken != curResizeCount)
			{
				m_pElem = m_item.FindPrevElem(ctx.basisSeq, ctx.currentSeq);
				m_resizesWhenElemTaken = curResizeCount;
			}
			return m_pElem;
		}

	private:
		CHistory*            m_pHistory;
		CRegularlySyncedItem m_item;
		SElem*               m_pElem;
		uint32               m_resizesWhenElemTaken;
	};

private:
	virtual bool NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item) = 0;
	virtual bool CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item) = 0;
	virtual bool SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue) = 0;
};

#endif
