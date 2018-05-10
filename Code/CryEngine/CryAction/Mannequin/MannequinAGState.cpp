// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "MannequinAGState.h"
#include "CryAction.h"
#include "AnimationGraph/AnimatedCharacter.h"
#include "MannequinAGExistanceQuery.h"

// TODO: Go over all calls to AG_SEND_EVENT in AnimationGraphState
// TODO: Check whether the Release() & dtor is always called, because animchar has a rather 'interesting' way of calling Release()

// ============================================================================
// ============================================================================

namespace MannequinAG
{

// ============================================================================
// ============================================================================

#define AG_SEND_EVENT(func)                                                                                                                                       \
  {                                                                                                                                                               \
    m_callingListeners = m_listeners;                                                                                                                             \
    for (std::vector<SListener>::reverse_iterator agSendEventIter = m_callingListeners.rbegin(); agSendEventIter != m_callingListeners.rend(); ++agSendEventIter) \
      agSendEventIter->pListener->func;                                                                                                                           \
  }

// ============================================================================
// ============================================================================

TAnimationGraphQueryID CMannequinAGState::s_lastQueryID = TAnimationGraphQueryID(0);

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
CMannequinAGState::CMannequinAGState()
	:
	m_pauseState(0),
	m_pAnimatedCharacter(NULL)
{
	for (size_t i = 0; i < eSIID_COUNT; i++)
	{
		m_queryChangedInputIDs[i] = 0;
	}

	ResetInputValues();
}

// ----------------------------------------------------------------------------
CMannequinAGState::~CMannequinAGState()
{
	AG_SEND_EVENT(DestroyedState(this));

	Reset();
}

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
void CMannequinAGState::SetAnimatedCharacter(class CAnimatedCharacter* animatedCharacter, int layerIndex, IAnimationGraphState* parentLayerState)
{
	// TODO: What to do with the controller?

	m_pAnimatedCharacter = animatedCharacter;
}

// ----------------------------------------------------------------------------
bool CMannequinAGState::SetInput(InputID iid, const char* value, TAnimationGraphQueryID* pQueryID /*= 0 */)
{
	return SetInputInternal(iid, value, pQueryID, false);
}

// ----------------------------------------------------------------------------
bool CMannequinAGState::SetInputOptional(InputID iid, const char* value, TAnimationGraphQueryID* pQueryID /*= 0 */)
{
	return SetInputInternal(iid, value, pQueryID, true);
}

// ----------------------------------------------------------------------------
void CMannequinAGState::GetInput(InputID iid, char* value) const
{
	GetInput(iid, value, 0);
}

// ----------------------------------------------------------------------------
void CMannequinAGState::GetInput(InputID iid, char* value, int layerIndex) const
{
	value[0] = '\0';

	CRY_ASSERT(layerIndex == 0);
	if (layerIndex != 0)
		return;

	switch (iid)
	{
	case eSIID_Action:
		strcpy(value, m_actionValue.c_str());
		break;
	case eSIID_Signal:
		strcpy(value, m_signalValue.c_str());
		break;
	default:
		break;
	}
	;
}

// ----------------------------------------------------------------------------
bool CMannequinAGState::IsDefaultInputValue(InputID iid) const
{
	switch (iid)
	{
	case eSIID_Action:
		return (m_actionValue == "idle");
	case eSIID_Signal:
		// Can be called from within CAnimationGraphStates when QueryComplete is called
		return (m_signalValue == "none");
	default:
		return true;
	}
	;
}

// ----------------------------------------------------------------------------
const char* CMannequinAGState::GetInputName(InputID iid) const
{
	// Only called from CAnimationGraphStates::RebindInputs
	switch (iid)
	{
	case eSIID_Action:
		return "Action";
	case eSIID_Signal:
		return "Signal";
	default:
		return NULL;
	}
	;
}

// ----------------------------------------------------------------------------
const char* CMannequinAGState::GetVariationInputName(InputID) const
{
	// Only called from CAnimationGraphStates::RebindInputs
	return NULL;
}

// ----------------------------------------------------------------------------
// When QueryID of SetInput (reached queried state) is emitted this function is called by the outside, by convention(verify!).
// Remember which layers supported the SetInput query and emit QueryLeaveState QueryComplete when all those layers have left those states.
void CMannequinAGState::QueryLeaveState(TAnimationGraphQueryID* pQueryID)
{
	_smart_ptr<CAnimActionAGAction> pCallerAction = m_pCallerOfSendEvent;

	// If this was not called from within an action, which is not really supported by the AG
	// we use some random heuristics to try to figure out which action to use as 'callerAction'
	if (!pCallerAction)
	{
		if (m_pOneShotAction && m_pOneShotAction->IsPendingOrInstalled())
		{
			pCallerAction = m_pOneShotAction;
		}
		else if (m_pLoopingAction && m_pLoopingAction->IsPendingOrInstalled())
		{
			pCallerAction = m_pLoopingAction;
		}
	}

	*pQueryID = GenerateQueryID();

	if (!pCallerAction)
	{
		CRY_ASSERT(false);
		// I considered sending the event here, but as the AG didn't call this event
		// directly, I cannot assume the calling code is written in a way that can
		// safely handle this case.
		// AG_SEND_EVENT(QueryComplete(*pQueryID, false));
	}
	else
	{
		pCallerAction->AddLeaveQueryID(*pQueryID);
	}
}

// ----------------------------------------------------------------------------
// assert all equal, forward to all layers, complete when all have changed once (trivial, since all change at once via SetInput).
// (except for signalled, forward only to layers which currently are not default, complete when all those have changed).
void CMannequinAGState::QueryChangeInput(InputID iid, TAnimationGraphQueryID* pQuery)
{
	CRY_ASSERT(pQuery);
	*pQuery = 0;
	if (iid >= eSIID_COUNT)
		return;
	m_queryChangedInputIDs[iid] = *pQuery = GenerateQueryID();
}

// ----------------------------------------------------------------------------
void CMannequinAGState::AddListener(const char* name, IAnimationGraphStateListener* pListener)
{
	SListener listener;
	cry_strcpy(listener.name, name);
	listener.pListener = pListener;
	stl::push_back_unique(m_listeners, listener);
}

// ----------------------------------------------------------------------------
void CMannequinAGState::RemoveListener(IAnimationGraphStateListener* pListener)
{
	SListener listener;
	listener.name[0] = 0;
	listener.pListener = pListener;
	stl::find_and_erase(m_listeners, listener);
}

// ----------------------------------------------------------------------------
void CMannequinAGState::Release()
{
	delete this;
}

// ----------------------------------------------------------------------------
void CMannequinAGState::ForceTeleportToQueriedState()
{
	Hurry();
}

// ----------------------------------------------------------------------------
AnimationGraphInputID CMannequinAGState::GetInputId(const char* input)
{
	if (0 == strcmp(input, "Action"))
		return eSIID_Action;
	else if (0 == strcmp(input, "Signal"))
		return eSIID_Signal;
	else
	{
		CRY_ASSERT(false);
		return InputID(-1);
	}
}

// ----------------------------------------------------------------------------
AnimationGraphInputID CMannequinAGState::GetVariationInputId(const char* variationInputName) const
{
	CRY_ASSERT(false);
	return InputID(-1);
}

// ----------------------------------------------------------------------------
void CMannequinAGState::Serialize(TSerialize ser)
{
	// TODO: Serialize?  What about our back-pointers?

	if (ser.GetSerializationTarget() == eST_SaveGame)
	{
		if (ser.IsReading())
			Reset();

		//ser.Value("action", m_actionValue);
		//ser.Value("signal", m_signalValue);

		// TODO: Start actions based on the serialized value?  Or just not serialize at all?
	}
}

// ----------------------------------------------------------------------------
const char* CMannequinAGState::GetCurrentStateName()
{
	const char* result = "<unknown>";

	// Figure out something which could pose as a 'state name'
	// Currently it's the name of the first fragmentID I can find on the scopes
	IActionController* pActionController = GetActionController();
	if (!pActionController)
		return result;

	for (int scopeIndex = 0; scopeIndex < pActionController->GetTotalScopes(); ++scopeIndex)
	{
		IScope* pScope = pActionController->GetScope(scopeIndex);
		FragmentID fragmentID = pScope->GetLastFragmentID();
		if (fragmentID != FRAGMENT_ID_INVALID)
		{
			result = pActionController->GetContext().controllerDef.m_fragmentIDs.GetTagName(fragmentID);
			break;
		}
	}

	return result;
}

// ----------------------------------------------------------------------------
void CMannequinAGState::Pause(bool pause, EAnimationGraphPauser pauser, float fOverrideTransTime /*= -1.0f */)
{
	bool bWasPaused = IsPaused();

	if (pause)
		m_pauseState |= (1 << pauser);
	else
		m_pauseState &= ~(1 << pauser);

	if (bWasPaused != IsPaused())
	{
		// TODO: do something here?
	}
}

// ----------------------------------------------------------------------------
IAnimationGraphExistanceQuery* CMannequinAGState::CreateExistanceQuery()
{
	return new CMannequinAGExistanceQuery(this);
}

// ----------------------------------------------------------------------------
IAnimationGraphExistanceQuery* CMannequinAGState::CreateExistanceQuery(int layer)
{
	CRY_ASSERT(layer == 0);
	if (layer != 0)
		return NULL;

	return CreateExistanceQuery();
}

// ----------------------------------------------------------------------------
void CMannequinAGState::Reset()
{
	// TODO: what exactly do we want to 'reset'?
	//   This is called from the outside AND from within our destructor
	//m_pLoopingAction = NULL;
	//m_pOneShotAction = NULL;

	ResetInputValues(); // TODO: don't call from dtor?
}

// ----------------------------------------------------------------------------
void CMannequinAGState::Hurry()
{
	// first check whether we have anything that is still pending, if not, we have nothing to hurry
	const bool oneShotActionIsPending = (m_pOneShotAction && (m_pOneShotAction->GetStatus() == IAction::Pending));
	const bool loopingActionIsPending = (m_pLoopingAction && (m_pLoopingAction->GetStatus() == IAction::Pending));

	if (!oneShotActionIsPending && !loopingActionIsPending)
		return;

	IActionController* pActionController = GetActionController();
	if (!pActionController)
		return;

	// TODO: How do we mimic Hurry?  Somehow we need to, in a clean way, end all actions on all scopes
	//   (without screwing up the pending ones, properly moving interruptable ones to the queue,
	//    calling Exit() when needed, etc)
}

// ----------------------------------------------------------------------------
void CMannequinAGState::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	s->AddObject(m_listeners);
	s->AddObject(m_callingListeners);
}

