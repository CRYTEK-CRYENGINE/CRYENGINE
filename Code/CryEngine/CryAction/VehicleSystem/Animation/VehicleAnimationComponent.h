// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements vehicle animation using Mannequin

   -------------------------------------------------------------------------
   History:
   - 06:02:2012: Created by Tom Berry

*************************************************************************/
#ifndef __CVEHICLEANIMATION_H__
#define __CVEHICLEANIMATION_H__

#include <ICryMannequinDefs.h>

class CVehicle;
class IAnimationDatabase;
class IActionController;
struct SAnimationContext;

class CVehicleAnimationComponent : public IVehicleAnimationComponent
{
public:
	CVehicleAnimationComponent();
	~CVehicleAnimationComponent();

	void               Initialise(CVehicle& vehicle, const CVehicleParams& mannequinTable);
	void               Reset();
	void               DeleteActionController();

	void               Update(float timePassed);

	IActionController* GetActionController() { return m_pActionController; }
	void               AttachPassengerScope(CVehicleSeat* seat, EntityId ent, bool attach);

	virtual CTagState* GetTagState()
	{
		return m_pAnimContext ? &m_pAnimContext->state : NULL;
	}

	ILINE bool IsEnabled() const
	{
		return (m_pActionController != NULL);
	}

private:
	CVehicle*                 m_vehicle;
	const IAnimationDatabase* m_pADBVehicle;
	const IAnimationDatabase* m_pADBPassenger;
	SAnimationContext*        m_pAnimContext;
	IActionController*        m_pActionController;
	TagID                     m_typeTag;
};

#endif //!__CVEHICLEANIMATION_H__
