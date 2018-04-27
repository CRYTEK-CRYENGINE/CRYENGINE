// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - Created by Marco Corbetta

*************************************************************************/

#include "StdAfx.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "WorldBuilder.h"

//////////////////////////////////////////////////////////////////////////
CWorldBuilder::CWorldBuilder()
{
	if(gEnv->pGameFramework)
	{
		CRY_ASSERT_MESSAGE(gEnv->pGameFramework->GetILevelSystem(), "Unable to register as levelsystem listener!");
		if(gEnv->pGameFramework->GetILevelSystem())
		{
			gEnv->pGameFramework->GetILevelSystem()->AddListener(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CWorldBuilder::~CWorldBuilder()
{		
	if(gEnv->pGameFramework)
	{
		if(gEnv->pGameFramework->GetILevelSystem())
			gEnv->pGameFramework->GetILevelSystem()->RemoveListener( this );
	}
}

//////////////////////////////////////////////////////////////////////////
void CWorldBuilder::OnLoadingStart(ILevelInfo *pLevel)
{
	// reset existing prefabs before loading the level
	m_PrefabManager.Clear(false);
}

//////////////////////////////////////////////////////////////////////////
void CWorldBuilder::OnLoadingComplete(ILevelInfo* pLevel)
{	
	int nSeed=0;
	ICVar *pCvar=gEnv->pConsole->GetCVar("g_SessionSeed");
	if (pCvar)	
		nSeed=pCvar->GetIVal();	

	// spawn prefabs	
	{				
		IEntityItPtr i = gEnv->pEntitySystem->GetEntityIterator();
		while (!i->IsEnd())
		{
			IEntity* pEnt = i->Next();
			if (strncmp(pEnt->GetClass()->GetName(), "ProceduralObject", 16) == 0)
			{
				const Vec3& vPos = pEnt->GetPos();

				//m_PrefabManager.SetPrefabGroup(sTileThemeDesc);			
				m_PrefabManager.CallOnSpawn(pEnt, nSeed + VecHash(vPos));
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CWorldBuilder::OnUnloadComplete(ILevelInfo* pLevel)
{
	return;
}

//////////////////////////////////////////////////////////////////////////
void CWorldBuilder::DrawDebugInfo()
{
	// show prefab debug info
	char szDebugInfo[256];
	float colors[4]={1,1,1,1};
	//float colorsYellow[4]={1,1,0,1};
	//float colorsRed[4]={1,0,0,1};

	IEntityItPtr i = gEnv->pEntitySystem->GetEntityIterator();
	while (!i->IsEnd())
	{
		IEntity* pEnt = i->Next();
		if (strncmp(pEnt->GetClass()->GetName(), "ProceduralObject", 16) != 0)
			continue;
			
		Vec3 wp = pEnt->GetWorldTM().GetTranslation();				

		// check if we have the info we need from the script 
		IScriptTable *pScriptTable(pEnt->GetScriptTable());
		if (pScriptTable)
		{
			ScriptAnyValue value;
			if (pScriptTable->GetValueAny("PrefabSourceName",value))
			{
				char *szPrefabName=NULL;
				if (value.CopyTo(szPrefabName))
				{
					cry_sprintf( szDebugInfo, "%s", szPrefabName );
					wp.z-= 0.1f;
					IRenderAuxText::DrawLabelEx(wp, 1.1f, colors, true, true, szDebugInfo);
				}
			}
		}	
	}	//i
}