// ----------------------------------------------------------------------------
bool CMannequinAGState::IsSignalledInput(InputID iid) const
{
	return (iid == eSIID_Signal);
}

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
bool CMannequinAGState::SetInputInternal(InputID iid, const char* value, TAnimationGraphQueryID* pQueryID, bool optional)
{
	switch (iid)
	{
	case eSIID_Action:
		{
			static const uint32 idleCRC = CCrc32::ComputeLowercase("idle");
			return SetActionOrSignalInput(m_pLoopingAction, m_actionValue, iid, EAT_Looping, "idle", idleCRC, value, pQueryID, optional);
		}
	case eSIID_Signal:
		{
			static const uint32 noneCRC = CCrc32::ComputeLowercase("none");
			return SetActionOrSignalInput(m_pOneShotAction, m_signalValue, iid, EAT_OneShot, "none", noneCRC, value, pQueryID, optional);
		}
	case InputID(-1):
		{
			return false;
		}
	default:
		{
			CRY_ASSERT(false); // trying to set input with non-existing input id
			return false;
		}
	}
	;
}

// ----------------------------------------------------------------------------
bool CMannequinAGState::SetActionOrSignalInput(_smart_ptr<CAnimActionAGAction>& pAction, TKeyValue& currentValue, InputID inputID, EAIActionType actionType, const char* defaultValue, uint32 defaultValueCRC, const char* value, TAnimationGraphQueryID* pQueryID, bool optional)
{
	IActionController* pActionController = GetActionController();
	if (!pActionController)
		return false;

	// TODO Where to get the priorities from?
	const int MANNEQUIN_PRIORITY = 2; // currently equal to PP_Action, just above movement, but underneath urgent actions, hit death, etc.

	TAnimationGraphQueryID queryID = 0;
	if (pQueryID)
		queryID = *pQueryID = GenerateQueryID();

	const uint32 valueCRC = CCrc32::ComputeLowercase(value);

	static uint32 idleCRC = CCrc32::ComputeLowercase("idle");
	static uint32 noneCRC = CCrc32::ComputeLowercase("none");
	const bool isUnsupportedFragmentID = ((valueCRC == idleCRC) || (valueCRC == noneCRC));
	const FragmentID fragmentID = isUnsupportedFragmentID ? FRAGMENT_ID_INVALID : pActionController->GetContext().controllerDef.m_fragmentIDs.Find(valueCRC);

	const bool isDefaultValue = (valueCRC == defaultValueCRC);

	const bool somethingIsPendingOrInstalled = pAction && pAction->IsPendingOrInstalled();

	if (isDefaultValue)
	{
		if (actionType == EAT_Looping)
		{
			StopAnyLoopingExactPositioningAction();
		}

		const bool isPendingOrInstalled = !somethingIsPendingOrInstalled; // the fake action 'idle' isPendingOrInstalled if something else is NOT pending or installed
		if (isPendingOrInstalled)
		{
			// In the old sense: the current requested 'value' did not change, we were already 'idle'
			SendEvent_Entered(NULL, queryID, true);
		}
		else
		{
			currentValue = defaultValue;

			if (pAction)
			{
				pAction->Stop(); // this will eventually call Entered(..,false) of the user that was waiting on Entered()
				pAction = NULL;
			}

			// TODO: This is not entirely correct. The Entered() event for the idle action should only
			//   be called when you exactly "go back to idle". But this is not well defined in mannequin,
			//   so I just send it right away...
			SendEvent_Entered(NULL, queryID, true);

			SendEvent_ChangedInput(inputID, true); // tell whoever is listening to the input that it changed
		}

		return true;
	}
	else if (fragmentID != FRAGMENT_ID_INVALID)
	{
		const bool isPendingOrInstalled =
		  somethingIsPendingOrInstalled &&
		  (pAction->GetValueCRC() == valueCRC);
		if (isPendingOrInstalled)
		{
			// In the old sense: the current requested 'value' did not change

			// reuse the old queryid, if there is one, otherwise set the new one
			if (pAction->GetQueryID())
			{
				queryID = pAction->GetQueryID();
				if (pQueryID)
					*pQueryID = queryID;
			}
			else
			{
				if (queryID)
					pAction->SetQueryID(queryID);
			}

			if (pAction->GetStatus() == IAction::Installed)
			{
				// The other system is already playing the same action.
				// Send the 'entered' event immediately
				// (afai can see the AG only did this for signals for some reason??)
				SendEvent_Entered(pAction, pAction->GetQueryID(), true);
			}
		}
		else
		{
			currentValue = value;

			pAction = new CAnimActionAGAction(MANNEQUIN_PRIORITY, fragmentID, *this, actionType, value, valueCRC, queryID);
			pActionController->Queue(*pAction.get());

			SendEvent_ChangedInput(inputID, true); // tell whoever is listening the input that it changed
		}

		return true;
	}
	else if (!optional)
	{
		// 'not optional' means invalid values get translated into the default value
		// (so we always set a value, hence the setting of values is 'not optional')
		// Recurse and pass the defaultValue as value:
		return SetActionOrSignalInput(pAction, currentValue, inputID, actionType, defaultValue, defaultValueCRC, defaultValue, NULL, true);
	}
	else
	{
#ifndef _RELEASE
		if (!isUnsupportedFragmentID)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMannequinAGState::SetInput: Unable to find animation '%s'", value);
		}
#endif //#ifndef _RELEASE
		return false;
	}
}

