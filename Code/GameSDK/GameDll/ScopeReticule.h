// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Iron Sight

-------------------------------------------------------------------------
History:
- 17:09:2010   Created by Filipe Amim

*************************************************************************/
#pragma once

#ifndef __SCOPE_RETICULE_H__
#define __SCOPE_RETICULE_H__


struct SScopeParams;
class CWeapon;



class CScopeReticule
{
private:
	typedef _smart_ptr<IMaterial> TMaterialPtr;

public:
	CScopeReticule();

	void SetMaterial(IMaterial* pMaterial);
	void SetBlinkFrequency(float blink);

	void Disable(float time);
	void Update(CWeapon* pWeapon);

private:
	TMaterialPtr m_scopeReticuleMaterial;
	float m_blinkFrequency;
	float m_disabledTimeOut;
};


#endif
