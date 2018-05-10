// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 03:04:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEHELPER_H__
#define __VEHICLEHELPER_H__

class CVehicle;

class CVehicleHelper
	: public IVehicleHelper
{
public:
	CVehicleHelper() : m_pParentPart(NULL)
	{
	}

	// IVehicleHelper
	virtual void            Release()          { delete this; }

	virtual const Matrix34& GetLocalTM() const { return m_localTM; }
	virtual void            GetVehicleTM(Matrix34& vehicleTM, bool forced = false) const;
	virtual void            GetWorldTM(Matrix34& worldTM) const;
	virtual void            GetReflectedWorldTM(Matrix34& reflectedWorldTM) const;

	virtual Vec3            GetLocalSpaceTranslation() const;
	virtual Vec3            GetVehicleSpaceTranslation() const;
	virtual Vec3            GetWorldSpaceTranslation() const;

	virtual IVehiclePart*   GetParentPart() const;
	// ~IVehicleHelper

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

protected:
	IVehiclePart* m_pParentPart;

	Matrix34      m_localTM;

	friend class CVehicle;
};

#endif
