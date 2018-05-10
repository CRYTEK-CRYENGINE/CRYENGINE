// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MementoHistory.h"
#include "Streams/ArithStream.h"
#include "Protocol/Serialize.h"
#include "Network.h"
#include "../ContextView.h"

class CMementoHistory::CMementoInputStream : public CByteInputStream
{
public:
	CMementoInputStream(uint32 id) : CByteInputStream((const uint8*)MMM().PinHdl(id), MMM().GetHdlSize(id))
	{
	}
};

CMementoReadSyncedItems::CMementoReadSyncedItems() : CRegularlySyncedItems(eHSS_AspectData + 1, 2)
{
}

CMementoHistory::CMementoHistory(CContextView* pView) : CHistory(eHSS_AspectData, 2)
{
	indexMask = pView->Context()->DeclaredAspects();
}

bool CMementoHistory::OwnData(bool writing, const SSyncContext& ctx)
{
	bool isObjectOwner = ctx.ctxObj.main->bOwned;
	bool hasAuthority = ctx.pViewObj->authority;

	bool result = false;
	if (writing)
	{
		if (isObjectOwner && hasAuthority)
			result = true;
	}
	else
	{
		if (!isObjectOwner && hasAuthority)
			result = true;
	}
	return result;
}

bool CMementoHistory::CanSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	// any data yet?
	return ctx.ctxObj.xtra->vAspectData[ctx.index] != CMementoMemoryManager::InvalidHdl;
}

bool CMementoHistory::NeedToSync(const SSyncContext& ctx, CSyncContext* pSyncContext, const CRegularlySyncedItem& item)
{
	// low-memory-touching check
	uint32 ackedVersion = ctx.pViewObjEx->ackedAspectVersions[ctx.index];
	uint32 currentVersion = ctx.ctxObj.xtra->vAspectDataVersion[ctx.index];
	if (ackedVersion != currentVersion || ackedVersion == ~uint32(0) || currentVersion == ~uint32(0))
		return true;
	else if (ackedVersion != ~uint32(0) && currentVersion != ~uint32(0) && ackedVersion == currentVersion)
		return false;

	// linear scan of history
	SElem* pElem = pSyncContext->GetPrevElem(ctx);

	// have we sent anything
	if (!pElem)
		return true;

	// does the saved version tag match the current version?
	// here we start looking at memento data - very heavy memory thrashing :(
	CMementoInputStream in(pElem->data);
	in.GetTyped<uint8>(); //skip first byte!
	uint32 version = in.GetTyped<uint32>();
	if (version != currentVersion)
		return true;

	NET_ASSERT((ctx.ctxObj.main->vAspectProfiles[ctx.index] & ~PROFILE_MASK) == 0);

	const uint8* pBasisBuffer = (const uint8*)MMM().PinHdl(pElem->data);
	const uint8 bonusByte = pBasisBuffer[0];
	uint8 storedProfile = bonusByte & PROFILE_MASK;
	if (storedProfile != ctx.ctxObj.main->vAspectProfiles[ctx.index])
		return true;

	bool isOwner = OwnData(true, ctx);
	if (((bonusByte & OWNER_BIT) != 0) != isOwner)
		return true;

	return (bonusByte & POLLUTION_BIT) != 0;
}

bool CMementoHistory::SendCurrentValue(const SSendContext& ctx, CSyncContext* pSyncContext, uint32& newValue)
{
	ctx.pSender->BeginMessage(ctx.pView->GetConfig().pUpdateAspectMsgs[ctx.index]);
	ChunkID chunkID = GetChunkIDFromObject(ctx.ctxObj, ctx.index);
	bool isOwner = OwnData(true, ctx);

	CTimeValue timeValueStorage;
	CTimeValue* pTimeValue = 0;
	if (ctx.pView->Context()->TimestampedAspects() & (1 << ctx.index))
		pTimeValue = &timeValueStorage;

#if STATS_COLLECTOR
	STATS.BeginGroup(ctx.pView->Context()->GetAspectName(ctx.index));
#endif
	SSerializeParams sp(ctx, ctx.pSender->ser);
	sp.pTimeValue = pTimeValue;
	sp.profile = ctx.ctxObj.main->vAspectProfiles[ctx.index];
	sp.chunkID = chunkID;
	sp.pElem = pSyncContext->GetPrevElem(ctx);
	sp.commit = true;
	sp.isOwner = isOwner;
	sp.pNewValue = &newValue;
	sp.pAspectData = const_cast<TMemHdl*>(&sp.ctx.ctxObj.xtra->vAspectData[sp.ctx.index]);
	bool ok = Serialize(sp);
	/*	if (!ok)
	   {
	    int i = 0;
	   }
	 */
#if STATS_COLLECTOR
	STATS.EndGroup();
#endif

#if ENABLE_DEBUG_KIT
	uint8 bonusByte = POLLUTION_BIT;
	if (sp.pElem)
	{
		CByteInputStream curState((const uint8*)MMM().PinHdl(sp.pElem->data), MMM().GetHdlSize(sp.pElem->data));
		bonusByte = curState.GetTyped<uint8>();
	}
	if (pTimeValue && !(bonusByte & POLLUTION_BIT))
	{
		CTimeValue curTimeValue = ctx.pView->ContextState()->GetLocalPhysicsTime();
		if (curTimeValue != timeValueStorage)
		{
			NetQuickLog(true, 1, "Physics update sent %fms late", (curTimeValue - timeValueStorage).GetMilliSeconds());
		}
	}
#endif

	return ok;
}

