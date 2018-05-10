// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_DialogSystem.cpp
//  Version:     v1.00
//  Created:     02/08/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Dialog System ScriptBinding
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "ScriptBind_DialogSystem.h"
#include "DialogSystem.h"
#include "DialogSession.h"

//------------------------------------------------------------------------
CScriptBind_DialogSystem::CScriptBind_DialogSystem(ISystem* pSystem, CDialogSystem* pDS)
{
	m_pSystem = pSystem;
	m_pEntitySystem = gEnv->pEntitySystem;
	m_pDS = pDS;
	assert(m_pDS != 0);

	Init(gEnv->pScriptSystem, m_pSystem);
	SetGlobalName("DialogSystem");

	RegisterGlobals();
	RegisterMethods();
}

//------------------------------------------------------------------------
CScriptBind_DialogSystem::~CScriptBind_DialogSystem()
{
}

//------------------------------------------------------------------------
void CScriptBind_DialogSystem::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_DialogSystem::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_DialogSystem::

	SCRIPT_REG_TEMPLFUNC(CreateSession, "scriptID");
	SCRIPT_REG_TEMPLFUNC(DeleteSession, "sessionID");
	SCRIPT_REG_TEMPLFUNC(SetActor, "sessionID, actorID, entityID");
	SCRIPT_REG_TEMPLFUNC(Play, "sessionID");
	SCRIPT_REG_TEMPLFUNC(Stop, "sessionID");
	SCRIPT_REG_TEMPLFUNC(IsEntityInDialog, "entityID");
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::CreateSession(IFunctionHandler* pH, const char* scriptID)
{
	CDialogSystem::SessionID sessionID = m_pDS->CreateSession(scriptID);
	return pH->EndFunction(sessionID);
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::DeleteSession(IFunctionHandler* pH, int sessionID)
{
	const bool ok = m_pDS->DeleteSession(sessionID);
	return pH->EndFunction(ok);
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::SetActor(IFunctionHandler* pH, int sessionID, int actorID, ScriptHandle entity)
{
	bool ok = false;
	CDialogSession* pSession = m_pDS->GetSession(sessionID);
	if (pSession)
	{
		ok = pSession->SetActor(actorID, (EntityId) entity.n);
	}
	else
	{
		GameWarning("[DiaLOG] CScriptBind_DialogSystem::SetActor: Cannot find session %d", sessionID);
	}
	return pH->EndFunction(ok);
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::Play(IFunctionHandler* pH, int sessionID)
{
	bool ok = false;
	CDialogSession* pSession = m_pDS->GetSession(sessionID);
	if (pSession)
	{
		pSession->SetAutoDelete(true);
		ok = pSession->Play();
	}
	else
	{
		GameWarning("[DiaLOG] CScriptBind_DialogSystem::Play: Cannot find session %d", sessionID);
	}
	return pH->EndFunction(ok);
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::Stop(IFunctionHandler* pH, int sessionID)
{
	bool ok = false;
	CDialogSession* pSession = m_pDS->GetSession(sessionID);
	if (pSession)
	{
		pSession->Stop();
		m_pDS->DeleteSession(sessionID);
	}
	else
	{
		GameWarning("[DiaLOG] CScriptBind_DialogSystem::Play: Cannot find session %d", sessionID);
	}
	return pH->EndFunction(ok);
}

//------------------------------------------------------------------------
int CScriptBind_DialogSystem::IsEntityInDialog(IFunctionHandler* pH, ScriptHandle entity)
{
	CDialogScript::TActorID actorID;
	CDialogSystem::SessionID sessionID;
	const bool inDialog = m_pDS->FindSessionAndActorForEntity((EntityId)entity.n, sessionID, actorID);
	return pH->EndFunction(inDialog);
}
