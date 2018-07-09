// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugRenderPrimitives.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// make the global Serialize() functions available for use in yasli serialization
using UQS::Core::Serialize;

namespace UQS
{
	namespace Core
	{

		static IRenderAuxGeom* GetRenderAuxGeom()
		{
			return gEnv->pRenderer ? gEnv->pRenderer->GetIRenderAuxGeom() : nullptr;
		}

		//===================================================================================
		//
		// CDebugRenderPrimitiveBase
		//
		//===================================================================================

		int CDebugRenderPrimitiveBase::GetFlags3D()
		{
			int flags3D = e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn;

			if (SCvars::debugDrawZTestOn)
			{
				flags3D |= e_DepthTestOn;
			}
			else
			{
				flags3D |= e_DepthTestOff;
			}

			return flags3D;
		}

		float CDebugRenderPrimitiveBase::Pulsate()
		{
			static const float periodLengthInSeconds = 0.5f;

			const float currentSeconds = gEnv->pTimer->GetAsyncCurTime();
			const float remainder = fmodf(currentSeconds, periodLengthInSeconds);
			return 1.0f + sinf(remainder * gf_PI2);
		}

		void CDebugRenderPrimitiveBase::HelpDrawAABB(const AABB& localAABB, const Vec3& pos, const Matrix33* pOrientation, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				Vec3 frontPoints[4] =
				{
					Vec3(localAABB.min.x, localAABB.min.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.min.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.min.y, localAABB.max.z),
					Vec3(localAABB.min.x, localAABB.min.y, localAABB.max.z),
				};

				Vec3 backPoints[4] =
				{
					Vec3(localAABB.min.x, localAABB.max.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.max.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.max.y, localAABB.max.z),
					Vec3(localAABB.min.x, localAABB.max.y, localAABB.max.z),
				};

				Vec3 leftPoints[4] =
				{
					Vec3(localAABB.min.x, localAABB.min.y, localAABB.max.z),
					Vec3(localAABB.min.x, localAABB.min.y, localAABB.min.z),
					Vec3(localAABB.min.x, localAABB.max.y, localAABB.min.z),
					Vec3(localAABB.min.x, localAABB.max.y, localAABB.max.z),
				};

				Vec3 rightPoints[4] =
				{
					Vec3(localAABB.max.x, localAABB.min.y, localAABB.max.z),
					Vec3(localAABB.max.x, localAABB.min.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.max.y, localAABB.min.z),
					Vec3(localAABB.max.x, localAABB.max.y, localAABB.max.z),
				};

				ColorB colorArray[4] =
				{
					color, color, color, color
				};

				// rotate all points
				if (pOrientation)
				{
					for (int i = 0; i < 4; ++i)
					{
						frontPoints[i] = *pOrientation * frontPoints[i];
						backPoints[i] = *pOrientation * backPoints[i];
						leftPoints[i] = *pOrientation * leftPoints[i];
						rightPoints[i] = *pOrientation * rightPoints[i];
					}
				}

				// translate all points
				for (int i = 0; i < 4; ++i)
				{
					frontPoints[i] += pos;
					backPoints[i] += pos;
					leftPoints[i] += pos;
					rightPoints[i] += pos;
				}

				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawPolyline(frontPoints, 4, true, colorArray, SCvars::debugDrawLineThickness);
				pAux->DrawPolyline(backPoints, 4, true, colorArray, SCvars::debugDrawLineThickness);
				pAux->DrawPolyline(leftPoints, 4, true, colorArray, SCvars::debugDrawLineThickness);
				pAux->DrawPolyline(rightPoints, 4, true, colorArray, SCvars::debugDrawLineThickness);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Sphere
		//
		//===================================================================================

		CDebugRenderPrimitive_Sphere::CDebugRenderPrimitive_Sphere()
			: m_pos(ZERO)
			, m_radius(0.0f)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Sphere::CDebugRenderPrimitive_Sphere(const Vec3& pos, float radius, const ColorF& color)
			: m_pos(pos)
			, m_radius(radius)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Sphere::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos, m_radius, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Sphere::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Sphere::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_radius, "m_radius");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Sphere::Draw(const Vec3& pos, float radius, const ColorF& color, bool bHighlight)
		{
			const float radiusMultiplier = bHighlight ? Pulsate() : 1.0f;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawSphere(pos, radiusMultiplier * radius, color);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Direction
		//
		//===================================================================================

		CDebugRenderPrimitive_Direction::CDebugRenderPrimitive_Direction()
			: m_from(ZERO)
			, m_to(ZERO)
			, m_coneRadius(0.0f)
			, m_coneHeight(0.0f)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Direction::CDebugRenderPrimitive_Direction(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color)
			: m_from(from)
			, m_to(to)
			, m_coneRadius(coneRadius)
			, m_coneHeight(coneHeight)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Direction::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_from, m_to, m_coneRadius, m_coneHeight, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Direction::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Direction::Serialize(Serialization::IArchive& ar)
		{
			ar(m_from, "m_from");
			ar(m_to, "m_to");
			ar(m_coneRadius, "m_coneRadius");
			ar(m_coneHeight, "m_coneHeight");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Direction::Draw(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawLine(from, color, to, color, SCvars::debugDrawLineThickness);
				Vec3 dir = (to - from).GetNormalizedSafe();
				pAux->DrawCone(to - dir * coneHeight, dir, coneRadius, coneHeight, color);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Line
		//
		//===================================================================================

		CDebugRenderPrimitive_Line::CDebugRenderPrimitive_Line()
			: m_pos1(ZERO)
			, m_pos2(ZERO)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Line::CDebugRenderPrimitive_Line(const Vec3& pos1, const Vec3& pos2, const ColorF& color)
			: m_pos1(pos1)
			, m_pos2(pos2)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Line::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos1, m_pos2, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Line::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Line::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos1, "m_pos1");
			ar(m_pos2, "m_pos2");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Line::Draw(const Vec3& pos1, const Vec3& pos2, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawLine(pos1, color, pos2, color, SCvars::debugDrawLineThickness);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Cone
		//
		//===================================================================================

		CDebugRenderPrimitive_Cone::CDebugRenderPrimitive_Cone()
			: m_pos(ZERO)
			, m_dir(0, 0, 1)
			, m_baseRadius(0.0f)
			, m_height(0.0f)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Cone::CDebugRenderPrimitive_Cone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color)
			: m_pos(pos)
			, m_dir(dir)
			, m_baseRadius(baseRadius)
			, m_height(height)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Cone::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos, m_dir, m_baseRadius, m_height, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Cone::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Cone::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_dir, "m_dir");
			ar(m_baseRadius, "m_baseRadius");
			ar(m_height, "m_height");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Cone::Draw(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawCone(pos, dir, baseRadius, height, color);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Cylinder
		//
		//===================================================================================

		CDebugRenderPrimitive_Cylinder::CDebugRenderPrimitive_Cylinder()
			: m_pos(ZERO)
			, m_dir(0, 0, 1)
			, m_radius(0.0f)
			, m_height(0.0f)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Cylinder::CDebugRenderPrimitive_Cylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color)
			: m_pos(pos)
			, m_dir(dir)
			, m_radius(radius)
			, m_height(height)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Cylinder::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos, m_dir, m_radius, m_height, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Cylinder::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Cylinder::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_dir, "m_dir");
			ar(m_radius, "m_radius");
			ar(m_height, "m_height");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Cylinder::Draw(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawCylinder(pos, dir, radius, height, color);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Text
		//
		//===================================================================================

		CDebugRenderPrimitive_Text::CDebugRenderPrimitive_Text()
			: m_pos(ZERO)
			, m_size(0.0f)
			, m_text()
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Text::CDebugRenderPrimitive_Text(const Vec3& pos, float size, const char* szText, const ColorF& color)
			: m_pos(pos)
			, m_size(size)
			, m_text(szText)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Text::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos, m_size, m_text.c_str(), pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Text::GetRoughMemoryUsage() const
		{
			// FIXME: the stack_string might have switched to the heap when having exceeded its capacity (but then again: does he deal with ref-counting?)
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Text::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_size, "m_size");
			ar(m_text, "m_text");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Text::Draw(const Vec3& pos, float size, const char* szText, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				SDrawTextInfo ti;
				ti.scale.set(size, size);
				ti.flags = eDrawText_FixedSize;
				if (SCvars::debugDrawZTestOn)
				{
					ti.flags |= eDrawText_DepthTest;
				}
				ti.color[0] = color.r;
				ti.color[1] = color.g;
				ti.color[2] = color.b;
				ti.color[3] = color.a;
				pAux->RenderTextQueued(pos, ti, szText);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_Quat
		//
		//===================================================================================

		CDebugRenderPrimitive_Quat::CDebugRenderPrimitive_Quat()
			: m_pos(ZERO)
			, m_quat(ZERO)
			, m_radius(0.0f)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_Quat::CDebugRenderPrimitive_Quat(const Vec3& pos, const Quat& q, float r, const ColorF& color)
			: m_pos(pos)
			, m_quat(q)
			, m_radius(r)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_Quat::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_pos, m_quat, m_radius, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_Quat::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_Quat::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_quat, "m_quat");
			ar(m_radius, "m_radius");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_Quat::Draw(const Vec3& pos, const Quat& q, float r, const ColorF& color, bool bHighlight)
		{
			const bool bVisible = bHighlight ? (Pulsate() > 1.5f) : true;

			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (bVisible && pAux)
			{
				const Vec3 xAxis = r * q.GetColumn0();
				const Vec3 yAxis = r * q.GetColumn1();
				const Vec3 zAxis = r * q.GetColumn2();
				const OBB obb = OBB::CreateOBB(Matrix33::CreateFromVectors(xAxis, yAxis, zAxis), Vec3(0.05f, 0.05f, 0.05f), ZERO);

				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawOBB(obb, pos, false, color, eBBD_Extremes_Color_Encoded);
				pAux->DrawLine(pos, ColorF(1, 0, 0, color.a), pos + xAxis, ColorF(1, 0, 0, color.a), SCvars::debugDrawLineThickness);
				pAux->DrawLine(pos, ColorF(0, 1, 0, color.a), pos + yAxis, ColorF(0, 1, 0, color.a), SCvars::debugDrawLineThickness);
				pAux->DrawLine(pos, ColorF(0, 0, 1, color.a), pos + zAxis, ColorF(0, 0, 1, color.a), SCvars::debugDrawLineThickness);
			}
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_AABB
		//
		//===================================================================================

		CDebugRenderPrimitive_AABB::CDebugRenderPrimitive_AABB()
			: m_aabb(AABB::RESET)
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_AABB::CDebugRenderPrimitive_AABB(const AABB& aabb, const ColorF& color)
			: m_aabb(aabb)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_AABB::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_aabb, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_AABB::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CDebugRenderPrimitive_AABB::Serialize(Serialization::IArchive& ar)
		{
			ar(m_aabb, "m_aabb");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_AABB::Draw(const AABB& aabb, const ColorF& color, bool bHighlight)
		{
			HelpDrawAABB(aabb, Vec3(0, 0, 0), nullptr, color, bHighlight);
		}

		//===================================================================================
		//
		// CDebugRenderPrimitive_OBB
		//
		//===================================================================================

		CDebugRenderPrimitive_OBB::CDebugRenderPrimitive_OBB()
			: m_obb(OBB::CreateOBB(Matrix33::CreateIdentity(), ZERO, ZERO))
			, m_color(Col_Black)
		{}

		CDebugRenderPrimitive_OBB::CDebugRenderPrimitive_OBB(const OBB& obb, const ColorF& color)
			: m_obb(obb)
			, m_color(color)
		{}

		void CDebugRenderPrimitive_OBB::Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const
		{
			Draw(m_obb, pOptionalColorToOverrideWith ? *pOptionalColorToOverrideWith : m_color, bHighlight);
		}

		size_t CDebugRenderPrimitive_OBB::GetRoughMemoryUsage() const
		{
			return sizeof(*this);
		}

		void CDebugRenderPrimitive_OBB::Serialize(Serialization::IArchive& ar)
		{
			ar(m_obb, "m_obb");
			ar(m_color, "m_color");
		}

		void CDebugRenderPrimitive_OBB::Draw(const OBB& obb, const ColorF& color, bool bHighlight)
		{
			const AABB localAABB(-obb.h, obb.h);
			HelpDrawAABB(localAABB, obb.c, &obb.m33, color, bHighlight);
		}

	}
}