bool CMementoHistory::ReadCurrentValue(const SReceiveContext& ctx, bool commit)
{
	ChunkID chunkID = GetChunkIDFromObject(ctx.ctxObj, ctx.index);
	uint8 profile = ctx.ctxObj.main->vAspectProfiles[ctx.index];
	CHistory* pProfHistory = ctx.pView->GetHistory(eH_Profile);
	CHistory* pAuthHistory = ctx.pView->GetHistory(eH_Auth);

	if (pProfHistory->indexMask & (1 << ctx.index))
	{
		SSyncContext tempCtx = ctx;
		tempCtx.basisSeq = ctx.pView->Parent()->GetMostRecentAckedSeq();
		if (SElem* pProfElem = pProfHistory->FindPrevElem(tempCtx))
		{
			ChunkID basisChunkID = ctx.ctxObj.xtra->vAspectProfileChunks[ctx.index][pProfElem->data];
			if (chunkID != basisChunkID)
			{
				profile = pProfElem->data;
				chunkID = basisChunkID;
				commit = false;
			}
		}
	}
	bool isOwner = OwnData(false, ctx);

	uint32 newHandle = CMementoMemoryManager::InvalidHdl;

	CRegularlySyncedItem item(m_receiveBuffer.GetItem(ctx.index, ctx.pViewObjEx->historyElems));
	SElem* pElem = item.FindPrevElem(ctx.basisSeq, ctx.currentSeq);
	if (pElem)
		item.RemoveOlderThan(pElem->seq);

	CTimeValue timeValueStorage = ctx.pView->GetRemotePhysicsTime();
	CTimeValue* pTimeValue = 0;
	if (ctx.pView->Context()->TimestampedAspects() & (1 << ctx.index))
	{
		pTimeValue = &timeValueStorage;
#if LOG_BUFFER_UPDATES
		if (CNetCVars::Get().LogBufferUpdates)
			NetLog("[sync] memento history frame time is %f", timeValueStorage.GetMilliSeconds());
#endif
	}

	SSerializeParams sp(ctx, ctx.ser);
	sp.pTimeValue = pTimeValue;
	sp.profile = profile;
	sp.chunkID = chunkID;
	sp.pElem = pElem;
	sp.commit = commit;
	sp.isOwner = isOwner;
	sp.pNewValue = &newHandle;
	sp.pAspectData = const_cast<TMemHdl*>(&sp.ctx.ctxObj.xtra->vRemoteAspectData[sp.ctx.index]);
	bool ok = Serialize(sp);

	if (ok)
		item.AddElem(ctx.currentSeq, newHandle);

#if LOG_BUFFER_UPDATES
	if (CNetCVars::Get().LogBufferUpdates && pTimeValue)
	{
		NetLog("[sync] memento history reads time %f", pTimeValue->GetMilliSeconds());
	}
#endif

	if (ok && commit)
		ctx.pView->ContextState()->NotifyGameOfAspectUpdate(ctx.objId, ctx.index, ctx.pView->Parent(), timeValueStorage);

	return ok;
}

