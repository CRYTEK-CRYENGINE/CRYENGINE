// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CGeomUtil
		//
		//===================================================================================

		class CGeomUtil
		{
		public:
			static float     Distance_Lineseg_AABB(const Lineseg& lineseg, const AABB& aabb);

		private:
			static void      TrianglesFromAABB(Triangle (&outTriangles)[12], const AABB& aabb);
		};

	}
}
