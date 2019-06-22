// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		struct IRenderPrimitiveCollection
		{
			virtual void	AddSphere(const Vec3& pos, float radius, const ColorF& color) = 0;
			virtual void	AddLine(const Vec3& pos1, const Vec3& pos2, const ColorF& color) = 0;
			virtual void	AddTriangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color) = 0;
			virtual void	AddText(const Vec3& pos, float size, const ColorF& color, const char* szFormat, ...) PRINTF_PARAMS(5, 6) = 0;
			virtual void	AddArrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color) = 0;
			virtual void	AddAxes(const Vec3& pos, const Matrix33& axes) = 0;
			virtual void	AddAABB(const AABB& aabb, const ColorF& color) = 0;
			virtual void	AddOBB(const OBB& obb, const Vec3& pos, const ColorF& color) = 0;
			virtual void	AddCone(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) = 0;
			virtual void	AddCylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color) = 0;

			virtual void	AddSphereWithDebugDrawDuration(const Vec3& pos, float radius, const ColorF& color, const float duration) = 0;
			virtual void	AddLineWithDebugDrawDuration(const Vec3& pos1, const Vec3& pos2, const ColorF& color, const float duration) = 0;
			virtual void	AddTriangleWithDebugDrawDuration(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color, const float duration) = 0;
			virtual void	AddTextWithDebugDrawDuration(const Vec3& pos, float size, const ColorF& color, const float duration, const char* szFormat, ...) PRINTF_PARAMS(6, 7) = 0;
			virtual void	AddArrowWithDebugDrawDuration(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, const float duration) = 0;
			virtual void	AddAxesWithDebugDrawDuration(const Vec3& pos, const Matrix33& axes, const float duration) = 0;
			virtual void	AddAABBWithDebugDrawDuration(const AABB& aabb, const ColorF& color, const float duration) = 0;
			virtual void	AddOBBWithDebugDrawDuration(const OBB& obb, const Vec3& pos, const ColorF& color, const float duration) = 0;
			virtual void	AddConeWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) = 0;
			virtual void	AddCylinderWithDebugDrawDuration(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color, const float duration) = 0;

		protected:

			~IRenderPrimitiveCollection() = default; // not intended to get deleted via base class pointers
		};

	}
}