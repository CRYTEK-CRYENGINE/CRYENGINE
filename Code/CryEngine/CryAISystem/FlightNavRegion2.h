// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   FlightNacRegion.h
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - 15:4:2005   : Created by Mikko Mononen

 *********************************************************************/

#if !defined(__FLIGHTNAV2_H__)
#define __FLIGHTNAV2_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAISystem.h>
#include "NavRegion.h"
#include "AutoTypeStructs.h"
#include <vector>
#include <list>
#include <map>

#include <CryCore/TypeInfo_impl.h>

// Enable this to visualise the path radius calculations.
// This will add some extra data to the structures, so it is required to recreate the navigation.
// The extra information will not be saved to the file.
//#define		FLIGHTNAV_DEBUG_SPHERES	1

struct IRenderer;
struct I3DEngine;
struct IPhysicalWorld;
struct IGeometry;
struct SpecialArea;
class CGraph;

//FlightNav2 uses layered nav mesh containers for convenience reasons
namespace LayeredNavMesh
{
class SpanBuffer;
};

struct SpanDesc
{
	int x, y, smin, smax;

	//LayeredNavMesh::SpanBuffer::Span span;

	// cppcheck-suppress uninitMemberVar
	SpanDesc() : x(-1), y(-1)
	{}

	AUTO_STRUCT_INFO;
};

class CFlightNavRegion2 : public CNavRegion
{

public:

	struct NavData
	{
		uint32 width;
		uint32 height;

		struct Span
		{
			enum Neighbour
			{
				LEFT         = 0,
				LEFT_TOP     = 1,
				TOP          = 2,
				RIGHT_TOP    = 3,
				RIGHT        = 4,
				RIGHT_BOTTOM = 5,
				BOTTOM       = 6,
				LEFT_BOTTOM  = 7,
			};

			float  heightMin;
			float  heightMax;
			Span*  next;
			uint32 flags;

			uint16 neighbours[8][2];

			Span() : heightMin(0.0f), heightMax(0.0f), next(0), flags(0)
			{
				memset(neighbours, 0, 16 * sizeof(neighbours[0][0]));
			}

			bool Overlaps(const Vec3& pos) const;
			bool Overlaps(const Span* other, float amount) const;
			bool GetPortal(const Span* other, float& top, float& bottom, float amount) const;
		};

		Span*   spans;
		uint32* grid;
		uint32  maxSpans;
		uint32  spanCount;
		float   horVoxelSize;
		float   agentHeight;
		Vec3    basePos;

		NavData() : width(0), height(0), spans(0), grid(0), maxSpans(0), spanCount(0), horVoxelSize(0.0f), agentHeight(0.0f), basePos(ZERO)
		{}

		NavData(uint32 _width, uint32 _height, uint32 _spanCount);

		~NavData();

		Span& GetColumn(uint32 x, uint32 y)
		{
			Span& ret = (x < width && y < height) ? spans[GetGridCell(x, y)] : spans[0];

			return ret;
		}

		const Span& GetColumn(uint32 x, uint32 y) const
		{
			Span& ret = (x < width && y < height) ? spans[GetGridCell(x, y)] : spans[0];

			return ret;
		}

		const Span& GetNeighbourColumn(uint x, uint y, Span::Neighbour neihg) const;

		bool        IsSpanValid(const Span& span) const
		{
			return (spanCount > 0) && (&span != spans);
		}

		bool IsSpanValid(const Span* span) const
		{
			return ((spanCount > 0) && span && (span != spans));
		}

		bool IsColumnSet(uint32 x, uint32 y) const
		{
			return GetGridCell(x, y) != 0;
		}

		uint32& GetGridCell(uint32 x, uint32 y)
		{
			return grid[x + y * width];
		}

		const uint32& GetGridCell(uint32 x, uint32 y) const
		{
			return grid[x + y * width];
		}

		const Span* GetSpan(uint32 x, uint32 y, uint32 z) const
		{
			return &spans[grid[x + y * width] + z];
		}

		void GetHorCenter(uint32 x, uint32 y, Vec3& center) const
		{
			center = basePos;
			center.x += x * horVoxelSize + horVoxelSize * 0.5f;
			center.y += y * horVoxelSize + horVoxelSize * 0.5f;
		}

