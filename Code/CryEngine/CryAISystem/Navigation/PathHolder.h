// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
private:

	// functors for deciding whether to string-pull the path

	class CStringPullAlways
	{
	public:
		bool ShouldStringPullPath(const Vec3& from, const Vec3& to) const
		{
			return true;
		}
	};

	class CCheckWithPathCostComputerForStringPulling
	{
	public:
		explicit CCheckWithPathCostComputerForStringPulling(const IMNMCustomPathCostComputer& pathCostComputer)
			: m_pathCostComputer(pathCostComputer)
		{}

		bool ShouldStringPullPath(const Vec3& from, const Vec3& to) const
		{
			const IMNMCustomPathCostComputer::SComputationInput input(m_flags, from, to);
			IMNMCustomPathCostComputer::SComputationOutput output;

			m_pathCostComputer.ComputeCostThreadUnsafe(input, output);

			return output.bStringPullingAllowed;
		}

	private:
		const IMNMCustomPathCostComputer& m_pathCostComputer;
		const IMNMCustomPathCostComputer::ComputationFlags m_flags{ IMNMCustomPathCostComputer::EComputationType::StringPullingAllowed };
	};

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

	void                                      PullPathOnNavigationMesh(const MNM::CNavMesh& navMesh, uint16 iteration, const IMNMCustomPathCostComputer* pOptionalCustomPathCostComputer)
	{
		if (pOptionalCustomPathCostComputer)
		{
			StringPullLoop(navMesh, iteration, CCheckWithPathCostComputerForStringPulling(*pOptionalCustomPathCostComputer));
		}
		else
		{
			StringPullLoop(navMesh, iteration, CStringPullAlways());
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

	template <class TStringPullChecker>
	void StringPullLoop(const MNM::CNavMesh& navMesh, uint16 iteration, const TStringPullChecker& stringPullChecker)
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

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
					const bool bPerformStringPulling = stringPullChecker.ShouldStringPullPath(from, to);

					if (bPerformStringPulling)
					{
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
	}

private:
	PathHolderPath m_path;
};

#endif // __PathHolder_h__
