// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemDebugProxies.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		static const float MAX_ITEM_DEBUG_PROXY_DISTANCE_FROM_CAMERA = 1000.0f;

		//===================================================================================
		//
		// CItemDebugProxy_Sphere
		//
		//===================================================================================

		CItemDebugProxy_Sphere::CItemDebugProxy_Sphere()
			: m_pos(ZERO)
			, m_radius(0.0f)
		{}

		void CItemDebugProxy_Sphere::SetPosAndRadius(const Vec3& pos, float radius)
		{
			m_pos = pos;
			m_radius = radius;
		}

		Vec3 CItemDebugProxy_Sphere::GetPivot() const
		{
			return m_pos;
		}

		bool CItemDebugProxy_Sphere::GetDistanceToCameraView(const SDebugCameraView& cameraView, float& dist) const
		{
			const Lineseg lineseg(cameraView.pos + cameraView.dir * 1.0f, cameraView.pos + cameraView.dir * MAX_ITEM_DEBUG_PROXY_DISTANCE_FROM_CAMERA);

			// behind?
			if ((m_pos - lineseg.start).Dot(cameraView.dir) < 0.0f)
				return false;

			// too far ahead?
			if ((m_pos - lineseg.end).Dot(cameraView.dir) > 0.0f)
				return false;

			float t;
			dist = Distance::Point_Lineseg(m_pos, lineseg, t);

			// too far offside the view direction?
			if (dist > m_radius + 0.5f)
				return false;

			return true;
		}

		void CItemDebugProxy_Sphere::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pos, "m_pos");
			ar(m_radius, "m_radius");
		}

		//===================================================================================
		//
		// CItemDebugProxy_Path
		//
		//===================================================================================

		CItemDebugProxy_Path::CItemDebugProxy_Path()
			: m_bClosed(false)
		{}

		void CItemDebugProxy_Path::AddPoint(const Vec3& point)
		{
			m_pathPoints.push_back(point);
		}

		void CItemDebugProxy_Path::SetClosed(bool bClosed)
		{
			m_bClosed = bClosed;
		}

		Vec3 CItemDebugProxy_Path::GetPivot() const
		{
			return m_pathPoints.empty() ? Vec3(0, 0, 0) : m_pathPoints[0];
		}

		bool CItemDebugProxy_Path::GetDistanceToCameraView(const SDebugCameraView& cameraView, float& dist) const
		{
			bool foundValidPathSegment = false;

			if (!m_pathPoints.empty())
			{
				const Lineseg linesegOfCameraView(cameraView.pos + cameraView.dir * 1.0f, cameraView.pos + cameraView.dir * MAX_ITEM_DEBUG_PROXY_DISTANCE_FROM_CAMERA);
				const size_t pointCount = m_pathPoints.size();
				float distanceOfClosestPathSegmentSoFar = std::numeric_limits<float>::max();

				for (size_t i = 0; i < pointCount; ++i)
				{
					if (i < pointCount - 1 || m_bClosed)
					{
						const Vec3& p1 = m_pathPoints[i];
						const Vec3& p2 = m_pathPoints[(i + 1) % pointCount];

						// fully behind camera?
						if((p1 - linesegOfCameraView.start).Dot(cameraView.dir) < 0.0f && (p2 - linesegOfCameraView.start).Dot(cameraView.dir) < 0.0f)
							continue;

						// fully ahead of a reasonable distance?
						if ((p1 - linesegOfCameraView.end).Dot(cameraView.dir) > 0.0f && (p2 - linesegOfCameraView.end).Dot(cameraView.dir) > 0.0f)
							continue;

						const Lineseg linesegOfPathSegment(p1, p2);
						const float distance = Distance::Lineseg_Lineseg<float>(linesegOfCameraView, linesegOfPathSegment, nullptr, nullptr);

						// too far offside the view direction?
						if(distance > 0.5f)
							continue;

						distanceOfClosestPathSegmentSoFar = std::min(distanceOfClosestPathSegmentSoFar, distance);
						foundValidPathSegment = true;
					}
				}

				if (foundValidPathSegment)
				{
					dist = distanceOfClosestPathSegmentSoFar;
				}
			}

			return foundValidPathSegment;
		}

		void CItemDebugProxy_Path::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pathPoints, "m_pathPoints");
			ar(m_bClosed, "m_bClosed");
		}

		//===================================================================================
		//
		// CItemDebugProxy_AABB
		//
		//===================================================================================

		CItemDebugProxy_AABB::CItemDebugProxy_AABB()
			: m_aabb(AABB::RESET)
		{}

		void CItemDebugProxy_AABB::SetAABB(const AABB& aabb)
		{
			m_aabb = aabb;
		}

		Vec3 CItemDebugProxy_AABB::GetPivot() const
		{
			return m_aabb.GetCenter();
		}

		bool CItemDebugProxy_AABB::GetDistanceToCameraView(const SDebugCameraView& cameraView, float& dist) const
		{
			const Lineseg lineseg(cameraView.pos + cameraView.dir * 1.0f, cameraView.pos + cameraView.dir * MAX_ITEM_DEBUG_PROXY_DISTANCE_FROM_CAMERA);
			dist = CGeomUtil::Distance_Lineseg_AABB(lineseg, m_aabb);

			// accept only if "close enough" (arbitrarily chosen threshold)
			return dist < 1.0f;
		}

		void CItemDebugProxy_AABB::Serialize(Serialization::IArchive& ar)
		{
			ar(m_aabb, "m_aabb");
		}

	}
}
