// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/MNMTile.h>
#include <CrySerialization/TypeID.h>

//! INavMeshQueryFilter is a structure used in navigation queries (finding path, ray-cast, query triangles, etc...)
//! for filtering passable triangles and computing costs of traversing triangles.
//! 
//! Asynchronous jobs inside Navigation System can create a copy of the filter,
//! therefore filter implementations need to provide overridden Clone() and Release() functions. 
struct INavMeshQueryFilter
{
	virtual ~INavMeshQueryFilter() {}

	//! Clone method that should return new deep copy of the object.
	virtual INavMeshQueryFilter* Clone() const = 0;

	//! Release method used for destroying the object.
	virtual void Release() const = 0;

	//! Returns false if the annotation isn't passable.
	virtual bool PassFilter(const MNM::AreaAnnotation annotation) const = 0;

	//! Returns false if the triangle isn't passable.
	virtual bool PassFilter(const MNM::Tile::STriangle& triangle) const = 0;

	//! Returns cost multiplier for traversing the triangle.
	virtual float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const = 0;

	//! Returns cost multiplier for the given annotation.
	virtual float GetCostMultiplier(const MNM::AreaAnnotation annotation) const = 0;

	//! Currently not used but planned to be used in the future instead of GetCostMultiplier.
	virtual MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
		const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
		const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const = 0;

	//! Function for comparing with other filters. Returns true if both filters are of the same type and contain same data.
	virtual bool Compare(const INavMeshQueryFilter* pOther) const = 0;

	//! Returns true if both filters are of the same type.
	bool IsSameType(const INavMeshQueryFilter* pOther) const
	{
		return pOther && (m_typeID == pOther->m_typeID);
	}

protected:
	INavMeshQueryFilter(const Serialization::TypeID& typeID)
		: m_typeID(typeID)
	{}

private:
	const Serialization::TypeID m_typeID;
};

DECLARE_SHARED_POINTERS(INavMeshQueryFilter);

//! Base class for all filter classes that will be created.
//! Flag masks with combination of triangle flags are used for filtering passable triangles. All cost multipliers are 1.0 here.
//! TDerivedFilter type has to be the actually derived class (see NavMeshQueryFilterDefault.h for example).
template <class TDerivedFilter>
struct SNavMeshQueryFilterBase : public INavMeshQueryFilter
{
	typedef MNM::AreaAnnotation::value_type ValueType;

	SNavMeshQueryFilterBase& operator=(const SNavMeshQueryFilterBase& other)
	{
		includeFlags = other.includeFlags;
		excludeFlags = other.excludeFlags;
		return *this;
	}

	bool operator==(const SNavMeshQueryFilterBase& other) const
	{
		return includeFlags == other.includeFlags && excludeFlags == other.excludeFlags;
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
		return 1.0f;
	}

	virtual float GetCostMultiplier(const MNM::AreaAnnotation annotation) const override
	{
		return 1.0f;
	}

	virtual MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
		const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
		const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const override
	{
		return (fromPos - toPos).lenNoOverflow();
	}

	virtual bool Compare(const INavMeshQueryFilter* pOther) const override final
	{
		if (!IsSameType(pOther))
			return false;

		return static_cast<const TDerivedFilter&>(*this) == static_cast<const TDerivedFilter&>(*pOther);
	}

	void SetIncludeFlags(const ValueType flags) { includeFlags = flags; }
	ValueType GetIncludeFlags() const { return includeFlags; }

	void SetExcludeFlags(const ValueType flags) { excludeFlags = flags; }
	ValueType GetExcludeFlags() const { return excludeFlags; }

protected:
	SNavMeshQueryFilterBase()
		: INavMeshQueryFilter(Serialization::TypeID::get<TDerivedFilter>())
		, includeFlags(-1)
		, excludeFlags(0)
	{}

	SNavMeshQueryFilterBase(const SNavMeshQueryFilterBase& other)
		: INavMeshQueryFilter(other)
		, includeFlags(other.includeFlags)
		, excludeFlags(other.excludeFlags)
	{}

public:
	ValueType includeFlags;
	ValueType excludeFlags;
};
