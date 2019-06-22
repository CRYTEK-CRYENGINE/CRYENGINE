// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>

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
