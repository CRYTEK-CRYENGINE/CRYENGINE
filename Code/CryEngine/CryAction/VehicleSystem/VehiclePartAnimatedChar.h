// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which extends the VehiclePartAnimated
             but provides for using living entity type, useful for walkers

   -------------------------------------------------------------------------
   History:
   - 24:08:2011: Created by Richard Semmens

*************************************************************************/
#ifndef __VEHICLEPARTANIMATEDCHAR_H__
#define __VEHICLEPARTANIMATEDCHAR_H__

#include "VehiclePartAnimated.h"

class CVehiclePartAnimatedChar
	: public CVehiclePartAnimated
{
	IMPLEMENT_VEHICLEOBJECT
public:
	CVehiclePartAnimatedChar();
	virtual ~CVehiclePartAnimatedChar();

	// IVehiclePart
	virtual void Physicalize() override;
	// ~IVehiclePart
};

#endif
