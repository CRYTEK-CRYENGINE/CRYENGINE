// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CoverPath_h__
#define __CoverPath_h__
#pragma once

#include "Cover.h"

class CoverPath
{
public:
	enum PathFlags
	{
		Looped = 1 << 0,
	};

	struct Point
	{
		Point()
			: position(ZERO)
			, distance(0.0f)
		{
		}
		Point(const Vec3& pos, float dist)
			: position(pos)
			, distance(dist)
		{
		}

		Vec3  position;
		float distance;
	};

	CoverPath();

	typedef std::vector<Point> Points;
	const Points& GetPoints() const;

	bool          Empty() const;
	void          Clear();
	void          Reserve(uint32 pointCount);
	void          AddPoint(const Vec3& point);

	Vec3          GetPointAt(float distance) const;
	Vec3          GetClosestPoint(const Vec3& point, float* distanceToPath, float* distanceOnPath = 0) const;
	float         GetDistanceAt(const Vec3& point, float tolerance = 0.05f) const;
	Vec3          GetNormalAt(const Vec3& point) const;

	float         GetLength() const;

	uint32        GetFlags() const;
	void          SetFlags(uint32 flags);

	bool          Intersect(const Vec3& origin, const Vec3& dir, float* distance, Vec3* point) const;

	void          DebugDraw();

private:
	Points m_points;
	uint32 m_flags;
};

#endif //__CoverPath_h__
