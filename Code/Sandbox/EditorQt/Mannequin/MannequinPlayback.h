// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_PLAYBACK_PAGE__H__
#define __MANN_PLAYBACK_PAGE__H__

#include "MannequinBase.h"

class CFragmentPlayback : public CBasicAction
{
public:
	DEFINE_ACTION("FragmentPlayback");

	CFragmentPlayback(ActionScopes scopeMask, uint32 flags);

	virtual void             Enter();
	virtual void             Exit();
	void                     TriggerExit();
	void                     SetFragment(FragmentID fragmentID, TagState fragTags, uint32 option, const CTimeValue& maxTime);
	void                     Restart(const CTimeValue& time = -1);
	void                     SetTime(const CTimeValue& time, bool bForce = false);
	const CTimeValue&        GetTimeSinceInstall() const;
	const CTimeValue&        GetTimeMax() const { return m_maxTime; }
	bool                     ReachedEnd() const;
	virtual IAction::EStatus Update(const CTimeValue& timePassed);
	virtual void             OnSequenceFinished(int layer, uint32 scopeID);

	virtual int              GetPriority() const { return 102; }

private:
	void InternalSetFragment(FragmentID fragmentID, TagState fragTags, uint32 option);

	CTimeValue m_timeSinceInstall;
	CTimeValue m_maxTime;
};

//////////////////////////////////////////////////////////////////////////

class CFragmentSequencePlayback
{
public:
	CFragmentSequencePlayback(const CFragmentHistory& history, IActionController& actionController, EMannequinEditorMode mode, const ActionScopes& scopeMask = ACTION_SCOPES_ALL)
		:
		m_time(0),
		m_maxTime(5),
		m_curIdx(0),
		m_playScale(1),
		m_history(history),
		m_actionController(actionController),
		m_mode(mode),
		m_scopeMask(scopeMask)
	{
	}
	~CFragmentSequencePlayback();

	void Restart(const CTimeValue& time = -1);
	void SetTime(const CTimeValue& time, bool bForce = false);
	void Update(const CTimeValue& time, class CMannequinModelViewport * pViewport);

	const CTimeValue& GetTime() const
	{
		return m_time;
	}

	void SetScopeMask(const ActionScopes& scopeMask)
	{
		m_scopeMask = scopeMask;
	}

	void SetSpeedBias(const mpfloat& playScale);

private:

	void StopPrevious(ActionScopes scopeMask);
	void UpdateDebugParams();

	const CFragmentHistory& m_history;
	IActionController&      m_actionController;

	std::vector<IAction*>   m_actions;

	EMannequinEditorMode    m_mode;

	ActionScopes            m_scopeMask;

	CTimeValue              m_time;
	CTimeValue              m_maxTime;
	uint32                  m_curIdx;
	mpfloat                 m_playScale;

};

#endif

