// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IObjectManager.h"
#include "Objects/BaseObject.h"
#include "CryPhysics/physinterface.h"

struct DisplayContext;
struct IPhysicalEntity;

//! This class draws an alignment line from a position to the world
class CAxisHelperExtended
{
public:
	CAxisHelperExtended();
	void DrawAxes(DisplayContext& dc, const Matrix34& matrix);

private:
	void DrawAxis(DisplayContext& dc, const Vec3& direction, const float dist, const Vec3& up, const Vec3& color);

private:
	Matrix34                      m_matrix;
	Vec3                          m_position;
	std::vector<IPhysicalEntity*> m_skipEntities;
	std::vector<CBaseObjectPtr>   m_objects;
	CBaseObjectsArray             m_objectsForPicker;
	CBaseObject*                  m_pCurObject;
	DWORD                         m_lastUpdateTime;

	float                         m_lastDistNegX;
	float                         m_lastDistNegY;
	float                         m_lastDistNegZ;
	float                         m_lastDistPosX;
	float                         m_lastDistPosY;
	float                         m_lastDistPosZ;

	static const float            ms_maxDist;
};

