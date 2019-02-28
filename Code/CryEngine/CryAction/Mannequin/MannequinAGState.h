// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//-------------------------------------------------------------------------
//
// Description:
//  Adapter class that looks like an IAnimationGraphState but uses CryMannequin.
//
//  Only implements a small subset of IAnimationGraphState:
//  - Action & Signal inputs get treated as fragment IDs
//  - All other inputs are ignored (SetInput returns false)
//  - No Hurry() support
//  - No ExactPositioning support (this is now a separate component
//    ExactPositioning.cpp/.h, only dependent on IAnimatedCharacter)
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MANNEQUINAGSTATE_H__
#define __MANNEQUINAGSTATE_H__
#pragma once

#include "IAnimationGraph.h"
#include "IAnimatedCharacter.h"
#include "ICryMannequin.h"
#include "Mannequin/MannequinAGDefs.h"
#include "Mannequin/AnimActionTriState.h"

namespace MannequinAG
{

// ============================================================================
// ============================================================================

#ifndef _RELEASE
	#define ASSERT_NEVERCALLED(x) \
	  CRY_ASSERT(false);          \
	  AssertFunctionNeverCalled( # x);
#else
	#define ASSERT_NEVERCALLED(x)
#endif

// ============================================================================
// ============================================================================

//------------------------------------------------------------------------------
class CMannequinAGState;
class CAnimActionAGAction : public CAnimActionTriState
{
public:
	typedef CAnimActionTriState TBase;

	DEFINE_ACTION("AGAction");

public:
	CAnimActionAGAction(int priority, FragmentID fragmentID, CMannequinAGState& mannequinAGState, EAIActionType type, const string& value, uint32 valueCRC, TAnimationGraphQueryID queryID, bool skipIntro = false);

public:
	void                         AddLeaveQueryID(TAnimationGraphQueryID leaveQueryID);
	ILINE void                   SetQueryID(TAnimationGraphQueryID queryID) { CRY_ASSERT(m_queryID == 0); m_queryID = queryID; }
	ILINE EAIActionType          GetType() const                            { return m_type; }
	ILINE const string&          GetValue() const                           { return m_value; }
	ILINE uint32                 GetValueCRC() const                        { return m_valueCRC; }
	ILINE TAnimationGraphQueryID GetQueryID()                               { return m_queryID; }
	ILINE bool                   IsPendingOrInstalled() const               { return ((m_eStatus == IAction::Pending) || (m_eStatus == IAction::Installed)); }

private:
	// CAnimActionTriState implementation ---------------------------------------
	virtual void OnEnter() override;
	virtual void OnExit() override;
	// ~CAnimActionTriState implementation --------------------------------------

private:
	EAIActionType                            m_type;
	string                                   m_value;
	uint32                                   m_valueCRC;
	TAnimationGraphQueryID                   m_queryID;
	CryFixedArray<TAnimationGraphQueryID, 4> m_leaveQueryIDs;
	CMannequinAGState&                       m_mannequinAGState;
};

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
class CMannequinAGState : public IAnimationGraphState
{
public:
	friend class CAnimActionAGAction;

	typedef IAnimationGraphState::InputID InputID;

public:
	CMannequinAGState();
	~CMannequinAGState();

	// --------------------------------- IAnimationGraphState implementation ----

	virtual bool SetInput(InputID, float, TAnimationGraphQueryID* pQueryID = 0) { ASSERT_NEVERCALLED(SetInput_Float); return false; }
	virtual bool SetInput(InputID, int, TAnimationGraphQueryID* pQueryID = 0)   { ASSERT_NEVERCALLED(SetInput_Int); return false; }
	virtual bool SetInput(InputID, const char*, TAnimationGraphQueryID * pQueryID = 0);
	// SetInputOptional is same as SetInput except that it will not set the default input value in case a non-existing value is passed
	virtual bool SetInputOptional(InputID, const char*, TAnimationGraphQueryID * pQueryID = 0);
	virtual void        ClearInput(InputID)                                    { ASSERT_NEVERCALLED(ClearInput); };
	virtual void        LockInput(InputID, bool locked)                        { ASSERT_NEVERCALLED(LockInput); }

	virtual bool        SetVariationInput(const char* name, const char* value) { return true; }
	virtual bool        SetVariationInput(InputID inputID, const char* value)  { return true; }
	virtual const char* GetVariationInput(InputID inputID) const               { ASSERT_NEVERCALLED(GetVariationInput); return ""; }

	// assert all equal, use any (except if signalled, then return the one not equal to default, or return default of all default)
	virtual void GetInput(InputID, char*) const;

	virtual void GetInput(InputID, char*, int layerIndex) const;

	virtual bool IsDefaultInputValue(InputID) const;

	// returns NULL if InputID is out of range
	virtual const char* GetInputName(InputID) const;
	virtual const char* GetVariationInputName(InputID) const;

	virtual void QueryLeaveState(TAnimationGraphQueryID* pQueryID);

	virtual void QueryChangeInput(InputID, TAnimationGraphQueryID*);

	virtual void AddListener(const char* name, IAnimationGraphStateListener* pListener);
	virtual void RemoveListener(IAnimationGraphStateListener* pListener);

	virtual bool DoesInputMatchState(InputID) const { ASSERT_NEVERCALLED(DoesInputMatchState); return false; }

	// Only set for fullbody, null for upperbody.
	virtual void SetAnimatedCharacter(class CAnimatedCharacter* animatedCharacter, int layerIndex, IAnimationGraphState* parentLayerState);

	virtual bool        Update() { return true; }
	virtual void        Release();
	virtual void        ForceTeleportToQueriedState();

	virtual void        PushForcedState(const char* state, TAnimationGraphQueryID* pQueryID = 0) { ASSERT_NEVERCALLED(PushForcedState); }
	virtual void        ClearForcedStates()                                                      { ASSERT_NEVERCALLED(ClearForcedStates); }

	virtual float       GetInputAsFloat(InputID inputId)                                         { ASSERT_NEVERCALLED(GetInputAsFloat); return 0.0f; }

	virtual InputID     GetInputId(const char* input);
	virtual InputID     GetVariationInputId(const char* variationInputName) const;

	virtual void        Serialize(TSerialize ser);

	virtual void        SetAnimationActivation(bool activated) { ASSERT_NEVERCALLED(SetAnimationActivation); };
	virtual bool        GetAnimationActivation()               { ASSERT_NEVERCALLED(GetAnimationActivation); return false; };

	virtual const char* GetCurrentStateName();

	virtual void        Pause(bool pause, EAnimationGraphPauser pauser, float fOverrideTransTime = -1.0f);

	virtual bool        IsInDebugBreak()              { ASSERT_NEVERCALLED(IsInDebugBreak); return false; };

	virtual const char* QueryOutput(const char* name) { ASSERT_NEVERCALLED(QueryOutput); return NULL; };

	// Exact positioning
	virtual IAnimationSpacialTrigger* SetTrigger(const SAnimationTargetRequest& req, EAnimationGraphTriggerUser user, TAnimationGraphQueryID* pQueryStart, TAnimationGraphQueryID* pQueryEnd) { ASSERT_NEVERCALLED(SetTrigger); return NULL; }
	virtual void                      ClearTrigger(EAnimationGraphTriggerUser user)                                                                                                           { ASSERT_NEVERCALLED(ClearTrigger); }
	virtual const SAnimationTarget*   GetAnimationTarget()                                                                                                                                    { return NULL; }
	virtual bool                      HasAnimationTarget() const                                                                                                                              { ASSERT_NEVERCALLED(HasAnimationTarget); return false; }
	virtual bool                      IsUpdateReallyNecessary()                                                                                                                               { ASSERT_NEVERCALLED(IsUpdateReallyNecessary); return false; }

	// Creates an object you can use to test whether a specific combination of inputs will select a state
	// (and to get a bit more information about that state)
	virtual IAnimationGraphExistanceQuery* CreateExistanceQuery();
	virtual IAnimationGraphExistanceQuery* CreateExistanceQuery(int layer);

	virtual void                           Reset(); // note: Reset will properly send ChangedInput values while resetting inputs
	virtual void                           Hurry();
	virtual void                           SetFirstPersonMode(bool on) {};

	// simply recurse (will add all layer's containers to the sizer)
	virtual void GetMemoryUsage(ICrySizer* pSizer) const;

	virtual bool IsMixingAllowedForCurrentState() const { return true; }

	virtual bool IsSignalledInput(InputID inputId) const;

	// --------------------------------- ~IAnimationGraphState implementation ---

	void OnReload();

private:
	bool               SetInputInternal(InputID iid, const char* value, TAnimationGraphQueryID* pQueryID, bool optional);
	ILINE bool         IsPaused() const { return (m_pauseState != 0); }
	bool               SetActionOrSignalInput(_smart_ptr<CAnimActionAGAction>& pAction, TKeyValue& currentValue, InputID inputID, EAIActionType actionType, const char* defaultValue, uint32 defaultValueCRC, const char* value, TAnimationGraphQueryID* pQueryID, bool optional);
	void               SendEvent_ChangedInput(InputID iid, bool success);
	void               SendEvent_Entered(_smart_ptr<CAnimActionAGAction> pCaller, TAnimationGraphQueryID queryID, bool success); // should ONLY be called by the CAnimActionAGAction
	void               SendEvent_Left(TAnimationGraphQueryID queryID, bool success);
	void               ResetInputValues(); // note: ResetInputs will properly send ChangedInput events
	IActionController* GetActionController();
	void               StopAnyLoopingExactPositioningAction();

private:
	static TAnimationGraphQueryID GenerateQueryID() { ++s_lastQueryID; CRY_ASSERT(s_lastQueryID); return s_lastQueryID; }
	static void                   AssertFunctionNeverCalled(const char* func)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMannequinAGState: Function IAnimationGraphState::%s called while we assume it was never called", func);
	}

private:
	// Input values
	TKeyValue m_actionValue;
	TKeyValue m_signalValue;

	// Pausing
	uint32 m_pauseState;

	// Listeners
	struct SListener
	{
		IAnimationGraphStateListener* pListener;
		char                          name[16 - sizeof(IAnimationGraphStateListener*)];

		bool operator==(const SListener& other) const
		{
			return pListener == other.pListener;
		}
		void GetMemoryUsage(ICrySizer* pSizer) const {}
	};

	std::vector<SListener> m_listeners;
	std::vector<SListener> m_callingListeners;
	TAnimationGraphQueryID m_queryChangedInputIDs[eSIID_COUNT];

	// Anim Actions
	_smart_ptr<CAnimActionAGAction> m_pLoopingAction;
	_smart_ptr<CAnimActionAGAction> m_pOneShotAction;
	_smart_ptr<CAnimActionAGAction> m_pCallerOfSendEvent; // only used to figure out within QueryLeaveState which action we are inside

	// AC
	CAnimatedCharacter* m_pAnimatedCharacter;

private:
	static TAnimationGraphQueryID s_lastQueryID;
};

// ============================================================================
// ============================================================================

} // namespace MannequinAG

#endif // __MANNEQUINAGSTATE_H__
