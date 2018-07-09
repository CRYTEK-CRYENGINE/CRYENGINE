// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCRIPT_BIND__TURRET__H__
#define __SCRIPT_BIND__TURRET__H__

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>


class CTurret;

class CScriptBind_Turret
	: public CScriptableBase
{
public:
	CScriptBind_Turret( ISystem* pSystem );
	virtual ~CScriptBind_Turret();

	void AttachTo( CTurret* pTurret );
	void DettachFrom( CTurret* pTurret );

	int Enable( IFunctionHandler* pH );
	int Disable( IFunctionHandler* pH );

	int OnPropertyChange( IFunctionHandler* pH );
	int OnHit( IFunctionHandler* pH, SmartScriptTable scriptHitInfo );
	int SetStateById( IFunctionHandler* pH, int stateId );

	int SetFactionToPlayerFaction( IFunctionHandler* pH );
};

#endif
