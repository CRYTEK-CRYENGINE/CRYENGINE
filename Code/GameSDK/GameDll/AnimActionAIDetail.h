// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIM_ACTION_AI_DETAIL__
#define __ANIM_ACTION_AI_DETAIL__

#include "ICryMannequin.h"
#include "FragmentVariationHelper.h"

////////////////////////////////////////////////////////////////////////////
class CAnimActionAIDetail : public TAction<SAnimationContext>
{
	friend bool mannequin::UpdateFragmentVariation<CAnimActionAIDetail>(class CAnimActionAIDetail* pAction, class CFragmentVariationHelper* pFragmentVariationHelper, const FragmentID fragmentID, const TagState& requestedFragTags, const bool forceUpdate, const bool trumpSelf);

public:
	typedef TAction<SAnimationContext> TBase;

	DEFINE_ACTION("AIDetail");

	enum EMovementDetail
	{
		None,
		Idle,
		Move,
		Turn,
	};

	CAnimActionAIDetail();

	// -- IAction Implementation ------------------------------------------------
	virtual EPriorityComparison ComparePriority(const IAction &actionCurrent) const override
	{
		return (IAction::Installed == actionCurrent.GetStatus() && IAction::Installing & ~actionCurrent.GetFlags()) ? Higher : TBase::ComparePriority(actionCurrent);
	}

	virtual void Enter() override;
	virtual void Exit() override;

	virtual void OnInitialise() override;
	virtual EStatus Update(float timePassed) override;
	virtual EStatus	UpdatePending(float timePassed) override;
	virtual void OnSequenceFinished(int layer, uint32 scopeID) override;
	// -- ~IAction Implementation -----------------------------------------------

	void RequestDetail(EMovementDetail detail);

	static bool IsSupported( const SAnimationContext& context );

private:
	void UpdateFragmentVariation(bool forceUpdate = false);

private:
	const struct SMannequinAIDetailParams* m_pManParams;

	CFragmentVariationHelper m_fragmentVariationHelper;

	FragmentID m_requestedDetail;
};


#endif //__ANIM_ACTION_AI_DETAIL__