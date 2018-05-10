// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id: ReplayObject.cpp$
$DateTime$
Description: A replay entity spawned during KillCam replay

-------------------------------------------------------------------------
History:
- 03/19/2010 09:15:00: Created by Martin Sherburn

*************************************************************************/

#include "StdAfx.h"
#include "ReplayObject.h"
#include "IItemSystem.h"

CReplayObject::CReplayObject()
: m_timeSinceSpawn(0)
{
}

CReplayObject::~CReplayObject()
{
}

//------------------------------------------------------------------------
bool CReplayObject::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);

	return true;
}

//------------------------------------------------------------------------
bool CReplayObject::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CReplayObject::ReloadExtension not implemented");
	
	return false;
}

//------------------------------------------------------------------------
bool CReplayObject::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CReplayObject::GetEntityPoolSignature not implemented");
	
	return true;
}

//------------------------------------------------------------------------
//- REPLAY ACTION
//------------------------------------------------------------------------

CReplayObjectAction::CReplayObjectAction( FragmentID fragID, const TagState &fragTags, uint32 optionIdx, bool trumpsPrevious, const int priority )
	: BaseClass(priority, fragID)
	, m_trumpsPrevious(trumpsPrevious)
{
	m_fragTags = fragTags;
	m_optionIdx = optionIdx;
}

EPriorityComparison CReplayObjectAction::ComparePriority( const IAction &actionCurrent ) const
{
	if (m_trumpsPrevious)
	{
		return ((IAction::Installed == actionCurrent.GetStatus() && IAction::Installing & ~actionCurrent.GetFlags()) ? Higher : BaseClass::ComparePriority(actionCurrent));
	}
	else
	{
		return Equal;
	}
}

//////////////////////////////////////////////////////////////////////////

void CReplayItemList::AddItem( const EntityId itemId )
{
	m_items.push_back(itemId);
}

void CReplayItemList::OnActionControllerDeleted()
{
	IItemSystem* pItemSys = g_pGame->GetIGameFramework()->GetIItemSystem();
	TItemVec::const_iterator end = m_items.end();
	for(TItemVec::const_iterator it = m_items.begin(); it!=end; ++it)
	{
		if(IItem* pItem = pItemSys->GetItem(*it))
		{
			pItem->UpdateCurrentActionController();
		}
	}

	m_items.clear();

}
