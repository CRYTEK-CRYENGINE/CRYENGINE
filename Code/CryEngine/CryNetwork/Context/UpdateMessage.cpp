// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UpdateMessage.h"
#include "ContextView.h"
#include "History/History.h"
#include "DebugKit/DebugKit.h"
#include "Utils.h"
#include <CrySystem/ISystem.h>
#include <CryGame/IGameFramework.h>

ILINE static NetworkAspectType BitIf(NetworkAspectID bitIndex, bool flag)
{
	return NetworkAspectType(flag) << bitIndex;
}

CUpdateMessage::CUpdateMessage(const SUpdateMessageConfig& cfg, uint32 syncFlags)
	: INetSendable(0, cfg.m_pStartUpdateDef->reliability)
	, SUpdateMessageConfig(cfg)
	, m_syncFlags(syncFlags)
#if ENABLE_THIN_BINDS
	, m_bindAspectMask(0)
#endif // ENABLE_THIN_BINDS
	, m_wasSent(false)
	, m_anythingSent(false)
{
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (ctxObj.main)
		SetPulses(ctxObj.xtra->pPulseState);
	++g_objcnt.updateMessage;
}

CUpdateMessage::~CUpdateMessage()
{
	--g_objcnt.updateMessage;
}

#if ENABLE_PACKET_PREDICTION
// The below is an approximation and will not always match which aspects are sent (sent may send less)
NetworkAspectType CUpdateMessage::ComputeSendAspectMask(INetSender* pSender, SHistorySyncContext& hsc, SContextViewObject* pViewObj, SContextViewObjectEx* pViewObjEx, bool& willSend)
{
	willSend = false;

	if (m_pView->IsLocal() || (!m_pView->Context()->IsMultiplayer()))
	{
		return 0;
	}

	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!ctxObj.main || !ctxObj.main->userID)
	{
		NetWarning("UpdateMessage: No such object %s", m_netID.GetText());
		return 0;
	}
	if (m_syncFlags & eSCF_EnsureEnabled)
	{
		if (!m_pView->IsObjectEnabled(m_netID))
		{
			NetWarning("UpdateMessage: Object %s not enabled", m_netID.GetText());
			return 0;
		}
	}
	if (pViewObj->locks)
	{
		return 0;
	}

	NetworkAspectType sentAspectsNormalMask = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, CContextView::eGSAA_DefaultAuthority);
	NetworkAspectType sentAspectMask = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, CContextView::eGSAA_AssumeNoAuthority);
	NetworkAspectType partialUpdateForces = 0;
	NetworkAspectType maySend = sentAspectsNormalMask;

	CTimeValue oneSecond;
	oneSecond.SetSeconds((int64)1LL);
	CBitIter iter(sentAspectMask);
	NetworkAspectID aspectIdx;
	while (iter.Next(aspectIdx))
	{
		// cppcheck-suppress nullPointer
		partialUpdateForces |= BitIf(aspectIdx, (g_time - pViewObjEx->partialUpdateReceived[aspectIdx]) < oneSecond || pViewObjEx->partialUpdatesRemaining[aspectIdx] > 0);
	}
	maySend &= pViewObjEx->dirtyAspects;

	maySend |= partialUpdateForces;

	SSyncContext ctx;
	ctx.basisSeq = pSender->nBasisSeq;
	ctx.currentSeq = pSender->nCurrentSeq;
	ctx.flags = m_syncFlags;
	ctx.objId = m_netID;
	ctx.ctxObj = ctxObj;
	ctx.pViewObj = pViewObj;
	ctx.pViewObjEx = pViewObjEx;
	ctx.pView = m_pView;

	const uint8 forceSendData = (m_syncFlags & eSCF_AssumeEnabled) != 0;

	CHistory* pAuthHistory = m_pView->GetHistory(eH_Auth);
	CHistory* pHistory = m_pView->GetHistory(eH_AspectData);

	NetworkAspectType aspectMask = maySend & pHistory->indexMask;
	NetworkAspectType isAspectData = 1;
	NetworkAspectType sendingAuth = 0;//pAuthHistory->indexMask!=0;
	NetworkAspectType mustSync = sentAspectMask & ((~(isAspectData & sendingAuth)) + 1);
	mustSync |= partialUpdateForces & ((~isAspectData) + 1);
	mustSync |= sentAspectsNormalMask & ((~isAspectData) + 1) & ((~forceSendData) + 1);
	aspectMask |= mustSync;

	CBitIter bitIter(NetworkAspectType(aspectMask & (~mustSync)));
	while (bitIter.Next(ctx.index))
	{
		aspectMask &= ~BitIf(ctx.index, !hsc.ctx[eH_AspectData][ctx.index].NeedToSync(pHistory, ctx));
	}
	bitIter = CBitIter(NetworkAspectType(aspectMask & mustSync));
	while (bitIter.Next(ctx.index))
	{
		aspectMask &= ~BitIf(ctx.index, !hsc.ctx[eH_AspectData][ctx.index].PrepareToSync(pHistory, ctx));
	}

	willSend = true;
	return aspectMask;    // Local channel/!multiplayer
}

	#include <CryEntitySystem/IEntitySystem.h>