// ----------------------------------------------------------------------------
void CMannequinAGState::SendEvent_ChangedInput(InputID iid, bool success)
{
	TAnimationGraphQueryID temp = m_queryChangedInputIDs[iid];
	if (temp)
	{
		m_queryChangedInputIDs[iid] = 0;
		AG_SEND_EVENT(QueryComplete(temp, success));
	}
}

// ----------------------------------------------------------------------------
void CMannequinAGState::SendEvent_Entered(_smart_ptr<CAnimActionAGAction> pCaller, TAnimationGraphQueryID queryID, bool success)
{
	CRY_ASSERT(m_pCallerOfSendEvent == NULL); // paranoia: check for weird re-entry case
	if (m_pCallerOfSendEvent != NULL)
		return;

	if (!queryID)
		return;

	m_pCallerOfSendEvent = pCaller;

	AG_SEND_EVENT(QueryComplete(queryID, success))

	m_pCallerOfSendEvent = NULL;
}

// ----------------------------------------------------------------------------
void CMannequinAGState::SendEvent_Left(TAnimationGraphQueryID queryID, bool success)
{
	if (!queryID)
		return;

	AG_SEND_EVENT(QueryComplete(queryID, success))
}

// ----------------------------------------------------------------------------
void CMannequinAGState::ResetInputValues()
{
	m_actionValue = "idle";
	m_signalValue = "none";

	for (size_t i = 0; i < eSIID_COUNT; i++)
	{
		SendEvent_ChangedInput(static_cast<MannequinAG::CMannequinAGState::InputID>(i), false);
	}

	// TODO: Did the actioncontroller send the proper Enter/Exit events?
}

