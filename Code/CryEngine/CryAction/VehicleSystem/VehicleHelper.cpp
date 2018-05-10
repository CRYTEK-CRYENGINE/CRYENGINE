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
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleHelper.h"
#include "CryAction.h"
#include "PersistantDebug.h"

//------------------------------------------------------------------------
void CVehicleHelper::GetVehicleTM(Matrix34& vehicleTM, bool forced) const
{
	vehicleTM = m_localTM;

	IVehiclePart* pParent = m_pParentPart;
	while (pParent)
	{
		vehicleTM = pParent->GetLocalTM(true, forced) * vehicleTM;
		pParent = pParent->GetParent();
	}
}

//------------------------------------------------------------------------
void CVehicleHelper::GetWorldTM(Matrix34& worldTM) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const Matrix34& partWorldTM = m_pParentPart->GetWorldTM();

	worldTM = Matrix34(Matrix33(partWorldTM) * Matrix33(m_localTM));
	worldTM.SetTranslation((partWorldTM * m_localTM).GetTranslation());
}

//------------------------------------------------------------------------
void CVehicleHelper::GetReflectedWorldTM(Matrix34& reflectedWorldTM) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	Matrix34 tempMatrix = m_localTM;
	tempMatrix.m03 = -tempMatrix.m03; // negate x coord of translation

	const Matrix34& partWorldTM = m_pParentPart->GetWorldTM();

	reflectedWorldTM = Matrix34(Matrix33(partWorldTM) * Matrix33(tempMatrix));
	reflectedWorldTM.SetTranslation((partWorldTM * tempMatrix).GetTranslation());
}

//------------------------------------------------------------------------
Vec3 CVehicleHelper::GetLocalSpaceTranslation() const
{
	return m_localTM.GetTranslation();
}

//------------------------------------------------------------------------
Vec3 CVehicleHelper::GetVehicleSpaceTranslation() const
{
	Matrix34 temp;
	GetVehicleTM(temp);
	return temp.GetTranslation();
}

//------------------------------------------------------------------------
Vec3 CVehicleHelper::GetWorldSpaceTranslation() const
{
	Matrix34 temp;
	GetWorldTM(temp);
	return temp.GetTranslation();
}

//------------------------------------------------------------------------
IVehiclePart* CVehicleHelper::GetParentPart() const
{
	return m_pParentPart;
}
