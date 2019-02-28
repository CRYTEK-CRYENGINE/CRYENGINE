// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Script Binding for Item
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ITEM_H__
#define __SCRIPTBIND_ITEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>


struct IItemSystem;
struct IGameFramework;
class CItem;
class CActor;


class CScriptBind_Item :
	public CScriptableBase
{
public:
	CScriptBind_Item(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Item();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	void AttachTo(CItem *pItem);

	int Reset(IFunctionHandler *pH);

	int CanPickUp(IFunctionHandler *pH, ScriptHandle userId);
	int CanUse(IFunctionHandler *pH, ScriptHandle userId);
	int CanUseVehicle(IFunctionHandler *pH, ScriptHandle userId);
	int IsPickable(IFunctionHandler *pH);
	int IsMounted(IFunctionHandler *pH);
	int GetUsableText(IFunctionHandler *pH);

	int GetOwnerId(IFunctionHandler *pH);
	int StartUse(IFunctionHandler *pH, ScriptHandle userId);
	int StopUse(IFunctionHandler *pH, ScriptHandle userId);
	int Use(IFunctionHandler *pH, ScriptHandle userId);
	int IsUsed(IFunctionHandler *pH);
	int OnUsed(IFunctionHandler *pH, ScriptHandle userId);

	int GetMountedDir(IFunctionHandler *pH);
	int SetMountedAngleLimits(IFunctionHandler *pH, float min_pitch, float max_pitch, float yaw_range);

  int OnHit(IFunctionHandler *pH, SmartScriptTable hitTable);
  int IsDestroyed(IFunctionHandler *pH);

	int HasAccessory(IFunctionHandler *pH, const char* accessoryName);

	int AllowDrop(IFunctionHandler *pH);
	int DisallowDrop(IFunctionHandler *pH);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CItem *GetItem(IFunctionHandler *pH);
	CActor *GetActor(EntityId actorId);

	ISystem						*m_pSystem;
	IGameFramework		*m_pGameFW;

	SmartScriptTable	m_stats;
	SmartScriptTable	m_params;
};


#endif //__SCRIPTBIND_ITEM_H__