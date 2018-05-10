// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for anything we only want to be accessed at controlled/protected points in the application

-------------------------------------------------------------------------
History:
- 17:01:2012   11:11 : Created by AndrewB
*************************************************************************/
#ifndef __SCRIPTBIND_PROTECTED_H__
#define __SCRIPTBIND_PROTECTED_H__

//Base class include
#include <CryScriptSystem/ScriptHelpers.h>

//Important includes
//#include <IScriptSystem.h>

//Pre-declarations
struct ISystem;
struct IPlayerProfile;

//////////////////////////////////////////////////////////////////////////
class CScriptBind_ProtectedBinds :
	public CScriptableBase
{

public:
	CScriptBind_ProtectedBinds( ISystem *pSystem );
	virtual ~CScriptBind_ProtectedBinds();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}


	// Persistant Stats
	int GetPersistantStat(IFunctionHandler *pH, const char *name);
	int SetPersistantStat(IFunctionHandler *pH, const char *name, SmartScriptTable valueTab);
	int SavePersistantStatsToBlaze(IFunctionHandler *pH);

	//Profile Functions
	int GetProfileAttribute( IFunctionHandler *pH, const char* name );
	int SetProfileAttribute(  IFunctionHandler *pH, const char* name, SmartScriptTable valueTab );

	int ActivateDemoEventEntitlement( IFunctionHandler *pH );

	void	Enable();
	void	Disable();

protected:

private:
	void RegisterGlobals();
	void RegisterMethods();

	IPlayerProfile*		GetCurrentUserProfile();

	ISystem*					m_pSystem;
	bool							m_active;
};

#endif //__SCRIPTBIND_PROTECTED_H__
