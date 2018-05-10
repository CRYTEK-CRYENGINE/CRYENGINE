// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../NetContext.h"
#include "AuthorityHistory.h"
#include "../ContextView.h"

CAuthorityHistory::CAuthorityHistory(CContextView* pView) : CHistory(eHSS_Auth, 0)
{
	indexMask = (pView->GetConfig().pSetAuthorityMsg != 0) * 0x80;
}

bool CAuthorityHistory::ReadCurrentValue(const SReceiveContext& ctx, bool commit)
{
	bool auth;
	ctx.ser.Value("bAuthority", auth);
	if (commit)
	{
		ctx.pView->ContextState()->UpdateAuthority(ctx.objId, auth, ctx.pView->IsLocal());
		ctx.pViewObj->authority = auth;
	}
	return true;
}

void CAuthorityHistory::HandleEvent(const SHistoricalEvent& event)
{
}

bool CAuthorityHistory::CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	return true;
}

bool CAuthorityHistory::NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	switch (item.GetHistoryCount())
	{
	case eHC_Zero:
		return ctx.pViewObj->authority;
	case eHC_One:
		if (SElem* pElem = pSyncContext->GetPrevElem(ctx))
			return pElem->data != ctx.pViewObj->authority;
		else
			return true;
	case eHC_MoreThanOne:
		CRegularlySyncedItem::ConstValueIter iter = item.GetConstIter();
		uint32 oldestSeq = 0;
		while (const SElem* pOther = iter.Current())
		{
			if (pOther->data != ctx.pViewObj->authority)
				return true;
			oldestSeq = pOther->seq;
			iter.Next();
		}
		return oldestSeq > ctx.basisSeq;
	}
	return false;
}

bool CAuthorityHistory::SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue)
{
	ctx.pSender->BeginMessage(ctx.pView->GetConfig().pSetAuthorityMsg);
	bool auth = ctx.pViewObj->authority;
	ctx.pSender->ser.Value("bAuthority", auth);
	newValue = auth;
	return true;
}