SMessageTag CUpdateMessage::GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
{
	bool willSend = false;
	SMessageTag mTag;
	mTag.messageId = mapper->GetMsgId(m_pStartUpdateDef);

	if (!m_pView->IsObjectBound(m_netID))
	{
		mTag.messageId = 0xFFFFFFFF;
		return mTag;
	}

	IEntityClass* pEntityClass = NULL;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_pView->m_pContextState->GetEntityID(m_netID));

	if (pEntity != NULL)
	{
		pEntityClass = pEntity->GetClass();
	#if CRY_PLATFORM_64BIT
		mTag.varying1 = (uint64)pEntityClass;
	#else
		uint32 t = (uint32)pEntityClass;
		mTag.varying1 = (uint64)t;
	#endif
	}
	else
	{
		// Failed to get class
		mTag.messageId = 0xFFFFFFFF;
		return mTag;
	}

	SContextViewObject* pViewObj = &m_pView->m_objects[m_netID.id];

	if (m_handle != pViewObj->activeHandle)
	{
		mTag.messageId = 0xFFFFFFFF;
		return mTag;
	}

	SContextViewObjectEx* pViewObjEx = 0;
	if (!m_pView->m_objectsEx.empty())
		pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

	SHistorySyncContext hsc;
	NetworkAspectType expectedAspectMask = ComputeSendAspectMask(pSender, hsc, pViewObj, pViewObjEx, willSend);
	if (!willSend)
	{
		mTag.messageId = 0xFFFFFFFF;
	}
	else
	{
		mTag.varying2 = expectedAspectMask;
	}

	return mTag;
}
#endif

