// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include "MNMTile.h"

namespace MNM
{
namespace Tile
{
	struct STriangle;
}
}

//! INavMeshQueryFilter is structure used in navigation queries (finding path, raycast, query triangles...)
//! for filtering passable triangles and computing costs of traversing triangles
//! 
//! Asynchronous jobs inside Navigation System can create a copy of the filter,
//! therefore filter implementations need to provide overridden Clone() and Release() functions. 
struct INavMeshQueryFilter
{
	virtual ~INavMeshQueryFilter() {}
	
	//! Clone method that should return new deep copy of the object
	virtual INavMeshQueryFilter* Clone() const = 0;
	
	//! Release method used for destroying the object
	virtual void Release() const = 0;

	//! Returns false if the annotation isn't passable
	virtual bool PassFilter(const MNM::AreaAnnotation annotation) const = 0;

	//! Returns false if the triangle isn't passable
	virtual bool PassFilter(const MNM::Tile::STriangle& triangle) const = 0;

	//! Returns cost multiplier for traversing the triangle
	virtual float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const = 0;

	//! Currently not used but planned be used in the future instead of GetCostMultiplier
	virtual MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
		const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
		const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const = 0;
};

DECLARE_SHARED_POINTERS(INavMeshQueryFilter);

//! Default base implementation of INavMeshQueryFilter
//! Flag masks with combination with triangle flags are used for filtering passable triangles
//! and area costs multipliers for each triangle area type are used for computing the costs
struct SNavMeshQueryFilterDefaultBase : public INavMeshQueryFilter
{
	typedef MNM::AreaAnnotation::value_type ValueType;
	enum { MaxAreasCount = MNM::AreaAnnotation::MaxAreasCount() };

	SNavMeshQueryFilterDefaultBase()
		: includeFlags(-1)
		, excludeFlags(0)
	{
		for (size_t i = 0; i < MaxAreasCount; ++i)
		{
			costs[i] = 1.0f;
		}
	}
	SNavMeshQueryFilterDefaultBase(const SNavMeshQueryFilterDefaultBase& other)
		: includeFlags(other.includeFlags)
		, excludeFlags(other.excludeFlags)
	{
		memcpy(costs, other.costs, sizeof(costs[0]) * MaxAreasCount);
	}

	virtual bool PassFilter(const MNM::AreaAnnotation annotation) const override
	{
		return (annotation.GetFlags() & includeFlags) && !(annotation.GetFlags() & excludeFlags);
	}

	virtual bool PassFilter(const MNM::Tile::STriangle& triangle) const override
	{
		return (triangle.areaAnnotation.GetFlags() & includeFlags) && !(triangle.areaAnnotation.GetFlags() & excludeFlags);
	}

	virtual float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const override
	{
		CRY_ASSERT(triangle.areaAnnotation.GetType() < MaxAreasCount);
		return costs[triangle.areaAnnotation.GetType()];
	}

	virtual MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
		const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
		const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const override
	{
		CRY_ASSERT(currentTriangle.areaAnnotation.GetType() < MaxAreasCount);
		return (fromPos - toPos).lenNoOverflow() * (MNM::real_t)costs[currentTriangle.areaAnnotation.GetType()];
	}

	void SetAreaCost(const ValueType areaIdx, const float cost)
	{
		CRY_ASSERT(areaIdx >= 0 && areaIdx < MaxAreasCount);
		costs[areaIdx] = cost;
	}
	float GetAreaCost(const ValueType areaIdx) const
	{
		CRY_ASSERT(areaIdx >= 0 && areaIdx < MaxAreasCount);
		return costs[areaIdx];
	}

	void SetIncludeFlags(const ValueType flags) { includeFlags = flags; }
	ValueType GetIncludeFlags() const { return includeFlags; }

	void SetExcludeFlags(const ValueType flags) { excludeFlags = flags; }
	ValueType GetExcludeFlags() const { return excludeFlags; }

	ValueType includeFlags;
	ValueType excludeFlags;

	float costs[MaxAreasCount];
};

//! Default final implementation of INavMeshQueryFilter used in AI System
//! Flag masks with combination with triangle flags are used for filtering passable triangles
//! and area costs multipliers for each triangle area type are used for computing the costs
struct SNavMeshQueryFilterDefault final : public SNavMeshQueryFilterDefaultBase
{
	virtual INavMeshQueryFilter* Clone() const override
	{
		return new SNavMeshQueryFilterDefault(*this);
	}

	virtual void Release() const override
	{
		delete this;
	}

	bool operator==(const SNavMeshQueryFilterDefault& other) const
	{
		return includeFlags == other.includeFlags && excludeFlags == other.excludeFlags
			&& !memcmp(costs, other.costs, sizeof(costs[0]) * MaxAreasCount);
	}
};

//! \endcond