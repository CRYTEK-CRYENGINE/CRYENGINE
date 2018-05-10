// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   AngleAlert.h
   $Id$
   $DateTime$
   Description: Single Range donut
   ---------------------------------------------------------------------
   History:
   - 11:09:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#ifndef _ANGLE_ALERT_H_
#define _ANGLE_ALERT_H_

class CPersonalRangeSignaling;

class CAngleAlert
{

public:
	// Base ----------------------------------------------------------
	CAngleAlert(CPersonalRangeSignaling* pPersonal);
	virtual ~CAngleAlert();
	void Init(float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);

	// Utils ---------------------------------------------------------
	bool  Check(const Vec3& vPos) const;
	bool  CheckPlusBoundary(const Vec3& vPos) const;
	float GetAngleTo(const Vec3& vPos) const;

	bool  operator<(const CAngleAlert& AngleAlert) const
	{
		return(m_fAngle < AngleAlert.m_fAngle);
	}

	bool Check(float fAngle) const
	{
		return(fAngle < m_fAngle);
	}

	bool CheckPlusBoundary(float fAngle) const
	{
		return(fAngle < m_fBoundary);
	}

	const string& GetSignal() const
	{
		return(m_sSignal);
	}

	IAISignalExtraData* GetSignalData() const
	{
		return(m_pSignalData);
	}

private:
	CPersonalRangeSignaling* m_pPersonal;
	IAISignalExtraData*      m_pSignalData;
	float                    m_fAngle;
	float                    m_fBoundary;
	string                   m_sSignal;
};
#endif // _ANGLE_ALERT_H_