EMessageSendResult CUpdateMessage::Send(INetSender* pSender)
{
	NET_ASSERT(!(!m_handle));

	m_anythingSent = false;
	m_immediateResendAspects = 0;

	if (!m_pView->IsObjectBound(m_netID))
		return eMSR_FailedMessage;

	SContextViewObject* pViewObj = &m_pView->m_objects[m_netID.id];
	if (m_handle != pViewObj->activeHandle)
		return eMSR_FailedMessage;

	SContextViewObjectEx* pViewObjEx = 0;
	if (!m_pView->m_objectsEx.empty())
		pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

	for (int i = 0; i < eH_NUM_HISTORIES; i++)
		m_sentHistories[i] = 0;
	for (NetworkAspectID i = 0; i < NumAspects; i++)
		m_sentAspectVersions[i] = ~uint32(0);

	SHistorySyncContext hsc;

	// determine which aspects we need to sync for each history
	EMessageSendResult res = InitialChecks(hsc, pSender, pViewObj, pViewObjEx);
	if (res != eMSR_SentOk)
		return res;

	bool containsData = false;
	for (int history = 0; history < eH_NUM_HISTORIES; history++)
		containsData |= m_maybeSendHistories[history] != 0;
	if (!containsData)
		containsData = AnyAttachments(m_pView, m_netID);
	if (!containsData)
		containsData = CheckHook();

	if (containsData)
	{
		DEBUGKIT_SET_OBJECT(m_pView->ContextState()->GetContextObject(m_netID).main->userID);

		TUpdateMessageBrackets brackets = m_pView->GetUpdateMessageBrackets();

		res = WorstMessageSendResult(res, WriteHook(eWH_BeforeBegin, pSender));
		if (res != eMSR_SentOk)
			return res;
		UpdateMsgHandle();
		if (brackets.first == m_pStartUpdateDef)
			pSender->BeginUpdateMessage(m_netID);
		else
		{
			pSender->BeginMessage(m_pStartUpdateDef);
			pSender->ser.Value("netID", m_netID, 'eid');
		}
		res = WorstMessageSendResult(res, WriteHook(eWH_DuringBegin, pSender));
		res = WorstMessageSendResult(res, SendMain(hsc, pSender));
		if (brackets.first == m_pStartUpdateDef)
			pSender->EndUpdateMessage();
		else
			pSender->BeginMessage(m_pView->m_config.pEndUpdateMsg);

		DEBUGKIT_SET_OBJECT(0);

		NET_ASSERT(m_anythingSent);
	}
	else
	{
		UpdateMsgHandle();
	}

	return res;
}

void CUpdateMessage::UpdateMsgHandle()
{
	m_pView->m_objects[m_netID.id].msgHandle = SSendableHandle();
	if (m_immediateResendAspects)
		m_pView->ChangedObject(m_netID, eCOF_NeverSubstitute, m_immediateResendAspects);
}

