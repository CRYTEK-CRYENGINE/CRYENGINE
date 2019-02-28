// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Script Binding for HUD
  
 -------------------------------------------------------------------------
  History:
  - 14:02:2006   11:30 : Created by AlexL
	- 04:04:2006	 17:30 : Extended by Jan Müller

*************************************************************************/
#ifndef __SCRIPTBIND_HUD_H__
#define __SCRIPTBIND_HUD_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>

struct IGameFramework;

class CScriptBind_HUD :
	public CScriptableBase
{
public:
	CScriptBind_HUD(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_HUD();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

protected:
	int SetObjectiveStatus(IFunctionHandler *pH,const char* pObjectiveID, int status, bool silent);
	int SetObjectiveEntity(IFunctionHandler *pH,const char* pObjectiveID,const char* entityName);
	int ClearObjectiveEntity(IFunctionHandler *pH,const char* pObjectiveID);
	int AddEntityToRadar(IFunctionHandler *pH, ScriptHandle entityId);
	
	int RemoveEntityFromRadar(IFunctionHandler *pH, ScriptHandle entityId);
	int AddEntitySilhouette(IFunctionHandler *pH, ScriptHandle entityId, float r, float g, float b, float a);
	
	int OnGameStatusUpdate(IFunctionHandler *pH, int goodBad, const char *msg);
	int RemoveObjective(IFunctionHandler *pH, ScriptHandle entityId);


private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem						*m_pSystem;
	IGameFramework		*m_pGameFW;
};

#endif //__SCRIPTBIND_HUD_H__
