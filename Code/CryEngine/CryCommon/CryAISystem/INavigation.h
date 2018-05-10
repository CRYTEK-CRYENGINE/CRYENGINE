// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	AI Navigation Interface.

   -------------------------------------------------------------------------
   History:
   - 3:3:2010		:	Created by Kevin Kirst

*************************************************************************/

#ifndef _INAVIGATION_H_
#define _INAVIGATION_H_

#include <CryAISystem/IAISystem.h> // <> required for Interfuscator

struct IAIObject;

struct INavigation
{
	enum EIFMode {IF_AREASBOUNDARIES, IF_AREAS, IF_BOUNDARIES};

	// <interfuscator:shuffle>
	virtual ~INavigation() {}

	virtual float  GetNearestPointOnPath(const char* szPathName, const Vec3& vPos, Vec3& vResult, bool& bLoopPath, float& totalLength) const = 0;
	virtual void   GetPointOnPathBySegNo(const char* szPathName, Vec3& vResult, float segNo) const = 0;

	//! Returns nearest designer created path/shape.
	//! \param devalue Specifies how long the path will be unusable by others after the query.
	//! \param useStartNode If true the start point of the path is used to select nearest path instead of the nearest point on path.
	virtual const char* GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& pos, float range, int type, float devalue, bool useStartNode) = 0;

	// </interfuscator:shuffle>
};

#endif //_INAVIGATION_H_
