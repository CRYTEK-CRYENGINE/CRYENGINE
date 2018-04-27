// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------

   Container for Raycast/Primitive deferred queues

   -------------------------------------------------------------------------
   History:
   - 20:10:20010   Created by Benito G.R.

*************************************************************************/

#ifndef __CRYACTION_PHYSICQUEUES_H__
#define __CRYACTION_PHYSICQUEUES_H__

#pragma once

#include <CryPhysics/RayCastQueue.h>
#include <CryPhysics/IntersectionTestQueue.h>

class CCryActionPhysicQueues
{
public:
	typedef RayCastQueue<41>          CryActionRayCaster;
	typedef IntersectionTestQueue<43> CryActionIntersectionTester;

	CCryActionPhysicQueues()
	{
		m_rayCaster.SetQuota(8);
		m_intersectionTester.SetQuota(6);
	}

	CryActionRayCaster&          GetRayCaster()          { return m_rayCaster; }
	CryActionIntersectionTester& GetIntersectionTester() { return m_intersectionTester; }

	void                         Update(float frameTime)
	{
		m_rayCaster.Update(frameTime);
		m_intersectionTester.Update(frameTime);
	}

private:
	CryActionRayCaster          m_rayCaster;
	CryActionIntersectionTester m_intersectionTester;
};

#endif //__CRYACTION_PHYSICQUEUES_H__
