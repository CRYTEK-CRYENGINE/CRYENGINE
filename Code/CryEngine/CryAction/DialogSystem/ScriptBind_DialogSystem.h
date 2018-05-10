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
#ifndef __SCRIPTBIND_DIALOGSYSTEM_H__
#define __SCRIPTBIND_DIALOGSYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

class CDialogSystem;

class CScriptBind_DialogSystem : public CScriptableBase
{
public:
	CScriptBind_DialogSystem(ISystem* pSystem, CDialogSystem* pDS);
	virtual ~CScriptBind_DialogSystem();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:
	void RegisterGlobals();
	void RegisterMethods();

	int  CreateSession(IFunctionHandler* pH, const char* scriptID);
	int  DeleteSession(IFunctionHandler* pH, int sessionID);
	int  SetActor(IFunctionHandler* pH, int sessionID, int actorID, ScriptHandle entity);
	int  Play(IFunctionHandler* pH, int sessionID);
	int  Stop(IFunctionHandler* pH, int sessionID);
	int  IsEntityInDialog(IFunctionHandler* pH, ScriptHandle entity);

private:
	ISystem*       m_pSystem;
	IEntitySystem* m_pEntitySystem;
	CDialogSystem* m_pDS;
};

#endif //__SCRIPTBIND_DIALOGSYSTEM_H__
