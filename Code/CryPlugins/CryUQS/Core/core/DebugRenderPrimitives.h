// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderAuxGeom.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDebugRenderPrimitiveBase
		//
		//===================================================================================

		class CDebugRenderPrimitiveBase
		{
		public:
			virtual            ~CDebugRenderPrimitiveBase() {}
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const = 0;
			virtual size_t     GetRoughMemoryUsage() const = 0;
			virtual void       Serialize(Serialization::IArchive& ar) = 0;

			static float       Pulsate();       // returns a value between [1.0f .. 2.0f] by mapping the current global time onto a sinus wave

		protected:
			static int         GetFlags3D();    // takes care of the z-test flag via cvar
			static void        HelpDrawAABB(const AABB& localAABB, const Vec3& pos, const Matrix33* pOrientation, const ColorF& color, bool bHighlight);
		};

		//===================================================================================
		//
		// CDebugRenderPrimitive_Sphere
		//
		//===================================================================================

		class CDebugRenderPrimitive_Sphere : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Sphere();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Sphere(const Vec3& pos, float radius, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos, float radius, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos;
			float              m_radius;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Sphere, "CDebugRenderPrimitive_Sphere", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Direction
		//
		//===================================================================================

		class CDebugRenderPrimitive_Direction : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Direction();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Direction(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_from;
			Vec3               m_to;
			float              m_coneRadius;
			float              m_coneHeight;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Direction, "CDebugRenderPrimitive_Direction", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Line
		//
		//===================================================================================

		class CDebugRenderPrimitive_Line : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Line();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Line(const Vec3& pos1, const Vec3& pos2, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos1, const Vec3& pos2, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos1;
			Vec3               m_pos2;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Line, "CDebugRenderPrimitive_Line", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Cone
		//
		//===================================================================================

		class CDebugRenderPrimitive_Cone : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Cone();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Cone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos;
			Vec3               m_dir;
			float              m_baseRadius;
			float              m_height;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Cone, "CDebugRenderPrimitive_Cone", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Cylinder
		//
		//===================================================================================

		class CDebugRenderPrimitive_Cylinder : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Cylinder();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Cylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos;
			Vec3               m_dir;
			float              m_radius;
			float              m_height;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Cylinder, "CDebugRenderPrimitive_Cylinder", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Text
		//
		//===================================================================================

		class CDebugRenderPrimitive_Text : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Text();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Text(const Vec3& pos, float size, const char* szText, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos, float size, const char* szText, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos;
			float              m_size;
			stack_string       m_text;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Text, "CDebugRenderPrimitive_Text", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_Quat
		//
		//===================================================================================

		class CDebugRenderPrimitive_Quat : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_Quat();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_Quat(const Vec3& pos, const Quat& q, float r, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const Vec3& pos, const Quat& q, float r, const ColorF& color, bool bHighlight);

		private:
			Vec3               m_pos;
			Quat               m_quat;
			float              m_radius;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_Quat, "CDebugRenderPrimitive_Quat", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_AABB
		//
		//===================================================================================

		class CDebugRenderPrimitive_AABB : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_AABB();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_AABB(const AABB& aabb, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const AABB& aabb, const ColorF& color, bool bHighlight);

		private:
			AABB               m_aabb;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_AABB, "CDebugRenderPrimitive_AABB", "");

		//===================================================================================
		//
		// CDebugRenderPrimitive_OBB
		//
		//===================================================================================

		class CDebugRenderPrimitive_OBB : public CDebugRenderPrimitiveBase
		{
		public:
			explicit           CDebugRenderPrimitive_OBB();    // default ctor required for yasli serialization
			explicit           CDebugRenderPrimitive_OBB(const OBB& obb, const ColorF& color);

			// CDebugRenderPrimitiveBase
			virtual void       Draw(bool bHighlight, const ColorF* pOptionalColorToOverrideWith) const override;
			virtual size_t     GetRoughMemoryUsage() const override;
			virtual void       Serialize(Serialization::IArchive& ar) override;
			// ~CDebugRenderPrimitiveBase

			static void        Draw(const OBB& obb, const ColorF& color, bool bHighlight);

		private:
			OBB                m_obb;
			ColorF             m_color;
		};
		SERIALIZATION_CLASS_NAME(CDebugRenderPrimitiveBase, CDebugRenderPrimitive_OBB, "CDebugRenderPrimitive_OBB", "");

	}
}
