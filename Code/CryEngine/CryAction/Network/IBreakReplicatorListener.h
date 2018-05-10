// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: listen to interesting physical events from the game
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __IBREAKREPLICATORLISTENER_H__
#define __IBREAKREPLICATORLISTENER_H__

#pragma once

struct IBreakReplicatorListener;
typedef _smart_ptr<IBreakReplicatorListener> IBreakReplicatorListenerPtr;
struct IProceduralBreakType;
typedef _smart_ptr<IProceduralBreakType>     IProceduralBreakTypePtr;

struct IBreakReplicatorListener : public _reference_target_t
{
	virtual const char* GetName() = 0;

	virtual bool        AttemptAbsorb(const IProceduralBreakTypePtr& pBT) = 0;

	virtual bool        AcceptUpdateMesh(const EventPhysUpdateMesh*) = 0;
	virtual bool        AcceptCreateEntityPart(const EventPhysCreateEntityPart*) = 0;
	virtual bool        AcceptRemoveEntityParts(const EventPhysRemoveEntityParts*) = 0;
	virtual bool        AcceptJointBroken(const EventPhysJointBroken*) = 0;

	virtual void        OnSpawn(IEntity* pEntity, SEntitySpawnParams& params) = 0;
	virtual void        OnRemove(IEntity* pEntity) = 0;

	virtual void        EndEvent(INetContext* pNetContext) = 0;

	virtual void        OnPostStep() = 0;
	virtual void        OnStartFrame() = 0;
	virtual bool        OnEndFrame() = 0;
	virtual void        OnTimeout() = 0;
};

#endif