EMessageSendResult CUpdateMessage::InitialChecks(SHistorySyncContext& hsc, INetSender* pSender, SContextViewObject* pViewObj, SContextViewObjectEx* pViewObjEx)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (!m_pView->IsObjectBound(m_netID))
	{
		NetWarning("UpdateMessage: No such object %s", m_netID.GetText());
		return eMSR_FailedMessage;
	}

	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!ctxObj.main || !ctxObj.main->userID)
	{
		NetWarning("UpdateMessage: No such object %s", m_netID.GetText());
		return eMSR_FailedMessage;
	}
	for (int history = 0; history < eH_NUM_HISTORIES; history++)
		m_maybeSendHistories[history] = NET_ASPECT_ALL;
	NetworkAspectType sentAspectsNormalMask = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, CContextView::eGSAA_DefaultAuthority);
	m_maybeSendHistories[eH_AspectData] = sentAspectsNormalMask;

	m_maybeSendHistories[eH_Profile] = m_pView->Context()->ServerManagedProfileAspects();
	if (!ctxObj.main->bOwned || ctxObj.main->spawnType == eST_Collected)
		m_maybeSendHistories[eH_Profile] = 0;

	if (m_syncFlags & eSCF_EnsureEnabled)
	{
		if (!m_pView->IsObjectEnabled(m_netID))
		{
			NetWarning("UpdateMessage: Object %s not enabled", m_netID.GetText());
			return eMSR_FailedMessage;
		}
	}

	SSyncContext ctx;
	ctx.basisSeq = pSender->nBasisSeq;
	ctx.currentSeq = pSender->nCurrentSeq;
	ctx.flags = m_syncFlags;
	ctx.objId = m_netID;
	ctx.ctxObj = ctxObj;
	ctx.pViewObj = pViewObj;
	ctx.pViewObjEx = pViewObjEx;
	ctx.pView = m_pView;

	if (pViewObj->locks)
		return eMSR_NotReady;

	NetworkAspectType partialUpdateForces = 0;
	NetworkAspectType sentAspectMask = m_pView->GetSentAspects(m_netID, (m_syncFlags & eSCF_AssumeEnabled) != 0, CContextView::eGSAA_AssumeNoAuthority);
	bool isLocal = ctx.pView->IsLocal();
	bool multiplayer = ctx.pView->Context()->IsMultiplayer();
	if (multiplayer && !isLocal)
	{
		CTimeValue oneSecond;
		oneSecond.SetSeconds((int64)1LL);
		CBitIter iter(sentAspectMask);
		NetworkAspectID aspectIdx;
		while (iter.Next(aspectIdx))
		{
			// cppcheck-suppress nullPointer
			partialUpdateForces |= BitIf(aspectIdx, (g_time - pViewObjEx->partialUpdateReceived[aspectIdx]) < oneSecond || pViewObjEx->partialUpdatesRemaining[aspectIdx] > 0);
		}
		m_maybeSendHistories[eH_AspectData] &= pViewObjEx->dirtyAspects;
	}
	m_maybeSendHistories[eH_AspectData] |= partialUpdateForces;
	m_immediateResendAspects = partialUpdateForces;

	if (multiplayer && !isLocal)
	{
		const uint8 forceSendData = (m_syncFlags & eSCF_AssumeEnabled) != 0;

		for (int history = 0; history < eH_NUM_HISTORIES; history++)
		{
			CHistory* pHistory = m_pView->GetHistory((EHistory)history);

			// remember... uint8(-1) == 0xff
			NetworkAspectType aspectMask = m_maybeSendHistories[history] & pHistory->indexMask;
			NetworkAspectType isAspectData = (history == eH_AspectData);
			NetworkAspectType sendingAuth = m_maybeSendHistories[eH_Auth] != 0;
			NetworkAspectType mustSync = sentAspectMask & ((~(isAspectData & sendingAuth)) + 1);
			mustSync |= partialUpdateForces & ((~isAspectData) + 1);
			mustSync |= sentAspectsNormalMask & ((~isAspectData) + 1) & ((~forceSendData) + 1);
			aspectMask |= mustSync;
#if ENABLE_THIN_BINDS
			if ((history == eH_AspectData) && (m_syncFlags & eSCF_UseBindAspectMask))
			{
				aspectMask &= m_bindAspectMask;
			}
#endif // ENABLE_THIN_BINDS
			if (aspectMask)
			{
				CBitIter bitIter(NetworkAspectType(aspectMask & (~mustSync)));
				while (bitIter.Next(ctx.index))
					aspectMask &= ~BitIf(ctx.index, !hsc.ctx[history][ctx.index].NeedToSync(pHistory, ctx));
				bitIter = CBitIter(NetworkAspectType(aspectMask & mustSync));
				while (bitIter.Next(ctx.index))
					aspectMask &= ~BitIf(ctx.index, !hsc.ctx[history][ctx.index].PrepareToSync(pHistory, ctx));
			}
			m_maybeSendHistories[history] = aspectMask;
		}
	}
	else
	{
		for (int i = 0; i < eH_NUM_HISTORIES; i++)
			m_maybeSendHistories[i] = 0;
	}

	return eMSR_SentOk;
}

void CUpdateMessage::GetPositionInfo(SMessagePositionInfo& pos)
{
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	pos.obj = m_netID;
#if FULL_ON_SCHEDULING
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!ctxObj.main)
		return;
	pos.havePosition = ctxObj.xtra->hasPosition;
	pos.position = ctxObj.xtra->position;
	pos.haveDrawDistance = ctxObj.xtra->hasDrawDistance;
	pos.drawDistance = ctxObj.xtra->drawDistance;
#endif
}