bool CMementoHistory::Serialize(const SSerializeParams& sp)
{
	bool doCommit = sp.commit;

#if CHECK_MEMENTO_AGES
	uint32 usingBasisId = ~uint32(0);
#endif

	TSerialize ser = sp.ser;
	uint8 newProfile = sp.profile;

	static_assert(MaxProfilesPerAspect <= 8, "Invalid profile count!");        // If this fails, increase the size of the policy below
	ser.Value("profile", newProfile, 'ui3');
	if (newProfile != sp.profile)
	{
		// Don't change anything just read through the packet
		doCommit = false;
	}

	// if commit isn't on and we're writing, something has gone astray
	if (!doCommit)
	{
		NET_ASSERT(sp.ser.IsReading());
	}

	// find the basis memento buffer if we have one
	CByteInputStream curState(0, 0);
	if (sp.pElem)
	{
		curState = CByteInputStream((const uint8*)MMM().PinHdl(sp.pElem->data), MMM().GetHdlSize(sp.pElem->data));
		uint8 bonusByte = curState.GetTyped<uint8>();

		NET_ASSERT((bonusByte & ILLEGAL_BITS) == 0);

		// peek at the profile byte (bonus byte)
		uint8 storedProfile = bonusByte & PROFILE_MASK;
		bool wasOwner = (bonusByte & OWNER_BIT) != 0;
		if (storedProfile != newProfile || wasOwner != sp.isOwner)
		{
#if LOG_OUTGOING_MESSAGES | LOG_INCOMING_MESSAGES
			if (CNetCVars::Get().LogNetMessages & 32)
				NetLog("Use no basis [profile changed %d->%d from %d or ownership changed %d->%d]", newProfile, storedProfile, sp.pElem->seq, wasOwner, sp.isOwner);
#endif
			curState = CByteInputStream(0, 0);
		}
		else
		{
#if CHECK_MEMENTO_AGES
			usingBasisId = sp.pElem->seq;
#endif
#if LOG_OUTGOING_MESSAGES | LOG_INCOMING_MESSAGES
			if (CNetCVars::Get().LogNetMessages & 32)
				NetLog("Use basis from %d", sp.pElem->seq);
#endif
		}
	}
#if LOG_OUTGOING_MESSAGES | LOG_INCOMING_MESSAGES
	else
	{
		if (CNetCVars::Get().LogNetMessages & 32)
			NetLog("Use no basis");
	}
	if (CNetCVars::Get().LogNetMessages & 32)
		NetLog("Commit: %d   owned: %d", doCommit, sp.isOwner);
#endif
	// initialize streams
	CMementoStreamAllocator streamAllocatorForNewState(&MMM());
	CByteOutputStream newState(&streamAllocatorForNewState, curState.GetSize() > 0 ? curState.GetSize() : 16, UP_STACK_PTR);
	newState.PutTyped<uint8>() = newProfile + sp.isOwner * OWNER_BIT;
	CNetSerialize* pSer = GetNetSerializeImpl(sp.ser);
	uint32 mementoAge = sp.ctx.currentSeq - sp.ctx.basisSeq;

	// skip over first 32 bit word of curstate if it's there
	if (curState.GetSize())
		curState.GetTyped<uint32>();

	// store the version number for comparison later
	newState.PutTyped<uint32>() = sp.ctx.ctxObj.xtra->vAspectDataVersion[sp.ctx.index];

	pSer->SetMementoStreams(curState.GetSize() ? &curState : NULL, &newState, mementoAge, sp.ctx.timeValue, sp.isOwner);

	TMemHdl* pAspectData = sp.pAspectData;

	if (!doCommit)
		pAspectData = 0;

#if CHECK_MEMENTO_AGES
	if (sp.ser.IsWriting())
	{
		ser.Value("basis", usingBasisId);
	}
	else
	{
		uint32 checkBasisId;
		ser.Value("basis", checkBasisId);
		NET_ASSERT(usingBasisId == checkBasisId);
	}
#endif

	// finally serialize
	ESerializeChunkResult r = pSer->SerializeChunk(sp.chunkID, newProfile, pAspectData, sp.pTimeValue, sp.ctx.pView->ContextState()->GetStateMMM());

	switch (r)
	{
	case eSCR_Ok_Updated:
	case eSCR_Ok:
		// and 'return' the new buffer index
		*sp.pNewValue = streamAllocatorForNewState.GetHdl();
		MMM().ResizeHdl(*sp.pNewValue, newState.GetSize());
		return true;
	case eSCR_Failed:
		MMM().FreeHdl(streamAllocatorForNewState.GetHdl());
		return false;
	}
	NET_ASSERT(!"should never reach here");
	return false;
}

void CMementoHistory::Destroy(uint32 data)
{
	MMM().FreeHdl(data);
}

void CMementoHistory::Flush(const SSyncContext& ctx)
{
	CHistory::Flush(ctx);

	CRegularlySyncedItem item(m_receiveBuffer.GetItem(ctx.index, ctx.pViewObjEx->historyElems));
	item.Flush();
}

void CMementoHistory::HandleEvent(const SHistoricalEvent& event)
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
	case eHE_Pollute:
		{
			SContextObjectRef ctxObj = event.pView->ContextState()->GetContextObject(event.objId);
			//if (const SContextObject * pCtxObj = event.pView->Context()->GetContextObject( event.id ))
			if (ctxObj.main)
			{
				CRegularlySyncedItem item = GetItem(event.index, event.pViewObjEx->historyElems);
				CRegularlySyncedItem::ValueIter viter = item.GetIter();
				while (SElem* pElem = viter.Current())
				{
					*(uint8*)MMM().PinHdl(pElem->data) |= POLLUTION_BIT;
					viter.Next();
				}
			}
		}
		break;
	case eHE_Reset:
		m_receiveBuffer.Reset();
	}
}

void CMementoReadSyncedItems::Destroy(uint32 data)
{
	MMM().FreeHdl(data);
}
