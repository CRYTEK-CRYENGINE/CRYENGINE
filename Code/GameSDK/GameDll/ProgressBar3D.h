// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Basic encapsulation of a 3D Hud Progress bar
	-------------------------------------------------------------------------
*************************************************************************/

#ifndef __PROGRESSBAR3D_H_
#define __PROGRESSBAR3D_H_

#include "ProgressBar.h"

struct ProgressBar3DParams
{
	ProgressBar3DParams() 
		: m_EntityId(0)
		, m_Offset(ZERO)
		, m_height(0)
		, m_width(0)
	{
	}

	//If 0 then bar will simply be drawn about m_Offset
	EntityId m_EntityId;
	Vec3		 m_Offset;
	float		 m_height;
	float		 m_width;
};

class CProgressBar3D : public CProgressBar
{
public:
	CProgressBar3D();
	virtual ~CProgressBar3D();

	void Init(const ProgressBarParams& params, const ProgressBar3DParams& params3D); 
	void Render3D();
	void SetProgressValue(float zeroToOneProgress);

private:
	ProgressBar3DParams m_params3D;
};
#endif //__PROGRESSBAR3D_H_