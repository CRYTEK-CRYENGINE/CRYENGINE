// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderAuxGeom.h>
namespace Cry
{
	namespace SensorSystem
	{
		enum class ESensorShape
		{
			None = 0,
			AABB,
			OBB,
			Sphere,
			Vec3
		};

		class CSensorBounds
		{
		public:

			inline CSensorBounds()
				: m_shape(ESensorShape::None)
			{}

			explicit inline CSensorBounds(const AABB& value)
				: m_shape(ESensorShape::AABB)
			{
				new(m_storage) AABB(value);
			}

			explicit inline CSensorBounds(const OBB& value)
				: m_shape(ESensorShape::OBB)
			{
				new(m_storage) OBB(value);
			}

			explicit inline CSensorBounds(const Sphere& value)
				: m_shape(ESensorShape::Sphere)
			{
				new(m_storage) Sphere(value);
			}

			explicit inline CSensorBounds(const Vec3& value)
				: m_shape(ESensorShape::Vec3)
			{
				new(m_storage) Vec3(value);
			}

			inline CSensorBounds(const CSensorBounds& rhs)
				: m_shape(rhs.m_shape)
			{
				Copy(rhs);
			}

			inline ~CSensorBounds()
			{
				Release();
			}

			inline ESensorShape GetShape() const
			{
				return m_shape;
			}

			inline const AABB& AsAABB() const
			{
				CRY_ASSERT(m_shape == ESensorShape::AABB);
				return *reinterpret_cast<const AABB*>(m_storage);
			}

			inline const OBB& AsOBB() const
			{
				CRY_ASSERT(m_shape == ESensorShape::OBB);
				return *reinterpret_cast<const OBB*>(m_storage);
			}

			inline const Sphere& AsSphere() const
			{
				CRY_ASSERT(m_shape == ESensorShape::Sphere);
				return *reinterpret_cast<const Sphere*>(m_storage);
			}

			inline const Vec3& AsVec3() const
			{
				CRY_ASSERT(m_shape == ESensorShape::Vec3);
				return *reinterpret_cast<const Vec3*>(m_storage);
			}

			inline AABB ToAABB() const
			{
				switch (m_shape)
				{
					case ESensorShape::AABB:
					{
						return AsAABB();
					}
					case ESensorShape::OBB:
					{
						return AABB::CreateAABBfromOBB(Vec3(ZERO), AsOBB());
					}
					case ESensorShape::Sphere:
					{
						const Sphere sphere = AsSphere();
						return AABB(sphere.center, sphere.radius);
					}
					case ESensorShape::Vec3:
					{
						return AABB(AsVec3(), AsVec3());
					}
				}
				return AABB(ZERO);
			}

			inline Vec3 GetCenter() const
			{
				switch (m_shape)
				{
					case ESensorShape::AABB:
					{
						return AsAABB().GetCenter();
					}
					case ESensorShape::OBB:
					{
						return AsOBB().c;
					}
					case ESensorShape::Sphere:
					{
						return AsSphere().center;
					}
					case ESensorShape::Vec3:
					{
						return AsVec3();
					}
				}
				return Vec3(ZERO);
			}

