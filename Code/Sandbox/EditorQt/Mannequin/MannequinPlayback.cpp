// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinPlayback.h"

#include "MannequinDialog.h"
#include "Controls/FragmentEditorPage.h"
#include "Controls/PreviewerPage.h"

#include <CryGame/IGameFramework.h>
#include "MannequinModelViewport.h"

const float MIN_TIME_CHANGE_EPSILON = 0.01f;

const int PLAYER_ACTION_PRIORITY = 10;

//////////////////////////////////////////////////////////////////////////

CFragmentPlayback::CFragmentPlayback(ActionScopes scopeMask, uint32 flags)
	: CBasicAction(PLAYER_ACTION_PRIORITY, FRAGMENT_ID_INVALID, TAG_STATE_EMPTY, flags | IAction::NoAutoBlendOut, scopeMask),
	m_maxTime(5.0f),
	m_timeSinceInstall(0.0f)
{
	AddRef();
}

void CFragmentPlayback::Enter()
{
	CBasicAction::Enter();
}

void CFragmentPlayback::Exit()
{
	CBasicAction::Exit();
}

void CFragmentPlayback::SetFragment(FragmentID fragmentID, TagState fragTags, uint32 option, float maxTime)
{
	m_timeSinceInstall = 0.0f;
	m_maxTime = maxTime;

	__super::SetFragment(fragmentID, fragTags, option);
}

void CFragmentPlayback::Restart(float time)
{
	if (time < 0.0f)
	{
		time = m_timeSinceInstall;
	}
	m_timeSinceInstall = -1.0f;
	SetTime(time, true);
}

void CFragmentPlayback::SetTime(float time, bool bForce)
{
	m_timeSinceInstall = time;
}

float CFragmentPlayback::GetTimeSinceInstall() const
{
	return m_timeSinceInstall;
}

bool CFragmentPlayback::ReachedEnd() const
{
	const float endMarker = CMannequinDialog::GetCurrentInstance()->FragmentEditor()->GetMarkerTimeEnd();
	if (endMarker > 0.0f)
	{
		return m_timeSinceInstall > endMarker;
	}

	return (m_timeSinceInstall > m_maxTime);
}

IAction::EStatus CFragmentPlayback::Update(float timePassed)
{
	m_timeSinceInstall += timePassed;

	return CBasicAction::Update(timePassed);
}

void CFragmentPlayback::OnSequenceFinished(int layer, uint32 scopeID)
{
	__super::OnSequenceFinished(layer, scopeID);
}

//////////////////////////////////////////////////////////////////////////

class CActionFragmentItem : public CBasicAction
{
public:
	CActionFragmentItem(FragmentID fragmentID, const TagState fragTags, const ActionScopes& forcedScopes, bool trumpsPrevious)
		:
		CBasicAction(PLAYER_ACTION_PRIORITY, fragmentID, fragTags, 0, forcedScopes),
		m_trumpsPrevious(trumpsPrevious)
	{
	}

	virtual EPriorityComparison ComparePriority(const IAction& actionCurrent) const
	{
		if (m_trumpsPrevious)
		{
			return (IAction::Installed == actionCurrent.GetStatus() && IAction::Installing & ~actionCurrent.GetFlags()) ? Higher : CBasicAction::ComparePriority(actionCurrent);
		}
		else
		{
			return Equal;
		}
	}

	IAction::EStatus Update(float timePassed)
	{
		return CBasicAction::Update(timePassed);
	}

private:
	bool m_trumpsPrevious;
};

CFragmentSequencePlayback::~CFragmentSequencePlayback()
{
	const uint32 numActions = m_actions.size();
	for (uint32 i = 0; i < numActions; i++)
	{
		m_actions[i]->ForceFinish();
		m_actions[i]->Release();
	}
	m_actions.clear();
}

void CFragmentSequencePlayback::Restart(float time)
{
	if (time < 0.0f)
	{
		time = m_time;
	}
	m_time = -1.0f;
	SetTime(time, true);
}

