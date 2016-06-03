// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PatHolder.h
   $Id$
   Description: Interface to hold an manipulate a path

   -------------------------------------------------------------------------
   History:
   - 30/11/2011 : Created by Francesco Roccucci

 *********************************************************************/

#ifndef __PathHolder_h__
#define __PathHolder_h__

#pragma once

#include "NavPath.h"
#include <CryAISystem/INavigationSystem.h>

template<class T>
class CPathHolder
{
public:
	typedef std::vector<T> PathHolderPath;

	CPathHolder()
	{
		Reset();
	}

	void PushBack(const T& element)
	{
		m_path.push_back(element);
	}

	void PushFront(const T& element)
	{
		m_path.insert(m_path.begin(), element);
	}

	void Reset()
	{
		m_path.clear();
	}

	const PathHolderPath&                     GetPath() const { return m_path; }
	inline typename PathHolderPath::size_type Size() const    { return m_path.size(); }

	void                                      PullPathOnNavigationMesh(const NavigationMeshID meshID, uint16 iteration, const MNM::WayTriangleData* way, const size_t maxLenght)
	{
		FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

		/*
		   MNM::WayTriangleData* way is a pointer to an array of maxLenght
		   elements. It's important to remember that the FindWay returns
		   that array filled in the opposite order (the first triangle
		   is the one in the last position)
		 */
		if (m_path.size() < 3)
			return;

		const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
		const MNM::MeshGrid& grid = mesh.grid;
		const Vec3 gridOrigin = grid.GetParams().origin;

		for (uint16 i = 0; i < iteration; ++i)
		{
			typename PathHolderPath::reverse_iterator it = m_path.rbegin();
			typename PathHolderPath::reverse_iterator end = m_path.rend();

			// Each triplets is associated with two triangles so the way length
			// should match the iteration through the path points
			size_t currentPosition = 0;
			for (; it + 2 != end && currentPosition < maxLenght; ++it)
			{
				const T& startPoint = *it;
				T& middlePoint = *(it + 1);
				const T& endPoint = *(it + 2);
				// Let's pull only the triplets that are fully not interacting with off-mesh links
				if ((middlePoint.offMeshLinkData.offMeshLinkID == MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID) &&
				    (endPoint.offMeshLinkData.offMeshLinkID == MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID))
				{

					const Vec3& from = startPoint.vPos;
					const Vec3& to = endPoint.vPos;
					Vec3& middle = middlePoint.vPos;

					MNM::vector3_t startLocation(from - gridOrigin);
					MNM::vector3_t middleLocation(middle - gridOrigin);
					MNM::vector3_t endLocation(to - gridOrigin);

					grid.PullString(startLocation, way[currentPosition].triangleID, endLocation, way[currentPosition + 1].triangleID, middleLocation);
					middle = middleLocation.GetVec3() + gridOrigin;
				}

				// On the exit the smart object the middle point of the triangle
				// is added in the path, so when the middle point is a smart object
				// it means we are still in the previous triangle (the way is from the end to the start)
				if (middlePoint.offMeshLinkData.offMeshLinkID == MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID)
					++currentPosition;
			}
		}
	}

	void FillNavPath(INavPath& navPath)
	{
		IF_UNLIKELY (m_path.empty())
			return;

		typename PathHolderPath::const_iterator it = m_path.begin();
		typename PathHolderPath::const_iterator end = m_path.end();
		for (; it != end; ++it)
		{
			navPath.PushBack(*it);
		}

		// Make sure the end point is inserted.
		if (navPath.Empty())
			navPath.PushBack(m_path.back(), true);
	}

	Vec3 At(typename PathHolderPath::size_type pathPosition) const
	{
		Vec3 pos(ZERO);
		if (pathPosition < m_path.size())
		{
			pos = m_path[pathPosition].vPos;
		}
		else
		{
			assert(0);
		}
		return pos;
	}

private:
	PathHolderPath m_path;
};

#endif // __PathHolder_h__