EMessageSendResult CUpdateMessage::SendMain(SHistorySyncContext& hsc, INetSender* pSender)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	SNetChannelEvent event;
	event.event = eNCE_SentUpdate;
	event.id = m_netID;
	event.seq = pSender->nCurrentSeq;
	m_pView->ContextState()->BroadcastChannelEvent(m_pView->Parent(), &event);

	EMessageSendResult res = eMSR_SentOk;

#if DEEP_BANDWIDTH_ANALYSIS
	const bool DBAEnabled = g_DBAEnabled;
	uint32 dbaPrevSize;
	IF_UNLIKELY (DBAEnabled)
	{
		dbaPrevSize = pSender->GetStreamSize();
		g_DBALargeProfileBuffer.Format("|prelude %u|", (dbaPrevSize - g_DBASizePriorToUpdate));
	}
#endif
	// validate that the said object still exists
	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	if (!m_pView->IsObjectBound(m_netID))
	{
		NET_ASSERT(false);
		return eMSR_FailedMessage;
	}
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextViewObject* pViewObj = &m_pView->m_objects[m_netID.id];

	SContextViewObjectEx* pViewObjEx = 0;
	if (!m_pView->m_objectsEx.empty())
		pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

	NET_ASSERT(!(!(m_netID)));
	NET_ASSERT(ctxObj.main && pViewObj && m_pView->IsObjectBound(m_netID));

	res = WorstMessageSendResult(res, SendAttachments(pSender, eRAT_PreAttach));

	// prepare sync context as well as we can (updated later for per-aspect stuff)
	SSendContext ctx;
	ctx.basisSeq = pSender->nBasisSeq;
	ctx.currentSeq = pSender->nCurrentSeq;
	ctx.timeValue = pSender->timeValue;
	ctx.flags = m_syncFlags;
	ctx.objId = m_netID;
	ctx.ctxObj = ctxObj;
	ctx.pSender = pSender;
	ctx.pSentAlready = m_sentHistories;
	ctx.pView = m_pView;
	ctx.pViewObj = pViewObj;

	// synchronize histories, one at a time
	NetworkAspectID aspectIdx;
	// useful log message for figuring out what's being sent...
	//CryLogAlways("%cM: %s -> %s [%08x %08x %08x %08x]", CheckHook()? 'S':'U', m_netID.GetText(), m_pView->Parent()->GetName(), m_maybeSendHistories[0], m_maybeSendHistories[1], m_maybeSendHistories[2], m_maybeSendHistories[3]);
	for (int history = 0; res == eMSR_SentOk && history < eH_NUM_HISTORIES; history++)
	{
		CHistory* pHistory = m_pView->GetHistory((EHistory) history);
		NetworkAspectType aspectMask = pHistory->indexMask & m_maybeSendHistories[history];
#if ENABLE_THIN_BINDS
		if (history == eH_AspectData)
		{
			m_pView->ContextState()->UpdateBindAspectMask(m_netID, aspectMask);
		}
#endif // ENABLE_THIN_BINDS
		CBitIter itAspects(aspectMask);
		NetworkAspectType aspectsSeen = 0;
		while (res == eMSR_SentOk && itAspects.Next(aspectIdx))
		{
			NET_ASSERT(!(aspectsSeen & BIT(aspectIdx)));
			aspectsSeen |= BIT(aspectIdx);

			ctx.index = aspectIdx;
#if DEEP_BANDWIDTH_ANALYSIS
			IF_UNLIKELY (DBAEnabled)
			{
				g_DBASmallProfileBuffer.Format("[%d-%u]", history, aspectIdx);
				g_DBAMainProfileBuffer += g_DBASmallProfileBuffer;
			}
#endif
			switch (hsc.ctx[history][aspectIdx].Send(ctx))
			{
			case eHSR_Ok:
				m_sentHistories[history] |= BIT(aspectIdx);
				SentData();
#if DEEP_BANDWIDTH_ANALYSIS
				IF_UNLIKELY (DBAEnabled)
				{
					g_DBASmallProfileBuffer.Format("|%d-%u : %u|", history, aspectIdx, (ctx.pSender->GetStreamSize() - dbaPrevSize));
					dbaPrevSize = ctx.pSender->GetStreamSize();
					g_DBALargeProfileBuffer += g_DBASmallProfileBuffer;
				}
#endif
				break;
			case eHSR_Failed:
				res = eMSR_FailedMessage;
				break;
			case eHSR_NotNeeded:
				break;
			}
		}
	}

	if (res == eMSR_SentOk)
		res = WorstMessageSendResult(res, SendAttachments(pSender, eRAT_PostAttach));

	if (res == eMSR_SentOk && m_pView->Context()->IsMultiplayer() && pViewObjEx)
	{
		CBitIter sentAspectDataIter(m_sentHistories[eH_AspectData]);
		while (sentAspectDataIter.Next(aspectIdx))
		{
			int& cnt = pViewObjEx->partialUpdatesRemaining[aspectIdx];
			cnt -= (cnt != 0);

			m_sentAspectVersions[aspectIdx] = ctxObj.xtra->vAspectDataVersion[aspectIdx];
		}

		pViewObjEx->dirtyAspects = 0;
	}

	if (m_wasSent)
	{
		NET_ASSERT(res == eMSR_SentOk);
	}