void CFragmentSequencePlayback::StopPrevious(ActionScopes scopeMask)
{
	const uint32 numActions = m_actions.size();
	for (uint32 i = 0; i < numActions; i++)
	{
		if ((m_actions[i]->GetInstalledScopeMask() & scopeMask) != 0)
		{
			m_actions[i]->Stop();
		}
	}
}

void CFragmentSequencePlayback::SetTime(float time, bool bForce)
{
	const float timeDiff = m_time - time;

	const float MAX_TIME_STEP = 0.2f;

	if (bForce || (fabs_tpl(timeDiff) >= MIN_TIME_CHANGE_EPSILON))
	{
		ActionScopes availableScopeMask = m_scopeMask & m_actionController.GetActiveScopeMask();
		const uint32 numActions = m_actions.size();
		for (uint32 i = 0; i < numActions; i++)
		{
			m_actions[i]->ForceFinish();
			m_actions[i]->Release();
		}
		m_actions.clear();

		const uint32 totalScopes = m_actionController.GetTotalScopes();

		CMannequinDialog* pDialog = CMannequinDialog::GetCurrentInstance();
		CMannequinModelViewport* pViewPort = pDialog->FragmentEditor()->ModelViewport();

		if (m_mode == eMEM_Previewer)
		{
			CPreviewerPage* pPreview = pDialog->Previewer();
			pPreview->OnSequenceRestart(time);
			pViewPort = pPreview->ModelViewport();
			m_actionController.Flush();
		}
		else if (m_mode == eMEM_TransitionEditor)
		{
			CTransitionEditorPage* pTransitionEditor = pDialog->TransitionEditor();
			pTransitionEditor->OnSequenceRestart(time);
			pViewPort = pTransitionEditor->ModelViewport();
			m_actionController.Flush();

			m_actionController.SetFlag(AC_NoTransitions, true);
		}

		m_actionController.ResetParams();
		UpdateDebugParams();
		pViewPort->UpdateDebugParams();

		float lastScopeUpdateTime[32];
		memset(lastScopeUpdateTime, 0, sizeof(lastScopeUpdateTime));

		uint32 idx;
		float lastUpdateTime = 0.0f;
		for (idx = 0; idx < m_history.m_items.size(); idx++)
		{
			const CFragmentHistory::SHistoryItem& item = m_history.m_items[idx];

			const float itemTime = item.time - m_history.m_firstTime;

			if (itemTime <= time)
			{
				if (idx > 0)
				{
					float incrementTime = itemTime - lastUpdateTime;

					if (incrementTime > 0.0f)
					{
						if (pViewPort)
						{
							if (MAX_TIME_STEP > 0.0f)
							{
								while (incrementTime > MAX_TIME_STEP)
								{
									pViewPort->UpdateAnimation(MAX_TIME_STEP);
									incrementTime -= MAX_TIME_STEP;
								}
							}

							pViewPort->UpdateAnimation(incrementTime);
						}
					}
				}
				lastUpdateTime = itemTime;

				switch (item.type)
				{
				case CFragmentHistory::SHistoryItem::Fragment:
					{
						ActionScopes filteredMask = (item.installedScopeMask & m_scopeMask);
						if ((filteredMask == item.installedScopeMask) && ((filteredMask & availableScopeMask) != 0))
						{
							StopPrevious(filteredMask);

							if ((m_mode == eMEM_TransitionEditor) && (item.time > 0.0f))
							{
								m_actionController.SetFlag(AC_NoTransitions, false);
							}

							CBasicAction* pAction = new CActionFragmentItem(item.fragment, item.tagState.fragmentTags, item.loadedScopeMask, item.trumpsPrevious);
							pAction->SetOptionIdx(item.fragOptionIdxSel);
							pAction->AddRef();
							m_actions.push_back(pAction);

							m_actionController.Queue(*pAction);
						}
					}
					break;

				case CFragmentHistory::SHistoryItem::Tag:
					if (m_mode == eMEM_Previewer)
					{
						CMannequinDialog::GetCurrentInstance()->Previewer()->SetTagState(item.tagState.globalTags);
					}
					else if (m_mode == eMEM_TransitionEditor)
					{
						CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetTagState(item.tagState.globalTags);
					}
					break;

				case CFragmentHistory::SHistoryItem::Param:
					m_actionController.SetParam(item.paramName, item.param.value);

					EMotionParamID paramID = MannUtils::GetMotionParam(item.paramName);
					if (paramID != eMotionParamID_COUNT)
					{
						pViewPort->LockMotionParam(paramID, item.param.value.q.v.x);
					}

					break;
				}
			}
			else
			{
				break;
			}
		}
		m_curIdx = idx;

		float incrementTime = time - lastUpdateTime;
		if (incrementTime > 0.0f)
		{
			if (pViewPort)
			{
				if (MAX_TIME_STEP > 0.0f)
				{
					while (incrementTime > MAX_TIME_STEP)
					{
						pViewPort->UpdateAnimation(MAX_TIME_STEP);
						incrementTime -= MAX_TIME_STEP;
					}
				}

				pViewPort->UpdateAnimation(incrementTime);
			}
		}

		m_time = time;
	}

	const uint32 newNumActions = m_actions.size();
	for (uint32 i = 0; i < newNumActions; i++)
	{
		m_actions[i]->SetSpeedBias(m_playScale);
	}
}