		void GetGridOffset(const Vec3& pos, Vec3i& spanPos) const
		{
			spanPos.x = static_cast<int>((pos.x - basePos.x) / horVoxelSize);
			spanPos.y = static_cast<int>((pos.y - basePos.y) / horVoxelSize);
			spanPos.z = -1;
		}

		void AddSpan(int x, int y, float minHeight, float maxHeight, uint32 flags = 0);
		bool GetEnclosing(const Vec3& pos, Vec3i& spanPos, Vec3* outPos = 0) const;

		bool CreateEightConnected();
		void ConnectDirection(NavData::Span* span, uint32 x, uint32 y, Span::Neighbour dir);

		//Graph traits
		typedef Vec3  ReferencePointType;
		typedef Vec3i IndexType;

		Vec3i GetNumNodes() const;

		Vec3i GetBestNodeIndex(Vec3& referencePoint) const;
		Vec3  GetNodeReferencePoint(Vec3i nodeIndex) const;

		int   GetNumLinks(Vec3i nodeIndex) const;
		Vec3i GetNextNodeIndex(Vec3i graphNodeIndex, int linkIndex) const;

		//heuristics and cost function
		float H(const NavData* graph, Vec3i currentNodeIndex, Vec3i endNodeIndex) const;
		//link traversalcost
		float GetCost(const NavData* graph, Vec3i currentNodeIndex, Vec3i nextNodeIndex) const;

		void  BeautifyPath(const Vec3& startPos, const Vec3& endPos, const std::vector<Vec3i>& pathRaw, std::vector<Vec3>& pathOut) const;
		void  AddPathPoint(std::vector<Vec3>& pathOut, const Vec3& newPoint) const;

		//Only for debugging purposes
		void GetRandomSpan(Vec3i& spanCoord, Vec3& center) const;
		void GetClosestSpan(const Vec3& pos, Vec3i& spanCoord, Vec3& center) const;
		void GetSpansAlongLine(const Vec3& startPos, const Vec3& endPos, std::vector<Vec3i>& spanCoords) const;
	};

private:

	bool CreateEightConnected();

public:
	static void InitCVars();

public:

	CFlightNavRegion2(IPhysicalWorld* pPhysWorld, CGraph* pGraph);
	~CFlightNavRegion2(void);

	virtual void Clear();

	// Beautifies path returned by the graph.
	virtual void BeautifyPath(
	  const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	  const Vec3& startPos, const Vec3& startDir,
	  const Vec3& endPos, const Vec3& endDir,
	  float radius,
	  const AgentMovementAbility& movementAbility,
	  const NavigationBlockers& navigationBlockers)
	{}

	virtual void UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath,
	                        const Vec3& startPos, const Vec3& startDir,
	                        const Vec3& endPos, const Vec3& endDir)
	{}

	static void DrawPath(const std::vector<Vec3>& path);

	// Returns graph node based on the specified location.
	virtual unsigned GetEnclosing(const Vec3& pos, float passRadius = 0.0f, unsigned startIndex = 0,
	                              float range = -1.0f, Vec3* closestValid = 0, bool returnSuspect = false, const char* requesterName = "", bool omitWalkabilityTest = false) { return 0; }

	// Returns graph node based on the specified location.
	unsigned       GetEnclosing(int x, int y, const Vec3& pos, Vec3* closestValid = 0, Vec3* spanPos = 0);

	const NavData* GetEnclosing(const Vec3& pos, Vec3i& span, Vec3* spanPos = 0) const;

	//// Builds the flight navigation from: terrain and static obstacles, limited by the specified special areas.
	//void	Process(I3DEngine* p3DEngine, const std::list<SpecialArea*>& flightAreas);

	// Reads the flight navigation from binary file, returns true on success.
	bool ReadFromFile(const char* pName);

	/// inherited
	virtual void Serialize(TSerialize ser);

	void         DebugDraw() const;

	/// inherited
	virtual size_t MemStats() { return 0; }

private:

	IPhysicalWorld*       m_pPhysWorld;
	CGraph*               m_pGraph;

	static int            m_DebugDraw;

	std::vector<NavData*> m_FlightNavData;
};

#endif //#if !defined(__FLIGHTNAV2_H__)
