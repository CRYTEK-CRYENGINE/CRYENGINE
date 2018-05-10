// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProfileHistory.h"
#include "../NetContext.h"
#include "../ContextView.h"

CProfileHistory::CProfileHistory(CContextView* pView) : CHistory(eHSS_Profile, 1)
{
	if (pView->IsLocal() || pView->IsClient())
		indexMask = 0;
	else
		indexMask = pView->Context()->ServerManagedProfileAspects();
}

bool CProfileHistory::ReadCurrentValue(const SReceiveContext& ctx, bool commit)
{
	uint8 profile;
	ctx.ser.Value("profile", profile);
	if (commit)
		ctx.pView->SetAspectProfile(ctx.objId, ctx.index, profile);
	return true;
}

void CProfileHistory::HandleEvent(const SHistoricalEvent& event)
{
	switch (event.event)
	{
	case eHE_UnbindAspect:
		{
			SSyncContext ctx;
			ctx.pViewObjEx = event.pViewObjEx;
			ctx.index = event.index;
			Flush(ctx);
		}
		break;
	}
}

bool CProfileHistory::CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	return true;
}

bool CProfileHistory::NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	switch (item.GetHistoryCount())
	{
	case eHC_Zero:
		return true;
	case eHC_One:
		if (SElem* pElem = pSyncContext->GetPrevElem(ctx))
			return ctx.ctxObj.main->vAspectProfiles[ctx.index] != pElem->data;
		else
			return true;
	case eHC_MoreThanOne:
		CRegularlySyncedItem::ConstValueIter iter = item.GetConstIter();
		uint32 oldestSeq = 0;
		while (const SElem* pOther = iter.Current())
		{
			if (pOther->data != ctx.ctxObj.main->vAspectProfiles[ctx.index])
				return true;
			oldestSeq = pOther->seq;
			iter.Next();
		}
		return oldestSeq > ctx.basisSeq;
	}
	NET_ASSERT(false);
	return false;
}

bool CProfileHistory::SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue)
{
	ctx.pSender->BeginMessage(ctx.pView->GetConfig().pSetAspectProfileMsgs[ctx.index]);
	uint8 profile = ctx.ctxObj.main->vAspectProfiles[ctx.index];
	ctx.pSender->ser.Value("profile", profile);
	newValue = profile;
	return true;
}
