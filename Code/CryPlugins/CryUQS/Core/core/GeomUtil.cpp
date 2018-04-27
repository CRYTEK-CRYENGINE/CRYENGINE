// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		float CGeomUtil::Distance_Lineseg_AABB(const Lineseg& lineseg, const AABB& aabb)
		{
			Triangle triangles[12];
			TrianglesFromAABB(triangles, aabb);

			float closestDistanceSoFar = std::numeric_limits<float>::max();

			for (int i = 0; i < 12; ++i)
			{
				const float d = Distance::Lineseg_Triangle(lineseg, triangles[i], static_cast<float*>(nullptr), static_cast<float*>(nullptr), static_cast<float*>(nullptr));

				if (d < closestDistanceSoFar)
				{
					closestDistanceSoFar = d;
				}
			}

			return closestDistanceSoFar;
		}

		void CGeomUtil::TrianglesFromAABB(Triangle (&outTriangles)[12], const AABB& aabb)
		{
			// front 1
			outTriangles[0](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.max.z));

			// front 2
			outTriangles[1](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.min.x, aabb.min.y, aabb.max.z));

			// back 1
			outTriangles[2](
				Vec3(aabb.min.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			// back 2
			outTriangles[3](
				Vec3(aabb.min.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.max.z));

			// up 1
			outTriangles[4](
				Vec3(aabb.min.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			// up 2
			outTriangles[5](
				Vec3(aabb.min.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.max.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			// down 1
			outTriangles[6](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.min.z));

			// down 2
			outTriangles[7](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.min.z));

			// right 1
			outTriangles[8](
				Vec3(aabb.max.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			// right 2
			outTriangles[9](
				Vec3(aabb.max.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.max.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			// left 1
			outTriangles[10](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.min.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.max.z));

			// left 2
			outTriangles[11](
				Vec3(aabb.min.x, aabb.min.y, aabb.min.z),
				Vec3(aabb.min.x, aabb.min.y, aabb.max.z),
				Vec3(aabb.min.x, aabb.max.y, aabb.max.z));
		}

	}
}
