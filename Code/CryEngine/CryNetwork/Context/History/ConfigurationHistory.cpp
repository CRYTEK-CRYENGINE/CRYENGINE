// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConfigurationHistory.h"
#include "Context/ContextView.h"

CConfigurationHistory::CConfigurationHistory(CContextView* pView) : CHistory(eHSS_Configuration, 0)
{
	indexMask = (pView->GetConfig().pReconfigureMsg != 0) * 0x80;
}

bool CConfigurationHistory::ReadCurrentValue(const SReceiveContext& ctx, bool commit)
{
	NetworkAspectType aspects;
	ctx.ser.Value("aspects", aspects);
	if (commit)
		ctx.pView->ContextState()->ReconfigureObject(ctx.objId, aspects);
	return true;
}

void CConfigurationHistory::HandleEvent(const SHistoricalEvent& event)
{
}

bool CConfigurationHistory::CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	return true;
}

bool CConfigurationHistory::NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	switch (item.GetHistoryCount())
	{
	case eHC_Zero:
		return true;
	case eHC_One:
		if (SElem* pElem = pSyncContext->GetPrevElem(ctx))
			return pElem->data != ctx.ctxObj.xtra->nAspectsEnabled;
		else
			return true;
	case eHC_MoreThanOne:
		CRegularlySyncedItem::ConstValueIter iter = item.GetConstIter();
		uint32 oldestSeq = 0;
		while (const SElem* pOther = iter.Current())
		{
			if (pOther->data != ctx.ctxObj.xtra->nAspectsEnabled)
				return true;
			oldestSeq = pOther->seq;
			iter.Next();
		}
		return oldestSeq > ctx.basisSeq;
	}
	return false;
}

bool CConfigurationHistory::SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue)
{
	ctx.pSender->BeginMessage(ctx.pView->GetConfig().pReconfigureMsg);
	NetworkAspectType aspects = ctx.ctxObj.xtra->nAspectsEnabled;
	ctx.pSender->ser.Value("aspects", aspects);
	newValue = aspects;
	return true;
}
