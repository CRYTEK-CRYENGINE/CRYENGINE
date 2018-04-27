// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PhysCallbacks.h
//  Created:     14/11/2006 by Anton.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PhysCallbacks_h__
#define __PhysCallbacks_h__
#pragma once

#include <CryPhysics/IDeferredCollisionEvent.h>

class CPhysCallbacks : public Cry3DEngineBase
{
public:
	static void Init();
	static void Done();

	static int  OnFoliageTouched(const EventPhys* pEvent);
	static int  OnPhysStateChange(const EventPhys* pEvent);
	//static int OnPhysAreaChange(const EventPhys *pEvent);
};

class CPhysStreamer : public IPhysicsStreamer
{
public:
	virtual int CreatePhysicalEntity(void* pForeignData, int iForeignData, int iForeignFlags);
	virtual int DestroyPhysicalEntity(IPhysicalEntity* pent) { return 1; }
	virtual int CreatePhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax);
	virtual int DestroyPhysicalEntitiesInBox(const Vec3& boxMin, const Vec3& boxMax);
};

// Deferred physics event object implementing CPhysCallbacks::OnCollision
class CDeferredCollisionEventOnPhysCollision : public IDeferredPhysicsEvent, public Cry3DEngineBase
{
public:
	virtual ~CDeferredCollisionEventOnPhysCollision();

	// Factory create function to pass to CDeferredPhysicsManager::HandleEvent
	static IDeferredPhysicsEvent* CreateCollisionEvent(const EventPhys* pCollisionEvent);

	// Entry function to register as event handler with physics
	static int OnCollision(const EventPhys* pEvent);

	// == IDeferredPhysicsEvent interface == //
	virtual void                                     Start();
	virtual void                                     Execute();
	virtual void                                     ExecuteAsJob();
	virtual int                                      Result(EventPhys*);
	virtual void                                     Sync();
	virtual bool                                     HasFinished();

	virtual IDeferredPhysicsEvent::DeferredEventType GetType() const { return PhysCallBack_OnCollision; }
	virtual EventPhys*                               PhysicsEvent()  { return &m_CollisionEvent; }

	void*                                            operator new(size_t);
	void                                             operator delete(void*);

private:
	// Private constructor, only allow creating by the factory function(which is used by the deferred physics event manager
	CDeferredCollisionEventOnPhysCollision(const EventPhysCollision* pCollisionEvent);

	// == Functions implementing the event logic == //
	void RayTraceVegetation();
	void TestCollisionWithRenderMesh();
	void FinishTestCollisionWithRenderMesh();
	void PostStep();
	void AdjustBulletVelocity();
	void UpdateFoliage();

	// == utility functions == //
	void MarkFinished(int nResult);

	// == state variables to sync the asynchron execution == //
	JobManager::SJobState m_jobStateRayInter;
	JobManager::SJobState m_jobStateOnCollision;

	volatile bool         m_bTaskRunning;

	// == members for internal state of the event == //
	bool m_bHit;                              // did  the RayIntersection hit something
	bool m_bFinished;                         // did an early out happen and we are finished with computation
	bool m_bPierceable;                       // remember if we are hitting a pierceable object
	int  m_nResult;                           // result value of the event(0 or 1)
	CDeferredCollisionEventOnPhysCollision* m_pNextEvent;
	int  m_nWaitForPrevEvent;

	// == internal data == //
	EventPhysCollision      m_CollisionEvent;               // copy of the original physics event
	SRayHitInfo             m_HitInfo;                      // RayIntersection result data
	_smart_ptr<IRenderMesh> m_pRenderMesh;                  // Rendermesh to use for RayIntersection
	bool                    m_bDecalPlacementTestRequested; // check if decal can be placed here
	_smart_ptr<IMaterial>   m_pMaterial;                    // Material for IMaterial
	bool                    m_bNeedMeshThreadUnLock;        // remeber if we need to unlock a rendermesh

	// == members to store values over functions == //
	Matrix34 m_worldTM;
	Matrix33 m_worldRot;
	int*     m_pMatMapping;
	int      m_nMats;

	// == States for ThreadTask and AsyncRayIntersection == //
	SIntersectionData m_RayIntersectionData;

};

#endif // __PhysCallbacks_h__