			inline bool Overlap(const CSensorBounds& rhs) const
			{
				switch (m_shape)
				{
					case ESensorShape::AABB:
					{
						switch (rhs.m_shape)
						{
							case ESensorShape::AABB:
							{
								return Overlap::AABB_AABB(AsAABB(), rhs.AsAABB());
							}
							case ESensorShape::OBB:
							{
								return Overlap::AABB_OBB(AsAABB(), Vec3(ZERO), rhs.AsOBB());
							}
							case ESensorShape::Sphere:
							{
								return Overlap::Sphere_AABB(rhs.AsSphere(), AsAABB());
							}
							case ESensorShape::Vec3:
							{
								return Overlap::Point_AABB(rhs.AsVec3(), AsAABB());
							}
						}
						break;
					}
					case ESensorShape::OBB:
					{
						switch (rhs.m_shape)
						{
							case ESensorShape::AABB:
							{
								return Overlap::AABB_OBB(rhs.AsAABB(), Vec3(ZERO), AsOBB());
							}
							case ESensorShape::OBB:
							{
								return Overlap::OBB_OBB(Vec3(ZERO), AsOBB(), Vec3(ZERO), rhs.AsOBB());
							}
							case ESensorShape::Sphere:
							{
								return Overlap::Sphere_OBB(rhs.AsSphere(), AsOBB());
							}
							case ESensorShape::Vec3:
							{
								return Overlap::Point_OBB(rhs.AsVec3(), Vec3(ZERO), AsOBB());
							}
						}
						break;
					}
					case ESensorShape::Sphere:
					{
						switch (rhs.m_shape)
						{
							case ESensorShape::AABB:
							{
								return Overlap::Sphere_AABB(AsSphere(), rhs.AsAABB());
							}
							case ESensorShape::OBB:
							{
								return Overlap::Sphere_OBB(AsSphere(), rhs.AsOBB());
							}
							case ESensorShape::Sphere:
							{
								return Overlap::Sphere_Sphere(AsSphere(), rhs.AsSphere());
							}
							case ESensorShape::Vec3:
							{
								return Overlap::Point_Sphere(rhs.AsVec3(), AsSphere());
							}
						}
						break;
					}
					case ESensorShape::Vec3:
					{
						switch (rhs.m_shape)
						{
							case ESensorShape::AABB:
							{
								return Overlap::Point_AABB(AsVec3(), rhs.AsAABB());
							}
							case ESensorShape::OBB:
							{
								return Overlap::Point_OBB(AsVec3(), Vec3(ZERO), rhs.AsOBB());
							}
							case ESensorShape::Sphere:
							{
								return Overlap::Point_Sphere(AsVec3(), rhs.AsSphere());
							}
							// N.B. No case for ESensorShape::Vec3 because we should never need to detect overlapping points.
						}
						break;
					}
				}
				return false;
			}

			inline void DebugDraw(const ColorB& color) const
			{
				IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();

				switch (m_shape)
				{
					case ESensorShape::AABB:
					{
						renderAuxGeom.DrawAABB(AsAABB(), Matrix34(IDENTITY), true, color, eBBD_Faceted);
						break;
					}
					case ESensorShape::OBB:
					{
						renderAuxGeom.DrawOBB(AsOBB(), Matrix34(IDENTITY), false, color, eBBD_Faceted);
						break;
					}
					case ESensorShape::Sphere:
					{
						SAuxGeomRenderFlags prevRenderFlags = renderAuxGeom.GetRenderFlags();
						renderAuxGeom.SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);

						const Sphere sphere = AsSphere();
						gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(sphere.center, sphere.radius, ColorB(color.r, color.g, color.b, 64), false);

						renderAuxGeom.SetRenderFlags(prevRenderFlags);
						break;
					}
					case ESensorShape::Vec3:
					{
						renderAuxGeom.DrawSphere(AsVec3(), 0.1f, color, false);
						break;
					}
				}
			}

			inline void operator=(const CSensorBounds& rhs)
			{
				Release();
				Copy(rhs);
			}

		private:

			inline void Copy(const CSensorBounds& rhs)
			{
				m_shape = rhs.m_shape;
				switch (rhs.m_shape)
				{
					case ESensorShape::AABB:
					{
						new(m_storage) AABB(rhs.AsAABB());
						break;
					}
					case ESensorShape::OBB:
					{
						new(m_storage) OBB(rhs.AsOBB());
						break;
					}
					case ESensorShape::Sphere:
					{
						new(m_storage) Sphere(rhs.AsSphere());
						break;
					}
					case ESensorShape::Vec3:
					{
						new(m_storage) Vec3(rhs.AsVec3());
						break;
					}
				}
			}

			inline void Release()
			{
				switch (m_shape)
				{
					case ESensorShape::AABB:
					{
						reinterpret_cast<const AABB*>(m_storage)->~AABB();
						break;
					}
					case ESensorShape::OBB:
					{
						reinterpret_cast<const OBB*>(m_storage)->~OBB();
						break;
					}
					case ESensorShape::Sphere:
					{
						reinterpret_cast<const Sphere*>(m_storage)->~Sphere();
						break;
					}
					case ESensorShape::Vec3:
					{
						reinterpret_cast<const Vec3*>(m_storage)->~Vec3();
						break;
					}
				}
			}

		private:

			ESensorShape m_shape;
			uint8        m_storage[sizeof(OBB)]; // #TODO : Use variant type?
		};
	}
}