void CFragmentSequencePlayback::Update(float timePassed, CMannequinModelViewport* pViewPort)
{
	m_time += timePassed * m_playScale;

	ActionScopes availableScopeMask = m_scopeMask & m_actionController.GetActiveScopeMask();

	uint32 idx;
	for (idx = m_curIdx; idx < m_history.m_items.size(); idx++)
	{
		const CFragmentHistory::SHistoryItem& item = m_history.m_items[idx];
		const float itemTime = item.time - m_history.m_firstTime;
		if (itemTime < m_time)
		{
			switch (item.type)
			{
			case CFragmentHistory::SHistoryItem::Fragment:
				{
					ActionScopes filteredMask = (item.installedScopeMask & availableScopeMask);
					if (filteredMask != 0)
					{
						StopPrevious(filteredMask);

						if ((m_mode == eMEM_TransitionEditor) && (item.time > 0.0f))
						{
							m_actionController.SetFlag(AC_NoTransitions, false);
						}

						//--- Push on item
						CBasicAction* pAction = new CActionFragmentItem(item.fragment, item.tagState.fragmentTags, item.loadedScopeMask, item.trumpsPrevious);
						pAction->SetOptionIdx(item.fragOptionIdxSel);
						pAction->AddRef();
						pAction->SetSpeedBias(m_playScale);
						m_actions.push_back(pAction);

						m_actionController.Queue(*pAction);
					}
					break;
				}

			case CFragmentHistory::SHistoryItem::Tag:
				//--- Push on tag
				if (m_mode == eMEM_Previewer)
				{
					CMannequinDialog::GetCurrentInstance()->Previewer()->SetTagState(item.tagState.globalTags);
				}
				else if (m_mode == eMEM_TransitionEditor)
				{
					CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetTagState(item.tagState.globalTags);
				}
				break;

			case CFragmentHistory::SHistoryItem::Param:
				{
					m_actionController.SetParam(item.paramName, item.param.value);

					EMotionParamID paramID = MannUtils::GetMotionParam(item.paramName);
					if (paramID != eMotionParamID_COUNT)
					{
						pViewPort->LockMotionParam(paramID, item.param.value.q.v.x);
					}
				}
				break;
			}
		}
		else
		{
			break;
		}
	}
	m_curIdx = idx;
}

void CFragmentSequencePlayback::SetSpeedBias(float playScale)
{
	if (m_playScale != playScale)
	{
		m_playScale = playScale;

		const uint32 numActions = m_actions.size();
		for (uint32 i = 0; i < numActions; i++)
		{
			m_actions[i]->SetSpeedBias(playScale);
		}
	}
}

void CFragmentSequencePlayback::UpdateDebugParams()
{
	m_actionController.SetParam("CameraLocation", QuatT(Quat(ZERO), Vec3(0.f, 0.f, 1.5f)));
}

