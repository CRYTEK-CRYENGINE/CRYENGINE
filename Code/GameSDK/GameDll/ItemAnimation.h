// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: Helper functionality for item animation control

-------------------------------------------------------------------------
History:
- 11.11.11: Created by Tom Berry

*************************************************************************/

#ifndef __ITEMANIMATION_H__
#define __ITEMANIMATION_H__

#include "ICryMannequin.h"

//--- TODO: Add generic itemSystem-specific fragmentIDs & tags here

#define MAN_ITEM_FRAGMENTS( x )	\
	x( change_firemode ) \
	x( unholster_select )	\
	x( Select )	\
	x( deselect ) \
	x( unprime ) \
	x( cock ) \
	x( empty_clip ) \
	x( pre_fire ) \
	x( fire ) \
	x( fire_cock ) \
	x( shot_last_bullet ) \
	x( burst_fire ) \
	x( stop_rapid_fire ) \
	x( spin_up ) \
	x( spin_down ) \
	x( spin_down_tail ) \
	x( charge ) \
	x( uncharge ) \
	x( hold ) \
	x( prime ) \
	x( primed_loop ) \
	x( Throw ) \
	x( fromPlant ) \
	x( plant ) \
	x( intoPlant ) \
	x( rip_off ) \
	x( enter_modify ) \
	x( leave_modify ) \
	x( destroy ) \
	x( pickedup ) \
	x( pickedup_ammo ) \
	x( friendly_enter ) \
	x( friendly_leave ) \
	x( weapon_lower_enter ) \
	x( weapon_lower_leave ) \
	x( meleeReaction ) \
	x( begin_reload ) \
	x( reload_shell ) \
	x( idle_lastGrenade ) \
	x( use_light ) \
	x( turret ) \
	x( cannon ) \
	x( lock ) \
	x( activate ) \
	x( deactivate ) \
	x( drop ) \
	x( zoom_in ) \
	x( zoom_out ) \
	x( barrel_spin )

#define MAN_ITEM_TAGS( x ) \
	x( shoulder ) \
	x( weaponMounted ) \
	x( weaponDetached )

#define MAN_ITEM_TAGGROUPS( x ) \
	x( scope_attachment ) \
	x( barrel_attachment ) \
	x( underbarrel_attachment ) \
	x( mountedWeapons )

#define MAN_ITEM_SCOPES( x )

#define MAN_ITEM_CONTEXTS( x ) \
	x( Weapon ) \
	x( attachment_top ) \
	x( attachment_bottom )

#define MAN_ITEM_SELECT_FRAGMENT_TAGS( x ) \
	x( special_first ) \
	x( first ) \
	x( fast_select ) \
	x( primary ) \
	x( secondary )

#define MAN_ITEM_FRAGMENT_TAGS( x ) \
	x( Select, MAN_ITEM_SELECT_FRAGMENT_TAGS, MANNEQUIN_USER_PARAMS__EMPTY_LIST )

MANNEQUIN_USER_PARAMS( SMannequinItemParams, MAN_ITEM_FRAGMENTS, MAN_ITEM_TAGS, MAN_ITEM_TAGGROUPS, MAN_ITEM_SCOPES, MAN_ITEM_CONTEXTS, MAN_ITEM_FRAGMENT_TAGS );

class CPlayer;
class CItem;

class CItemAction : public TAction<SAnimationContext>
{
public:
	DEFINE_ACTION("ItemAction");

	CItemAction( int priority, FragmentID fragmentID, const TagState &fragTags, const int flags = 0 )
		:	BaseClass(priority, fragmentID, fragTags, flags)	{	}

	virtual EPriorityComparison ComparePriority(const IAction &actionCurrent) const override
	{
		return (IAction::Installed == actionCurrent.GetStatus() && IAction::Installing & ~actionCurrent.GetFlags()) ? Higher : BaseClass::ComparePriority(actionCurrent);
	}

private:
	typedef TAction<SAnimationContext> BaseClass;
};


class CItemSelectAction : public CItemAction
{
public:
	DEFINE_ACTION("ItemSelectAction");

	CItemSelectAction( int priority, FragmentID fragmentID, const TagState &fragTags, CItem& _item);

	virtual void Enter() override;
	virtual void Exit() override;
	virtual void OnAnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event) override;

	void ItemSelectCancelled();

protected:
	void SelectWeapon();
	void UnhideWeapon(CItem* pItem);

	EntityId m_ItemID;
	bool m_bSelected;

private:
	typedef CItemAction BaseClass;
};

class CActionItemIdle : public TAction<SAnimationContext>
{
public:

	DEFINE_ACTION("ItemIdle");

	typedef TAction<SAnimationContext> BaseClass;

	CActionItemIdle(int priority, FragmentID fragmentID, FragmentID idleBreakFragmentID, const TagState fragTags, CPlayer& playerRef);

	virtual EStatus Update(float timePassed) override;
	virtual void OnInitialise() override;

	virtual void OnSequenceFinished(int layer, uint32 scopeID) override;

private:
	void UpdateFragmentTags();

	CPlayer& m_ownerPlayer;

	FragmentID m_fragmentIdle;
	FragmentID m_fragmentIdleBreak;
	float			 m_lastIdleBreakTime;
	bool			 m_playingIdleBreak;
};


#endif //!__ITEMANIMATION_H__
