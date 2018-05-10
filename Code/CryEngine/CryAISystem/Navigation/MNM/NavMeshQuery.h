// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryAISystem/NavigationSystem/INavigationQuery.h>

namespace MNM
{
class CNavMesh;

class CTriangleAtQuery : public INavMeshQueryProcessing
{
public:
	CTriangleAtQuery(const CNavMesh* pNavMesh, const vector3_t& localPosition)
		: pNavMesh(pNavMesh)
		, location(localPosition)
	{}

	const MNM::TriangleID GetTriangleId() const { return closestID; }

	virtual EResult operator() (const MNM::TileID tileId, const Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount) override;

private:
	const CNavMesh* pNavMesh;

	vector3_t location;
	TriangleID closestID = MNM::Constants::InvalidTriangleID;
	MNM::real_t::unsigned_overflow_type distMinSq = std::numeric_limits<MNM::real_t::unsigned_overflow_type>::max();
};

class CNearestTriangleQuery : public INavMeshQueryProcessing
{
public:
	CNearestTriangleQuery(const CNavMesh* pNavMesh, const vector3_t& localFromPosition)
		: pNavMesh(pNavMesh)
		, localFromPosition(localFromPosition)
	{}

	const vector3_t& GetClosestPosition() const { return closestPos; }
	const real_t GetClosestDistance() const { return real_t::sqrtf(distMinSq); }
	const MNM::TriangleID GetClosestTriangleId() const { return closestID; }

	virtual EResult operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount) override;

private:
	const CNavMesh* pNavMesh;
	const vector3_t localFromPosition;

	TriangleID closestID = MNM::Constants::InvalidTriangleID;
	vector3_t closestPos = real_t::max();
	MNM::real_t::unsigned_overflow_type distMinSq = std::numeric_limits<MNM::real_t::unsigned_overflow_type>::max();
};

class CGetMeshBordersQueryWithFilter : public INavMeshQueryProcessing
{
public:
	CGetMeshBordersQueryWithFilter(const CNavMesh* pNavMesh, Vec3* pBordersWithNormalsArray, const size_t maxBordersCount, const INavMeshQueryFilter& filter)
		: pNavMesh(pNavMesh)
		, pBordersWithNormalsArray(pBordersWithNormalsArray)
		, maxBordersCount(maxBordersCount)
		, filter(filter)
		, bordersCount(0)
	{}

	const size_t GetFoundBordersCount() const { return bordersCount; }

	virtual EResult operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount) override;

private:
	const CNavMesh* pNavMesh;
	const vector3_t localFromPosition;
	const size_t maxBordersCount;
	const INavMeshQueryFilter& filter;
	
	Vec3* pBordersWithNormalsArray;
	size_t bordersCount;
};

class CGetMeshBordersQueryNoFilter : public INavMeshQueryProcessing
{
public:
	CGetMeshBordersQueryNoFilter(const CNavMesh* pNavMesh, Vec3* pBordersWithNormalsArray, const size_t maxBordersCount)
		: pNavMesh(pNavMesh)
		, pBordersWithNormalsArray(pBordersWithNormalsArray)
		, maxBordersCount(maxBordersCount)
		, bordersCount(0)
	{}

	const size_t GetFoundBordersCount() const { return bordersCount; }

	virtual EResult operator() (const MNM::TileID tileId, const MNM::Tile::STriangle** pTriangles, const MNM::TriangleID* pTriangleIds, const size_t trianglesCount) override;

private:
	const CNavMesh* pNavMesh;
	const vector3_t localFromPosition;
	const size_t maxBordersCount;

	Vec3* pBordersWithNormalsArray;
	size_t bordersCount;
};

} // namespace MNM