// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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

		class CRenderPrimitiveBase
		{
		public:
			explicit                CRenderPrimitiveBase();

			const CTimeMetadata&    GetMetadata() const;

			virtual                 ~CRenderPrimitiveBase() {}
			virtual void            Draw() const = 0;
			virtual size_t          GetRoughMemoryUsage() const = 0;
			virtual void            Serialize(Serialization::IArchive& ar);

		protected:

			static IRenderAuxGeom*  GetRenderAuxGeom();
			static int              GetFlags3D();    // takes care of the z-test flag via cvar
			static void             HelpDrawAABB(const AABB& localAABB, const Vec3& pos, const Matrix33* pOrientation, const ColorF& color);

		private:

			CTimeMetadata           m_metadata;
		};

		//===================================================================================
		//
		// CRenderPrimitive_Sphere
		//
		//===================================================================================

		class CRenderPrimitive_Sphere final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Sphere();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Sphere(const Vec3& pos, float radius, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_pos;
			float                   m_radius;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Sphere, "CRenderPrimitive_Sphere", "");

		//===================================================================================
		//
		// CRenderPrimitive_Line
		//
		//===================================================================================

		class CRenderPrimitive_Line final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Line();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Line(const Vec3& pos1, const Vec3& pos2, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_pos1;
			Vec3                    m_pos2;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Line, "CRenderPrimitive_Line", "");

		//===================================================================================
		//
		// CRenderPrimitive_Triangle
		//
		//===================================================================================

		class CRenderPrimitive_Triangle final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Triangle();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Triangle(const Vec3& vtx1, const Vec3& vtx2, const Vec3& vtx3, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_vtx1;
			Vec3                    m_vtx2;
			Vec3                    m_vtx3;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Triangle, "CRenderPrimitive_Triangle", "");

		//===================================================================================
		//
		// CRenderPrimitive_Text
		//
		//===================================================================================

		class CRenderPrimitive_Text final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Text();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Text(const Vec3& pos, float size, const char* szText, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_pos;
			float                   m_size;
			stack_string            m_text;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Text, "CRenderPrimitive_Text", "");

		//===================================================================================
		//
		// CRenderPrimitive_Arrow
		//
		//===================================================================================

		class CRenderPrimitive_Arrow final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Arrow();   // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Arrow(const Vec3& from, const Vec3& to, float coneRadius, float coneHeight, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_from;
			Vec3                    m_to;
			float                   m_coneRadius;
			float                   m_coneHeight;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Arrow, "CRenderPrimitive_Arrow", "");

		//===================================================================================
		//
		// CRenderPrimitive_AABB
		//
		//===================================================================================

		class CRenderPrimitive_AABB final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_AABB();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_AABB(const AABB& aabb, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			AABB                    m_aabb;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_AABB, "CRenderPrimitive_AABB", "");

		//===================================================================================
		//
		// CRenderPrimitive_OBB
		//
		//===================================================================================

		class CRenderPrimitive_OBB final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_OBB(); // default ctor required for yasli serialization
			explicit                CRenderPrimitive_OBB(const OBB& obb, const Vec3& pos, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:
			OBB                     m_obb;
			Vec3                    m_pos;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_OBB, "CRenderPrimitive_OBB", "");

		//===================================================================================
		//
		// CRenderPrimitive_Cylinder
		//
		//===================================================================================

		class CRenderPrimitive_Cylinder final : public CRenderPrimitiveBase
		{
		public:

			explicit                CRenderPrimitive_Cylinder();    // default ctor required for yasli serialization
			explicit                CRenderPrimitive_Cylinder(const Vec3& pos, const Vec3& dir, const float radius, const float height, const ColorF& color);

			// CRenderPrimitiveBase
			virtual void            Draw() const override;
			virtual size_t          GetRoughMemoryUsage() const override;
			virtual void            Serialize(Serialization::IArchive& ar) override;
			// ~CRenderPrimitiveBase

		private:

			Vec3                    m_pos;
			Vec3                    m_dir;
			float                   m_radius;
			float                   m_height;
			ColorF                  m_color;
		};
		SERIALIZATION_CLASS_NAME(CRenderPrimitiveBase, CRenderPrimitive_Cylinder, "CRenderPrimitive_Cylinder", "");

	}
}