// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements vehicle seat specific Mannequin actions

   -------------------------------------------------------------------------
   History:
   - 06:02:2012: Created by Tom Berry

*************************************************************************/
#ifndef __CVEHICLESEATANIMACTIONS_H__
#define __CVEHICLESEATANIMACTIONS_H__

#include <ICryMannequin.h>

class CVehicleSeatAnimActionEnter : public TAction<SAnimationContext>
{
public:

	DEFINE_ACTION("EnterVehicle");

	typedef TAction<SAnimationContext> BaseAction;

	CVehicleSeatAnimActionEnter(int priority, FragmentID fragmentID, CVehicleSeat* pSeat)
		:
		BaseAction(priority, fragmentID),
		m_pSeat(pSeat)
	{
	}

	virtual void Enter() override;

	virtual void Exit() override
	{
		BaseAction::Exit();

		// before sitting down, check if the enter-action is still valid and didn't get interrupted (CE-4209)
		if (m_pSeat->GetCurrentTransition())
		{
			m_pSeat->SitDown();
		}
	}

private:
	CVehicleSeat* m_pSeat;
};

class CVehicleSeatAnimActionExit : public TAction<SAnimationContext>
{
public:

	DEFINE_ACTION("ExitVehicle");

	typedef TAction<SAnimationContext> BaseAction;

	CVehicleSeatAnimActionExit(int priority, FragmentID fragmentID, CVehicleSeat* pSeat)
		:
		BaseAction(priority, fragmentID),
		m_pSeat(pSeat)
	{
	}

	virtual void Enter() override;
	virtual void Exit() override;

private:
	CVehicleSeat* m_pSeat;
};

#endif //!__CVEHICLESEATANIMACTIONS_H__
