// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: don't listen to anything
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __NULLLISTENER_H__
#define __NULLLISTENER_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

class CNullListener : public IBreakReplicatorListener
{
public:
	bool AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
	{
		return false;
	}

	bool AcceptUpdateMesh(const EventPhysUpdateMesh* pEvent)
	{
		return true;
	}
	bool AcceptCreateEntityPart(const EventPhysCreateEntityPart* pEvent)
	{
		return true;
	}
	bool AcceptRemoveEntityParts(const EventPhysRemoveEntityParts* pEvent)
	{
		return true;
	}
	bool AcceptJointBroken(const EventPhysJointBroken* pEvent)
	{
		return true;
	}

	void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
	{
	}
	void OnRemove(IEntity* pEntity)
	{
	}

	void EndEvent(INetContext* pCtx)
	{
	}

	void OnPostStep()
	{
	}

	virtual void OnStartFrame()
	{
	}

	virtual bool OnEndFrame()
	{
		CRY_ASSERT(false);
		return false;
	}

	virtual void OnTimeout()
	{
	}

	const char* GetName()
	{
		return "NullListener";
	}
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
