// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 21:9:2004   3:00 : Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "ActorSystem.h"
#include <CryGame/IGameFramework.h>
#include "ScriptBind_ActorSystem.h"

//------------------------------------------------------------------------
CScriptBind_ActorSystem::CScriptBind_ActorSystem(ISystem* pSystem, IGameFramework* pGameFW)
{
	m_pSystem = pSystem;
	//m_pActorSystem = pActorSystem;
	m_pGameFW = pGameFW;

	Init(pSystem->GetIScriptSystem(), m_pSystem);
	SetGlobalName("Actor");

	RegisterGlobals();
	RegisterMethods();
}

//------------------------------------------------------------------------
CScriptBind_ActorSystem::~CScriptBind_ActorSystem()
{
}

//------------------------------------------------------------------------
void CScriptBind_ActorSystem::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_ActorSystem::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_ActorSystem::

	SCRIPT_REG_TEMPLFUNC(CreateActor, "channelId, actorParams");
}

//------------------------------------------------------------------------
// Example on how to use this function:
//
// local params =
// {
//   name     = "dude",
//   class    = "CSpectator",
//   position = {x=0, y=0, z=0},
//   rotation = {x=0, y=0, z=0},
//   scale    = {x=1, y=1, z=1}
// }
//
// Actor.CreateActor( channelId, params );
//
int CScriptBind_ActorSystem::CreateActor(IFunctionHandler* pH, int channelId, SmartScriptTable actorParams)
{
	CRY_ASSERT(m_pGameFW->GetIActorSystem() != NULL);
	const char* name;
	const char* className;
	Vec3 position;
	Vec3 scale;
	Vec3 rotation;

#define GET_VALUE_FROM_CHAIN(valName, val, chain)                                                                       \
  if (!chain.GetValue(valName, val))                                                                                    \
  {                                                                                                                     \
    CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CreateActor failed because <%s> field not specified", valName); \
    bFailed = true;                                                                                                     \
  }

	// The following code had to be enclosed in a bracket because
	// CScriptSetGetChain needs to be declared statically and also needs to
	// be destructed before EndFunction
	bool bFailed = false;
	do
	{
		CScriptSetGetChain actorChain(actorParams);
		GET_VALUE_FROM_CHAIN("name", name, actorChain);
		GET_VALUE_FROM_CHAIN("class", className, actorChain);
		GET_VALUE_FROM_CHAIN("position", position, actorChain);
		GET_VALUE_FROM_CHAIN("rotation", rotation, actorChain);
		GET_VALUE_FROM_CHAIN("scale", scale, actorChain);
	}
	while (false);

	if (bFailed)
		return pH->EndFunction(false);

	Quat q;
	q.SetRotationXYZ(Ang3(rotation));
	IActor* pActor = m_pGameFW->GetIActorSystem()->CreateActor(channelId, name, className, position, q, scale);

	if (pActor == NULL)
		return pH->EndFunction();
	else
		return pH->EndFunction(pActor->GetEntity()->GetScriptTable());
}
