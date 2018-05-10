// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef EnterLeaveUpdateGoalOp_h
	#define EnterLeaveUpdateGoalOp_h

	#include "GoalOp.h"

// Helper class which you can derive from when you want to get clear
// entry/exit/update points for your goal op.
//
// Preferably, this should be provided by the goal pipe system itself.
//
// Methods you may override:
// - Enter is called when the execution starts.
// - Leave is called when the execution stops.
// - Update is continuously called on non-dry updates until:
//   a) You explicitly say you've succeeded or failed with your operation.
//   b) A new goal pipe is selected and your goal op is pulled out.

class EnterLeaveUpdateGoalOp : public CGoalOp
{
public:
	EnterLeaveUpdateGoalOp()
		: m_initialized(false)
		, m_status(eGOR_IN_PROGRESS)
	{
	}

	// Called when the pipe user starts running this instance of the goal op.
	virtual void Enter(CPipeUser& pipeUser)
	{
	}

	// Called when the pipe user stops running this instance of the goal op.
	virtual void Leave(CPipeUser& pipeUser)
	{
	}

	// Don't override. Do your work in Update.
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser)
	{
		assert(pPipeUser);

		if (!m_initialized)
		{
			Enter(*pPipeUser);
			m_initialized = true;
		}

		Update(*pPipeUser);

		return m_status;
	}

	// Don't override. Do your work in Update.
	virtual void ExecuteDry(CPipeUser* pPipeUser)
	{
	}

	// Don't override. Do your work in OnLeave.
	virtual void Reset(CPipeUser* pPipeUser)
	{
		if (m_initialized)
		{
			Leave(*pPipeUser);

			m_status = eGOR_IN_PROGRESS;
			m_initialized = false;
		}
	}

protected:
	// Override with your own Update code.
	virtual void Update(CPipeUser& pipeUser)
	{
	}

	void          GoalOpSucceeded() { m_status = eGOR_SUCCEEDED; }
	void          GoalOpFailed()    { m_status = eGOR_FAILED; }
	EGoalOpResult GetStatus()       { return m_status; }

private:
	EGoalOpResult m_status;
	bool          m_initialized;
};

#endif // EnterLeaveUpdateGoalOp_h
