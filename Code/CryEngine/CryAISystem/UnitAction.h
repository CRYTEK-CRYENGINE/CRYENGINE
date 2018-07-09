// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   UnitAction.h
   $Id$
   Description: refactoring - moved out of LeaderAction.h

   -------------------------------------------------------------------------
   History:
   - 3:6:2005   9:56 : Created by Kirill Bulatsev
    serialization support for unitaction/unitimg

 *********************************************************************/

#ifndef __UnitAction_H__
#define __UnitAction_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryNetwork/ISerialize.h>
#include <list>
#include "AISignal.h"

enum EPriority
{
	PRIORITY_VERY_LOW = 1,
	PRIORITY_LOW,
	PRIORITY_NORMAL,
	PRIORITY_HIGH,
	PRIORITY_VERY_HIGH,
	PRIORITY_LAST // make sure this one is always the last!
};

enum  EUnitAction
{
	UA_NONE,
	UA_SIGNAL,
	UA_SEARCH,
	UA_ACQUIRETARGET,
	UA_LAST,  // make sure this one is always the last!
};

struct AISignalExtraData;
class CUnitAction;

typedef std::list<CUnitAction*> TActionList;
typedef std::list<CUnitAction*> TActionListRef;

class CUnitAction
{
public:
	CUnitAction();
	CUnitAction(EUnitAction eAction, bool bBlocking);
	CUnitAction(EUnitAction eAction, bool bBlocking, const Vec3& point);
	CUnitAction(EUnitAction eAction, bool bBlocking, const Vec3& point, const Vec3& dir);
	CUnitAction(EUnitAction eAction, bool bBlocking, float fDistance);
	CUnitAction(EUnitAction eAction, bool bBlocking, const char* szText);
	CUnitAction(EUnitAction eAction, bool bBlocking, const char* szText, const AISignalExtraData& data);
	CUnitAction(EUnitAction eAction, bool bBlocking, int priority);
	CUnitAction(EUnitAction eAction, bool bBlocking, int priority, const Vec3& point);
	CUnitAction(EUnitAction eAction, bool bBlocking, int priority, const char* szText);
	CUnitAction(EUnitAction eAction, bool bBlocking, int priority, const char* szText, const AISignalExtraData& data);

	~CUnitAction();
	void        Update();
	void        BlockedBy(CUnitAction* blockedAction);
	void        UnlockMyBlockedActions();
	void        NotifyMyBlockingActions();
	inline bool IsBlocked() const         { return !m_BlockingActions.empty(); };
	inline void SetPriority(int priority) { m_Priority = priority; };
	void        Serialize(TSerialize ser);

	EUnitAction       m_Action;
	bool              m_BlockingPlan;
	TActionList       m_BlockingActions;
	TActionList       m_BlockedActions;
	int               m_Priority;
	Vec3              m_Point;
	Vec3              m_Direction;
	float             m_fDistance;
	string            m_SignalText;
	int               m_Tag;
	AISignalExtraData m_SignalData;
};

#endif // __UnitAction_H__