#if DEEP_BANDWIDTH_ANALYSIS
	IF_UNLIKELY (DBAEnabled && res == eMSR_SentOk)
	{
		if (CNetContextState* pContextState = m_pView->ContextState())
		{
			SContextObjectRef ctxObj = pContextState->GetContextObject(m_netID);
			if (!ctxObj.main)
				m_description.Format("%s [not-existing %s]", GetBasicDescription(), m_netID.GetText());
			else
				m_description.Format("%s %s %08X %s %" PRISIZE_T, GetBasicDescription(), ctxObj.main->GetClassName(), m_sentHistories[eH_AspectData], g_DBALargeProfileBuffer.c_str(), m_attachments.size());
		}
		else
		{
			m_description.Format("%s [no context state, old object was %s]", GetBasicDescription(), m_netID.GetText());
		}
	}
#endif

	return res;
}

void CUpdateMessage::UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
{
	if (m_pView->IsDead())
		return;

	SNetChannelEvent event;
	event.event = update == eNSSU_Ack ? eNCE_AckedUpdate : eNCE_NackedUpdate;
	event.id = m_netID;
	event.seq = nFromSeq;
	m_pView->ContextState()->BroadcastChannelEvent(m_pView->Parent(), &event);

	// quick validation useful for debugging...
	switch (update)
	{
	case eNSSU_Ack:
	case eNSSU_Nack:
		break;
	case eNSSU_Requeue:
		m_wasSent = false;
		break;
	case eNSSU_Rejected:
		NET_ASSERT(!m_wasSent);
		break;
	}

	//const SContextObject * pCtxObj = m_pView->ContextState()->GetContextObject(m_netID);
	SContextObjectRef ctxObj = m_pView->ContextState()->GetContextObject(m_netID);
	bool isBound = m_pView->IsObjectBound(m_netID);
	if (isBound && m_pView->Context()->IsMultiplayer() && update != eNSSU_Rejected)
	{
		if (ctxObj.main)
		{
			SContextViewObject* pViewObj = &m_pView->m_objects[m_netID.id];

			SContextViewObjectEx* pViewObjEx = 0;
			if (!m_pView->m_objectsEx.empty())
				pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

			// prepare sync context as well as we can (updated later for per-aspect stuff)
			SSyncContext ctx;
			ctx.currentSeq = nFromSeq;
			ctx.flags = m_syncFlags;
			ctx.objId = m_netID;
			ctx.ctxObj = ctxObj;
			ctx.pSentAlready = m_sentHistories;
			ctx.pView = m_pView;
			ctx.pViewObj = pViewObj;
			ctx.pViewObjEx = pViewObjEx;

			for (int history = 0; history < eH_NUM_HISTORIES; history++)
			{
				if (!m_sentHistories[history])
					continue;
				CHistory* pHistory = m_pView->GetHistory((EHistory) history);
				NetworkAspectID aspectIdx;
				CBitIter iter(m_sentHistories[history]);
				while (iter.Next(aspectIdx))
				{
					ctx.index = aspectIdx;
					pHistory->Ack(ctx, update == eNSSU_Ack);
				}
			}
		}
		if (ctxObj.xtra)
		{
			SContextViewObjectEx* pViewObjEx = 0;
			if (!m_pView->m_objectsEx.empty())
				pViewObjEx = &m_pView->m_objectsEx[m_netID.id];

			if (update == eNSSU_Ack)
			{
				NetworkAspectID aspectIdx;
				CBitIter iter(m_sentHistories[eH_AspectData]);
				while (iter.Next(aspectIdx))
				{
					uint32 sent = m_sentAspectVersions[aspectIdx];
					if (pViewObjEx)
					{
						uint32& acked = pViewObjEx->ackedAspectVersions[aspectIdx];
						if (sent != ~uint32(0) && (sent > acked || acked == ~uint32(0)))
							acked = sent;
					}
				}
			}
			else
			{
				if (pViewObjEx)
					pViewObjEx->dirtyAspects |= m_sentHistories[eH_AspectData];
			}
		}
	}

	UpdateAttachmentsState(nFromSeq, update);
}

