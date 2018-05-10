// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: playback of generic breaking events
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __GENERICPLAYBACKLISTENER_H__
#define __GENERICPLAYBACKLISTENER_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "IBreakReplicatorListener.h"
	#include "IBreakPlaybackStream.h"
	#include "SimulateRemoveEntityParts.h"
	#include "BreakReplicator.h"

class CGenericPlaybackListener : public IBreakReplicatorListener
{
public:
	CGenericPlaybackListener();

	void Perform(const SSimulateRemoveEntityParts& param, INetBreakagePlaybackPtr pBreakage);
	bool AcceptUpdateMesh(const EventPhysUpdateMesh* pEvent);
	bool AcceptCreateEntityPart(const EventPhysCreateEntityPart* pEvent);
	bool AcceptRemoveEntityParts(const EventPhysRemoveEntityParts* pEvent);
	bool AcceptJointBroken(const EventPhysJointBroken*);

	void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	void OnRemove(IEntity* pEntity);

	void OnPostStep() {}

	void EndEvent(INetContext* pCtx);

	bool AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
	{
		return false;
	}

	virtual void OnStartFrame();
	virtual bool OnEndFrame();

	virtual void OnTimeout();

	const char*  GetName()
	{
		return "GenericPlaybackListener";
	}

private:
	int                     m_spawnIdx;
	INetBreakagePlaybackPtr m_pBreakage;
};

template<class TParam>
class CGenericPlaybackStream : public IBreakPlaybackStream
{
public:
	CGenericPlaybackStream(const TParam& param) : m_param(param) {}

	bool GotExplosiveObjectState(const SExplosiveObjectState* state)
	{
		return false;
	}
	bool GotProceduralSpawnRec(const SProceduralSpawnRec* rec)
	{
		return false;
	}
	bool GotJointBreakRec(const SJointBreakRec* rec)
	{
		return false;
	}
	bool GotJointBreakParticleRec(const SJointBreakParticleRec* rec)
	{
		return false;
	}

	IBreakReplicatorListenerPtr Playback(CBreakReplicator* pReplicator, INetBreakagePlaybackPtr pBreak)
	{
		_smart_ptr<CGenericPlaybackListener> pListener = new CGenericPlaybackListener();
		pReplicator->BeginEvent(&*pListener);
		pListener->Perform(m_param, pBreak);
		pReplicator->EndEvent();
		return 0;
	}

protected:
	CObjectSelector GetObjectSelector(int idx)
	{
		CRY_ASSERT(false);
		return CObjectSelector();
	}

private:
	TParam m_param;
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
