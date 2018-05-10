// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIM_ACTION_AI_MOVEMENT__
#define __ANIM_ACTION_AI_MOVEMENT__

#include "ICryMannequin.h"
#include "IAnimationGraph.h" // for movementcontrolmethod
#include "FragmentVariationHelper.h"
#include "AnimActionAIStance.h"
#include "AnimActionAICoverAction.h"
#include "AnimActionAIDetail.h"
#include "ActorDefinitions.h" // for SAITurnParams

////////////////////////////////////////////////////////////////////////////
struct SAnimActionAIMovementSettings
{
	SAnimActionAIMovementSettings()
	{}

	SAITurnParams turnParams;
};


////////////////////////////////////////////////////////////////////////////
class CAnimActionAIMovement : public TAction<SAnimationContext>
{
	friend bool mannequin::UpdateFragmentVariation<CAnimActionAIMovement>(class CAnimActionAIMovement* pAction, class CFragmentVariationHelper* pFragmentVariationHelper, const FragmentID fragmentID, const TagState& requestedFragTags, const bool forceUpdate, const bool trumpSelf);

public:
	typedef TAction<SAnimationContext> TBase;

	DEFINE_ACTION("AIMovement");

	enum EMoveState
	{
		eMS_None,
		eMS_Idle,
		eMS_Turn,
		eMS_TurnBig,
		eMS_Move,
		eMS_InAir,
		eMS_Count
	};

	CAnimActionAIMovement(const SAnimActionAIMovementSettings& settings);

	// -- IAction Implementation ------------------------------------------------
	virtual void Enter() override;
	virtual void Exit() override;

	virtual void OnInitialise() override;
	virtual EStatus Update(float timePassed) override;

	virtual EStatus	UpdatePending(float timePassed) override;
	virtual void OnSequenceFinished(int layer, uint32 scopeID) override;
	virtual void OnFragmentStarted() override;
	// -- ~IAction Implementation -----------------------------------------------

	// TODO: Move these requests and the storage of the actions outside of the movement action.
	void RequestStance(CPlayer& player, EStance stance, bool urgent);

	void RequestCoverBodyDirection(CPlayer& player, ECoverBodyDirection coverBodyDirection);
	void RequestCoverAction(CPlayer& player, const char *actionName);
	void CancelCoverAction();
	void CancelStanceChange();

private:
	EMoveState CalculateState();
	EMovementControlMethod CalculatePendingMCM(CPlayer& player) const;
	bool IsInCoverStance() const;
	bool SetState(const EMoveState newMoveState); // returns true when state changed
	bool IsAnimTargetForcingMoveState(CPlayer& player) const;
	void RequestMovementDetail(CPlayer& player);
	void ClearMovementDetail(CPlayer& player);
	bool UpdateFragmentVariation(const bool forceReevaluate, const bool trumpSelf = true);
	void SetMovementControlMethod(CPlayer& player);
	void ResetMovementControlMethod(CPlayer& player);
	CPlayer& GetPlayer() const;
private:

	EMoveState m_moveState;
	EMoveState m_installedMoveState;
	struct SStateInfo
	{
		SStateInfo()
			: m_fragmentID(FRAGMENT_ID_INVALID),
				m_MCM(eMCM_Undefined),
				m_movementDetail(CAnimActionAIDetail::None)
		{
		}

		FragmentID m_fragmentID;
		EMovementControlMethod m_MCM;
		enum CAnimActionAIDetail::EMovementDetail m_movementDetail;
	};
	SStateInfo m_moveStateInfo[eMS_Count];

	CTimeValue m_deviatedOrientationTime; // time at which the angle deviation was bigger than a certain threshold angle
	const SAnimActionAIMovementSettings& m_settings;

	const struct SMannequinAIMovementParams* m_pManParams;

	_smart_ptr<CAnimActionAIStance> m_pAnimActionAIStance;
	_smart_ptr<CAnimActionAICoverAction> m_pAnimActionAICoverAction;
	_smart_ptr<CAnimActionAIChangeCoverBodyDirection> m_pAnimActionAIChangeCoverBodyDirection;

	CFragmentVariationHelper m_fragmentVariationHelper;
};


#endif //__ANIM_ACTION_AI_MOVEMENT__