// ----------------------------------------------------------------------------
IActionController* CMannequinAGState::GetActionController()
{
	if (!m_pAnimatedCharacter)
		return NULL;

	return m_pAnimatedCharacter->GetActionController();
}

// ----------------------------------------------------------------------------
void CMannequinAGState::StopAnyLoopingExactPositioningAction()
{
	//if ( m_pExactPositioning.get() )
	//{
	//	m_pExactPositioning->StopAnyLoopingExactPositioningAction();
	//}
	// TODO!!!!
}

// ----------------------------------------------------------------------------
void CMannequinAGState::OnReload()
{

}

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
CAnimActionAGAction::CAnimActionAGAction(int priority, FragmentID fragmentID, CMannequinAGState& mannequinAGState, EAIActionType type, const string& value, uint32 valueCRC, TAnimationGraphQueryID queryID, bool skipIntro)
	: TBase(priority, fragmentID, *mannequinAGState.m_pAnimatedCharacter, skipIntro)
	, m_type(type)
	, m_value(value)
	, m_valueCRC(valueCRC)
	, m_queryID(queryID)
	, m_mannequinAGState(mannequinAGState)
{
	InitMovementControlMethods(eMCM_AnimationHCollision, eMCM_Entity);
}

// ----------------------------------------------------------------------------
void CAnimActionAGAction::OnEnter()
{
	m_mannequinAGState.SendEvent_Entered(this, m_queryID, true);
	m_queryID = 0;
}

// ----------------------------------------------------------------------------
void CAnimActionAGAction::OnExit()
{
	for (int i = 0; i < m_leaveQueryIDs.size(); ++i)
	{
		m_mannequinAGState.SendEvent_Left(m_leaveQueryIDs[i], true);
		m_leaveQueryIDs[i] = 0;
	}
}

// ----------------------------------------------------------------------------
void CAnimActionAGAction::AddLeaveQueryID(TAnimationGraphQueryID leaveQueryID)
{
	if (m_leaveQueryIDs.isfull())
	{
		CRY_ASSERT(false);
		return;
	}

	m_leaveQueryIDs.push_back(leaveQueryID);
}

// ============================================================================
// ============================================================================

} // MannequinAG
