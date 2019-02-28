// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$   All input actions handlers of the game

-------------------------------------------------------------------------
History:
- 16.04.10   : Created by Benito G.R.
*************************************************************************/

#pragma once

#ifndef _GAMEINPUT_ACTIONHANDLERS_H_
#define _GAMEINPUT_ACTIONHANDLERS_H_

/*********** WARNING **************
/
/ If you add a new handler here, be careful with using forward declarations of your classes
/ they might cause issues, prefer #includes
/
/***********************************************/

#include <IActionMapManager.h>
#include "Weapon.h"
#include "HeavyMountedWeapon.h"
#include "JAW.h"
#include "LTAG.h"
#include "Binocular.h"
#include "PickAndThrowWeapon.h"
#include "NoWeapon.h"

class CGameInputActionHandlers
{
public:
	typedef TActionHandler<CWeapon> TWeaponActionHandler;
	typedef TActionHandler<CPickAndThrowWeapon> TPickAndThrowWeaponActionHandler;
	typedef TActionHandler<CLTag> TLTagActionHandler;
	typedef TActionHandler<CJaw> TJawActionHandler;
	typedef TActionHandler<CHeavyMountedWeapon> THMGActionHandler;
	typedef TActionHandler<CBinocular> TBinocularActionHandler;
	typedef TActionHandler<CNoWeapon> TNoWeaponActionHandler;

	ILINE TWeaponActionHandler& GetCWeaponActionHandler() { return m_weaponActionHandler; }
	ILINE TPickAndThrowWeaponActionHandler& GetCPickAndThrowWeaponActionHandler() { return m_pickAndThrowWeaponActionHandler; }
	ILINE TLTagActionHandler& GetCLtagActionHandler() { return m_ltagActionHandler; }
	ILINE TJawActionHandler& GetCJawActionHandler() { return m_jawActionHandler; }
	ILINE THMGActionHandler& GetCHMGActionHandler() { return m_hmgActionHandler; }
	ILINE TBinocularActionHandler& GetCBinocularActionHandler() { return m_binocularActionHandler; }
	ILINE TNoWeaponActionHandler& GetCNoWeaponActionHandler() { return m_NoWeaponActionHandler; }

private:

	TWeaponActionHandler	m_weaponActionHandler;
	TPickAndThrowWeaponActionHandler m_pickAndThrowWeaponActionHandler;
	TLTagActionHandler m_ltagActionHandler;
	TJawActionHandler m_jawActionHandler;
	THMGActionHandler m_hmgActionHandler;
	TBinocularActionHandler m_binocularActionHandler;
	TNoWeaponActionHandler m_NoWeaponActionHandler;

};

#endif //_GAMEINPUT_ACTIONHANDLERS_H_
