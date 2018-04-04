// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

	void                                      PullPathOnNavigationMesh(const MNM::CNavMesh& navMesh, uint16 iteration)
	{
		FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

		if (m_path.size() < 3)
			return;

		const Vec3 gridOrigin = navMesh.GetGridParams().origin;

		for (uint16 i = 0; i < iteration; ++i)
		{
			typename PathHolderPath::reverse_iterator it = m_path.rbegin();
			typename PathHolderPath::reverse_iterator end = m_path.rend();

			// Each triplets is associated with two triangles so the way length
			// should match the iteration through the path points
			for (; it + 2 != end; ++it)
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

					navMesh.PullString(startLocation, startPoint.iTriId, endLocation, middlePoint.iTriId, middleLocation);
					middle = middleLocation.GetVec3() + gridOrigin;
				}
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
