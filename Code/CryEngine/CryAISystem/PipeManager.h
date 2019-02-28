// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PipeManager.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 8:6:2005   14:17 : Created by Kirill Bulatsev

 *********************************************************************/
#ifndef __PipeManager_H__
#define __PipeManager_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <map>
#include <CryAISystem/IAgent.h>

class CGoalPipe;
class CAISystem;
typedef std::map<string, CGoalPipe*> GoalMap;

class CPipeManager
{
	friend class CAISystem;
public:
	enum ActionToTakeWhenDuplicateFound
	{
		SilentlyReplaceDuplicate,
		ReplaceDuplicateAndReportError,
	};

	CPipeManager(void);
	~CPipeManager(void);

	void       ClearAllGoalPipes();
	IGoalPipe* CreateGoalPipe(const char* pName,
	                          const ActionToTakeWhenDuplicateFound actionToTakeWhenDuplicateFound);
	IGoalPipe* OpenGoalPipe(const char* pName);
	CGoalPipe* IsGoalPipe(const char* pName);
	void       Serialize(TSerialize ser);

	/// For debug. Checks the script files for created and used goalpipes and
	/// dumps the unused goalpipes into console. Does not actually use the loaded pipes.
	void CheckGoalpipes();

private:

	// keeps all possible goal pipes that the agents can use
	GoalMap m_mapGoals;
	bool    m_bDynamicGoalPipes;  // to indicate if goalpipe is created after AISystem initialization, loading of \aiconfig.lua
};

#endif // __PipeManager_H__
