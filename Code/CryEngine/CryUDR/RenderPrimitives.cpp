// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{

		//===================================================================================
		//
		// CRenderPrimitiveBase
		//
		//===================================================================================

		CRenderPrimitiveBase::CRenderPrimitiveBase()
		{
			m_metadata.Initialize();
		}

		const CTimeMetadata& CRenderPrimitiveBase::GetMetadata() const
		{
			return m_metadata;
		}

		void CRenderPrimitiveBase::Serialize(Serialization::IArchive& ar)
		{
			ar(m_metadata, "m_metadata");
		}

		IRenderAuxGeom* CRenderPrimitiveBase::GetRenderAuxGeom()
		{
			return gEnv->pRenderer ? gEnv->pRenderer->GetIRenderAuxGeom() : nullptr;
		}

		int CRenderPrimitiveBase::GetFlags3D()
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

		void CRenderPrimitiveBase::HelpDrawAABB(const AABB& localAABB, const Vec3& pos, const Matrix33* pOrientation, const ColorF& color)
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
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
		// CRenderPrimitive_Sphere
		//
		//===================================================================================

		CRenderPrimitive_Sphere::CRenderPrimitive_Sphere()
			: m_pos(ZERO)
			, m_radius(0.0f)
			, m_color(Col_Black)
		{}

		CRenderPrimitive_Sphere::CRenderPrimitive_Sphere(const Vec3& pos, float radius, const ColorF& color)
			: m_pos(pos)
			, m_radius(radius)
			, m_color(color)
		{}

		void CRenderPrimitive_Sphere::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawSphere(m_pos, m_radius, m_color);
			}
		}

		size_t CRenderPrimitive_Sphere::GetRoughMemoryUsage() const
		{
			return sizeof(*this);
		}

		void CRenderPrimitive_Sphere::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_pos, "pos");
			ar(m_radius, "radius");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_Line
		//
		//===================================================================================

		CRenderPrimitive_Line::CRenderPrimitive_Line()
			: m_pos1(ZERO)
			, m_pos2(ZERO)
			, m_color(Col_Black)
		{}

		CRenderPrimitive_Line::CRenderPrimitive_Line(const Vec3& pos1, const Vec3& pos2, const ColorF& color)
			: m_pos1(pos1)
			, m_pos2(pos2)
			, m_color(color)
		{}

		void CRenderPrimitive_Line::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawLine(m_pos1, m_color, m_pos2, m_color, SCvars::debugDrawLineThickness);
			}
		}

		size_t CRenderPrimitive_Line::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CRenderPrimitive_Line::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_pos1, "pos1");
			ar(m_pos2, "pos2");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_Triangle
		//
		//===================================================================================

		CRenderPrimitive_Triangle::CRenderPrimitive_Triangle()
			: m_vtx1(ZERO)
			, m_vtx2(ZERO)
			, m_vtx3(ZERO)
			, m_color(Col_Black)
		{}

		CRenderPrimitive_Triangle::CRenderPrimitive_Triangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color)
			: m_vtx1(vtx1)
			, m_vtx2(vtx2)
			, m_vtx3(vtx3)
			, m_color(color)
		{}

		void CRenderPrimitive_Triangle::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				ColorB col(m_color);
				pAux->DrawTriangle(m_vtx1, col, m_vtx2, col, m_vtx3, col);
			}
		}

		size_t CRenderPrimitive_Triangle::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CRenderPrimitive_Triangle::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_vtx1, "vtx1");
			ar(m_vtx2, "vtx2");
			ar(m_vtx3, "vtx3");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_Text
		//
		//===================================================================================

		CRenderPrimitive_Text::CRenderPrimitive_Text()
			: m_pos(ZERO)
			, m_size(0.0f)
			, m_text()
			, m_color(Col_Black)
		{}

		CRenderPrimitive_Text::CRenderPrimitive_Text(const Vec3& pos, float size, const char* szText, const ColorF& color)
			: m_pos(pos)
			, m_size(size)
			, m_text(szText)
			, m_color(color)
		{}

		void CRenderPrimitive_Text::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				SDrawTextInfo ti;
				ti.scale.set(m_size, m_size);
				ti.flags = eDrawText_FixedSize;	// FIXME: do we need or want this?
				if (SCvars::debugDrawZTestOn)
				{
					ti.flags |= eDrawText_DepthTest;
				}
				ti.color[0] = m_color.r;
				ti.color[1] = m_color.g;
				ti.color[2] = m_color.b;
				ti.color[3] = m_color.a;
				pAux->RenderTextQueued(m_pos, ti, m_text.c_str());
			}
		}

		size_t CRenderPrimitive_Text::GetRoughMemoryUsage() const
		{
			// FIXME: the stack_string might have switched to the heap when having exceeded its capacity (but then again: does he deal with ref-counting?)
			return sizeof *this;
		}

		void CRenderPrimitive_Text::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_pos, "pos");
			ar(m_size, "size");
			ar(m_text, "text");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_Arrow
		//
		//===================================================================================

		CRenderPrimitive_Arrow::CRenderPrimitive_Arrow()
			: m_from(ZERO)
			, m_to(ZERO)
			, m_coneRadius(0.0f)
			, m_coneHeight(0.0f)
			, m_color(Col_Black)
		{}


		CRenderPrimitive_Arrow::CRenderPrimitive_Arrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color)
			: m_from(from)
			, m_to(to)
			, m_coneRadius(coneRadius)
			, m_coneHeight(coneHeight)
			, m_color(color)
		{}


		void CRenderPrimitive_Arrow::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawLine(m_from, m_color, m_to, m_color, SCvars::debugDrawLineThickness);
				Vec3 dir = (m_to - m_from).GetNormalizedSafe();
				pAux->DrawCone(m_to - dir * m_coneHeight, dir, m_coneRadius, m_coneHeight, m_color);
			}
		}

		size_t CRenderPrimitive_Arrow::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CRenderPrimitive_Arrow::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_from, "from");
			ar(m_to, "to");
			ar(m_coneRadius, "coneRadius");
			ar(m_coneHeight, "coneHeight");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_AABB
		//
		//===================================================================================

		CRenderPrimitive_AABB::CRenderPrimitive_AABB()
			: m_aabb()
			, m_color(Col_Black)
		{}


		CRenderPrimitive_AABB::CRenderPrimitive_AABB(const AABB& aabb, const ColorF& color)
			: m_aabb(aabb)
			, m_color(color)
		{}


		void CRenderPrimitive_AABB::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawAABB(m_aabb, false, m_color, eBBD_Faceted);
			}
		}

		size_t CRenderPrimitive_AABB::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CRenderPrimitive_AABB::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_aabb, "aabb");
			ar(m_color, "color");
		}

		//===================================================================================
		//
		// CRenderPrimitive_OBB
		//
		//===================================================================================

		CRenderPrimitive_OBB::CRenderPrimitive_OBB()
			: m_obb()
			, m_pos(ZERO)
			, m_color(Col_Black)
		{}


		CRenderPrimitive_OBB::CRenderPrimitive_OBB(const OBB& obb, const Vec3& pos, const ColorF& color)
			: m_obb(obb)
			, m_pos(pos)
			, m_color(color)
		{}


		void CRenderPrimitive_OBB::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawOBB(m_obb, m_pos, false, m_color, eBBD_Faceted);
			}
		}

		size_t CRenderPrimitive_OBB::GetRoughMemoryUsage() const
		{
			return sizeof(*this);
		}

		void CRenderPrimitive_OBB::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_obb.m33, "rotation");
			ar(m_obb.h, "half-vector");
			ar(m_obb.c, "center");
			ar(m_pos, "pos");
			ar(m_color, "color");
		}


		//===================================================================================
		//
		// CRenderPrimitive_Cylinder
		//
		//===================================================================================

		CRenderPrimitive_Cylinder::CRenderPrimitive_Cylinder()
			: m_pos(ZERO)
			, m_dir(ZERO)
			, m_radius(0.0f)
			, m_height(0.0f)
			, m_color(Col_Black)
		{}


		CRenderPrimitive_Cylinder::CRenderPrimitive_Cylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color)
			: m_pos(pos)
			, m_dir(dir)
			, m_radius(radius)
			, m_height(height)
			, m_color(color)
		{}


		void CRenderPrimitive_Cylinder::Draw() const
		{
			IRenderAuxGeom* pAux = GetRenderAuxGeom();
			if (pAux)
			{
				pAux->SetRenderFlags(GetFlags3D());
				pAux->DrawCylinder(m_pos, m_dir, m_radius, m_height, m_color);
			}
		}

		size_t CRenderPrimitive_Cylinder::GetRoughMemoryUsage() const
		{
			return sizeof *this;
		}

		void CRenderPrimitive_Cylinder::Serialize(Serialization::IArchive& ar)
		{
			CRenderPrimitiveBase::Serialize(ar);

			ar(m_pos, "pos");
			ar(m_dir, "dir");
			ar(m_radius, "radius");
			ar(m_height, "height");
			ar(m_color, "color");
		}

	}
}