bool CUpdateMessage::AnyAttachments(CContextView* pView, SNetObjectID id)
{
	// TODO: check logic

	if (!pView->IsObjectBound(id))
		return false;

	SAttachmentIndex idxFirst = { id, 0 };
	SAttachmentIndex idxLast = { id, ~uint32(0) };

	for (int i = 0; i < 2; i++)
	{
		CContextView::TAttachmentMap::const_iterator iterFirst = pView->m_pAttachments[i]->lower_bound(idxFirst);
		CContextView::TAttachmentMap::const_iterator iterLast = pView->m_pAttachments[i]->upper_bound(idxLast);

		if (iterFirst != iterLast)
			return true;
	}

	return false;
}

void CUpdateMessage::UpdateAttachmentsState(uint32 nFromSeq, ENetSendableStateUpdate update)
{
	for (TAttachments::iterator iter = m_attachments.begin(); iter != m_attachments.end(); ++iter)
	{
		if (iter->pMsg->pListener)
			iter->pMsg->pListener->OnAck(m_pView->Parent(), iter->pMsg->userId, nFromSeq, update == eNSSU_Ack);

		switch (iter->pMsg->reliability)
		{
		case eNRT_UnreliableOrdered:
		case eNRT_UnreliableUnordered:
			update = eNSSU_Ack;
			break;
		case eNRT_ReliableUnordered:
			if (update != eNSSU_Ack)
			{
				m_pView->ScheduleAttachment(false, iter->pMsg);
			}
			break;
		case eNRT_ReliableOrdered:
			if (update != eNSSU_Ack)
			{
				m_pView->ScheduleAttachment(false, iter->pMsg, &iter->idx);
			}
			break;
		}

		SNetChannelEvent evt;
		evt.event = update == eNSSU_Ack ? eNCE_AckedRMI : eNCE_NackedRMI;
		evt.id = m_pView->ContextState()->GetNetID(iter->pMsg->objId);
		evt.seq = nFromSeq;
		evt.pRMIBody = iter->pMsg;
		m_pView->ContextState()->BroadcastChannelEvent(m_pView->Parent(), &evt);
	}

	m_attachments.resize(0);
}

