// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#ifndef HidespotQueryContext_h
	#define HidespotQueryContext_h

//! Contains information about what hide spots you want the AI system to return.
struct HidespotQueryContext
{
	HidespotQueryContext()
		: minRange(0.0f)
		, maxRange(0.0f)
		, hideFromPos(ZERO)
		, centerOfQuery(ZERO)
		, onlyThoseThatGiveCover(true)
		, onlyCollidable(false)
		, maxPoints(0)
		, pCoverPos(NULL)
		, pCoverObjPos(NULL)
		, pCoverObjDir(NULL)
	{

	}

	float        minRange;
	float        maxRange;
	Vec3         hideFromPos;
	Vec3         centerOfQuery;
	bool         onlyThoseThatGiveCover;
	bool         onlyCollidable;

	unsigned int maxPoints;
	Vec3*        pCoverPos;
	Vec3*        pCoverObjPos;
	Vec3*        pCoverObjDir;
};

#endif // HidespotQueryContext_h