EMessageSendResult CUpdateMessage::SendAttachments(INetSender* pSender, ERMIAttachmentType attachments)
{
	CContextView::TAttachmentMap& am = *m_pView->m_pAttachments[attachments];
	SAttachmentIndex idxFirst = { m_netID, 0 };
	CContextView::TAttachmentMap::iterator iterFirst = am.lower_bound(idxFirst);

	EMessageSendResult res = eMSR_SentOk;

	CContextView::TAttachmentMap::iterator iter;
	for (iter = iterFirst; iter != am.end() && iter->first.id == m_netID; ++iter)
	{
		IRMIMessageBodyPtr pBody = iter->second;
		const SNetMessageDef* pDef = pBody->pMessageDef;
		if (!pDef)
			pDef = m_pView->m_config.pRMIMsgs[eNRT_NumReliabilityTypes];

		NET_ASSERT(pDef);

		NET_ASSERT(pDef->reliability == eNRT_UnreliableOrdered || pDef->reliability == eNRT_UnreliableUnordered);

		pSender->BeginMessage(pDef);
		SentData();

		SSentAttachment sa;
		sa.idx = iter->first;
		sa.pMsg = iter->second;

		if (pBody->reliability == eNRT_ReliableOrdered)
			sa.lock = m_pView->LockObject(iter->first.id, "RMI");

		if (pBody->pListener)
			pBody->pListener->OnSend(m_pView->Parent(), pBody->userId, pSender->nCurrentSeq);

		SNetChannelEvent evt;
		evt.event = eNCE_SendRMI;
		evt.id = m_pView->ContextState()->GetNetID(pBody->objId);
		evt.seq = pSender->nCurrentSeq;
		evt.pRMIBody = pBody;
		m_pView->ContextState()->BroadcastChannelEvent(m_pView->Parent(), &evt);

		if (!pBody->pMessageDef)
		{
			uint8 funcID = pBody->funcId;
			pSender->ser.Value("funcID", funcID);
		}

		pBody->SerializeWith(pSender->ser);

		m_attachments.push_back(sa);
	}

	am.erase(iterFirst, iter);

	return res;
}

size_t CUpdateMessage::GetSize()
{
	return sizeof(*this) +
	       m_attachments.capacity() * sizeof(m_attachments[0]);
}

const char* CUpdateMessage::GetDescription()
{
	if (CNetContextState* pContextState = m_pView->ContextState())
	{
		if (m_description.empty())
		{
			//const SContextObject * pObj = m_pView->ContextState()->GetContextObject(m_netID);
			SContextObjectRef ctxObj = pContextState->GetContextObject(m_netID);
			if (!ctxObj.main)
				m_description.Format("%s [not-existing %s]", GetBasicDescription(), m_netID.GetText());
			else
				m_description.Format("%s %s (%s)", GetBasicDescription(), ctxObj.main->GetName(), m_netID.GetText());
		}
	}
	else
	{
		m_description.Format("%s [no context state, old object was %s]", GetBasicDescription(), m_netID.GetText());
	}
	NET_ASSERT(!m_description.empty());
	return m_description.c_str();
}

// regular update message - allocation optimization (safe to be single threaded since we always call with the global lock)

void CRegularUpdateMessage::UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
{
	CUpdateMessage::UpdateState(nFromSeq, state);
}

CRegularUpdateMessage* CRegularUpdateMessage::Create(const SUpdateMessageConfig& cfg, uint32 flags)
{
	ASSERT_GLOBAL_LOCK;
	TMemHdl hdl = MMM().AllocHdl(sizeof(CRegularUpdateMessage));
	CRegularUpdateMessage* p = new(MMM().PinHdl(hdl))CRegularUpdateMessage(cfg, flags);
	p->m_memhdl = hdl;
	return p;
}

void CRegularUpdateMessage::DeleteThis()
{
	ASSERT_GLOBAL_LOCK;
	MMM_REGION(&m_pView->GetMMM());
	TMemHdl hdl = m_memhdl;
	this->~CRegularUpdateMessage();
	MMM().FreeHdl(hdl);
}
