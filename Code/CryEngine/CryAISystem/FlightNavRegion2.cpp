// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "FlightNavRegion2.h"
#include "AILog.h"
#include <CryPhysics/IPhysics.h>
#include <algorithm>
#include <limits>
#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderer.h>      // this is needed for debug drawing
#include <CryRenderer/IRenderAuxGeom.h> // this is needed for debug drawing
//#include <CryFile.h>
#include <CrySystem/ITimer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryMath/Cry_GeoOverlap.h>
#include "AICollision.h"
#include <CryNetwork/ISerialize.h>
#include "Navigation.h"     // NOTE Feb 22, 2008: <pvl> for SpecialArea declaration

#include "GenericAStarSolver.h"

#define BAI_FNAV2_FILE_VERSION_READ  21
#define BAI_FNAV2_FILE_VERSION_WRITE 21

//#pragma optimize("", off)
//#pragma inline_depth(0)

static const int8 offsets[8][2] = {
	{ -1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }
};

// no need to expose those in the header
enum DebugDraws
{
	OFF              = 0,
	DRAWNAVDATA      = 1,
	DRAWCONNECTTIONS = 2,
	DRAWPATH         = 3,
	DRAWPORTALS      = 4,
};

STRUCT_INFO_BEGIN(SpanDesc)
STRUCT_VAR_INFO(x, TYPE_INFO(int))
STRUCT_VAR_INFO(y, TYPE_INFO(int))
STRUCT_VAR_INFO(smin, TYPE_INFO(int))
STRUCT_VAR_INFO(smax, TYPE_INFO(int))
STRUCT_INFO_END(SpanDesc)

struct AStarNodeCompare
{
	bool operator()(const AStarNode<Vec3i>& LHS, const AStarNode<Vec3i>& RHS) const
	{
		uint32 left = LHS.graphNodeIndex.x + LHS.graphNodeIndex.y * 1000 + LHS.graphNodeIndex.z * 1000000;
		uint32 right = RHS.graphNodeIndex.x + RHS.graphNodeIndex.y * 1000 + RHS.graphNodeIndex.z * 1000000;

		return left < right;
	}
};

class ClosedList
{
	std::set<Vec3i, AStarNodeCompare> closedList;
public:

	void Resize(Vec3i size)
	{
		closedList.clear();
	}

	void Close(Vec3i index)
	{
		closedList.insert(index);
	}

	bool IsClosed(Vec3i index) const
	{
		return closedList.find(index) != closedList.end();
	}

	//For users debug benefit they may template specialize this function:
	void Print() const;
};

class NodeContainer
{
	std::set<AStarNode<Vec3i>, AStarNodeCompare> aStarNodes;

public:

	AStarNode<Vec3i>* GetAStarNode(Vec3i nodeIndex)
	{
		std::pair<std::set<AStarNode<Vec3i>, AStarNodeCompare>::iterator, bool> result =
		  aStarNodes.insert(AStarNode<Vec3i>(nodeIndex));

		return const_cast<AStarNode<Vec3i>*>(&*result.first);
	}

	void Clear()
	{
		aStarNodes.clear();
	}
};

GenericAStarSolver<CFlightNavRegion2::NavData, CFlightNavRegion2::NavData, CFlightNavRegion2::NavData, DefaultOpenList<Vec3i>, ClosedList, NodeContainer> astar;

bool CFlightNavRegion2::NavData::Span::Overlaps(const Span* other, float amount) const
{
	bool ret = false;

	if (other)
	{
		float testMinHeight = other->heightMin;
		float testMaxHeight = other->heightMax;

		ret = !(heightMax > testMaxHeight) && !(heightMin < testMinHeight);
		ret |= (heightMin > testMinHeight) && (testMaxHeight - heightMin) > amount;
		ret |= (testMinHeight > heightMin) && (heightMax - testMinHeight) > amount;

	}

	return ret;
}

bool CFlightNavRegion2::NavData::Span::Overlaps(const Vec3& pos) const
{
	bool ret = false;

	if (!(heightMin > pos.z) && !(heightMax < pos.z))
	{
		ret = true;
	}

	return ret;
}

bool CFlightNavRegion2::NavData::Span::GetPortal(const Span* other, float& top, float& bottom, float amount) const
{
	bool ret = false;

	if (other)
	{
		top = static_cast<float>(__fsel(other->heightMax - heightMax, heightMax - amount, other->heightMax - amount));
		bottom = static_cast<float>(__fsel(other->heightMin - heightMin, other->heightMin + amount, heightMin + amount));

		ret = true;
	}

	return ret;
}

CFlightNavRegion2::NavData::NavData(uint32 _width, uint32 _height, uint32 _spanCount)
	: width(_width)
	, height(_height)
	, maxSpans(_spanCount + 1)
	, spanCount(1)
	, horVoxelSize(0.0f)
	, agentHeight(0.0f)
	, basePos(ZERO)
{
	spans = new Span[_spanCount + 1];

	grid = new uint32[width * height];
	memset(grid, 0, width * height * sizeof(grid[0]));
}

CFlightNavRegion2::NavData::~NavData()
{
	delete[] spans;
	delete[] grid;
}

void CFlightNavRegion2::NavData::AddSpan(int x, int y, float minHeight, float maxHeight, uint32 flags)
{
	if (spanCount < maxSpans)
	{
#ifdef _DEBUG
		gEnv->pLog->Log("Adding span x=%d, y=%d, min=%f, max=%f (spancount = %d)", x, y, minHeight, maxHeight, spanCount - 1);
#endif

		spans[spanCount].heightMin = minHeight;
		spans[spanCount].heightMax = maxHeight;

		if (IsColumnSet(x, y))
		{
			Span& span = GetColumn(x, y);

			while (span.next)
			{
				span = *span.next;
			}

			span.next = &spans[spanCount];
		}
		else
		{
			GetGridCell(x, y) = spanCount;
		}

		++spanCount;
	}
}

const CFlightNavRegion2::NavData::Span& CFlightNavRegion2::NavData::GetNeighbourColumn(uint x, uint y, Span::Neighbour neigh) const
{
	int xOff = offsets[neigh][0];
	int yOff = offsets[neigh][1];

	uint32 xr = x + xOff;
	uint32 yr = y + yOff;

	if (xr < width && yr < height)
	{
		return GetColumn(xr, yr);
	}

	return spans[0];
}

bool CFlightNavRegion2::NavData::GetEnclosing(const Vec3& pos, Vec3i& spanPos, Vec3* outPos) const
{
	bool ret = false;

	int32 offX = 0;
	int32 offY = 0;
	int32 offZ = 0;

	Vec3 diff = pos - basePos;
	//float horVoxelSize = horVoxelSize;

	offX = static_cast<int>(diff.x / horVoxelSize);
	offY = static_cast<int>(diff.y / horVoxelSize);

	if (!(offX < 0 || (uint32)offX > width || offY < 0 || (uint32)offY > height))
	{

		const NavData::Span* span = &GetColumn(offX, offY);
		if (IsSpanValid(span))
		{
			offZ = 0;
			while (span)
			{
				if (span->Overlaps(diff))
				{
					spanPos.x = offX;
					spanPos.y = offY;
					spanPos.z = offZ;

					ret = true;
					break;
				}

				++offZ;
				span = span->next;
			}
		}
	}

	return ret;
}

Vec3i CFlightNavRegion2::NavData::GetNumNodes() const
{
	return Vec3i(width, height, 50);
}

Vec3i CFlightNavRegion2::NavData::GetBestNodeIndex(Vec3& referencePoint) const
{
	Vec3i ret(-1, -1, -1);

	GetEnclosing(referencePoint, ret);

	if (-1 == ret.x || -1 == ret.y || -1 == ret.z)
	{
		Vec3 center;
		GetClosestSpan(referencePoint, ret, center);
	}

	return ret;
}

Vec3 CFlightNavRegion2::NavData::GetNodeReferencePoint(Vec3i nodeIndex) const
{
	Vec3 pos;
	const Span* span = GetSpan(nodeIndex.x, nodeIndex.y, nodeIndex.z);

	if (span)
	{
		pos.x = (static_cast<float>(nodeIndex.x) + 0.5f) * horVoxelSize;
		pos.y = (static_cast<float>(nodeIndex.y) + 0.5f) * horVoxelSize;
		pos.z = (span->heightMin + span->heightMax) * 0.5f;
	}

	return pos;
}

int CFlightNavRegion2::NavData::GetNumLinks(Vec3i nodeIndex) const
{
	int ret = 0;

	const Span* span = GetSpan(nodeIndex.x, nodeIndex.y, nodeIndex.z);

	if (span)
	{
		for (uint32 loop = 0; loop < 8; ++loop)
		{
			if (span->neighbours[loop][0])
			{
				ret += span->neighbours[loop][1] - span->neighbours[loop][0] + 1;
			}
		}
	}

	return ret;
}

Vec3i CFlightNavRegion2::NavData::GetNextNodeIndex(Vec3i graphNodeIndex, int linkIndex) const
{
	Vec3i ret(-1, -1, -1);

	const Span* span = GetSpan(graphNodeIndex.x, graphNodeIndex.y, graphNodeIndex.z);

	if (span)
	{
		for (uint32 loop = 0; loop < 8; ++loop)
		{
			if (span->neighbours[loop][0])
			{
				uint32 diff = span->neighbours[loop][1] - span->neighbours[loop][0] + 1;
				if ((uint32)linkIndex < diff)
				{
					uint32 z = span->neighbours[loop][0] + linkIndex - 1;
					return Vec3i(graphNodeIndex.x + offsets[loop][0], graphNodeIndex.y + offsets[loop][1], z);
				}
				else
				{
					linkIndex -= diff;
				}
			}
		}
	}

	return ret;
}

//heuristics and cost function
float CFlightNavRegion2::NavData::H(const NavData* graph, Vec3i currentNodeIndex, Vec3i endNodeIndex) const
{
	float ret = FLT_MAX;

	if (graph)
	{
		const Span* cur = graph->GetSpan(currentNodeIndex.x, currentNodeIndex.y, currentNodeIndex.z);
		const Span* end = graph->GetSpan(endNodeIndex.x, endNodeIndex.y, endNodeIndex.z);

		if ((cur != NULL) && (end != NULL))
		{
			Vec3i diff = endNodeIndex - currentNodeIndex;

			float curHeight = cur->heightMin; //(cur->heightMax + cur->heightMin) * 0.5f;
			float endHeight = end->heightMin;//(end->heightMax + end->heightMin) * 0.5f;
			endHeight -= curHeight;

			ret = diff.x * diff.x * horVoxelSize * horVoxelSize + diff.y * diff.y * horVoxelSize * horVoxelSize + endHeight * endHeight;
		}
	}

	return ret;
}

//link traversalcost
float CFlightNavRegion2::NavData::GetCost(const NavData* graph, Vec3i currentNodeIndex, Vec3i nextNodeIndex) const
{
	float ret = FLT_MAX;

	if (graph)
	{
		const Span* cur = graph->GetSpan(currentNodeIndex.x, currentNodeIndex.y, currentNodeIndex.z);
		const Span* end = graph->GetSpan(nextNodeIndex.x, nextNodeIndex.y, nextNodeIndex.z);

		if ((cur != NULL) && (end != NULL))
		{
			//Vec3i diff = nextNodeIndex - currentNodeIndex;
			//ret = diff.x * diff.x * horVoxelSize  * horVoxelSize + diff.y * diff.y * horVoxelSize  * horVoxelSize;

			Vec3i diff = nextNodeIndex - currentNodeIndex;

			float curHeight = cur->heightMin; //(cur->heightMax + cur->heightMin) * 0.5f;
			float endHeight = end->heightMin;//(end->heightMax + end->heightMin) * 0.5f;;
			endHeight -= curHeight;

			ret = diff.x * diff.x * horVoxelSize * horVoxelSize + diff.y * diff.y * horVoxelSize * horVoxelSize + endHeight * endHeight;

		}
	}

	return ret;
}

bool CFlightNavRegion2::NavData::CreateEightConnected()
{
	uint32 sizeX = width;
	uint32 sizeY = height;

	for (uint32 loopX = 0; loopX < sizeX; ++loopX)
	{
		for (uint32 loopY = 0; loopY < sizeY; ++loopY)
		{
			if (IsColumnSet(loopX, loopY))
			{
				NavData::Span* column = &GetColumn(loopX, loopY);

				while (column)
				{
					for (uint8 innerloop = 0; innerloop < 8; innerloop += 2)
					{
						ConnectDirection(column, loopX, loopY, static_cast<Span::Neighbour>(innerloop));

						//const NavData::Span *neigh = &GetNeighbourColumn(loopX, loopY, static_cast<Span::Neighbour>(innerloop));

						//if (IsSpanValid(neigh))
						//{
						//	if (spans->neighbours[innerloop][0] == 0)
						//	{
						//		uint32 z = 1;

						//		while (neigh && !spans->Overlaps(neigh, 8.0f) )
						//		{
						//			++z;
						//			neigh = neigh->next;
						//		}

						//		//lower overlap bound
						//		if (neigh && spans->Overlaps(neigh, 8.0f))
						//		{
						//			spans->neighbours[innerloop][0] = z;
						//			neigh = neigh->next;
						//		}
						//		else
						//		{
						//			continue;
						//		}

						//		while (neigh && spans->Overlaps(neigh, 8.0f) )
						//		{
						//			++z;
						//			neigh = neigh->next;
						//		}

						//		//upper overlap bound
						//		spans->neighbours[innerloop][1] = z;
						//	}
						//}
					}

					for (uint8 innerloop = 1; innerloop < 8; innerloop += 2)
					{
						if (column->neighbours[innerloop - 1][0] && column->neighbours[(innerloop + 1) % 8][0])
						{
							ConnectDirection(column, loopX, loopY, static_cast<Span::Neighbour>(innerloop));
						}
					}

					column = column->next;
				}
			}
		}
	}

	return true;
}

void CFlightNavRegion2::NavData::ConnectDirection(NavData::Span* startSpan, uint32 x, uint32 y, Span::Neighbour dir)
{
	uint32 sizeX = width;
	uint32 sizeY = height;

	if (startSpan)
	{
		const NavData::Span* neigh = &GetNeighbourColumn(x, y, static_cast<NavData::Span::Neighbour>(dir));

		if (IsSpanValid(neigh))
		{
			if (startSpan->neighbours[dir][0] == 0)
			{
				uint32 z = 1;

				while (neigh && !startSpan->Overlaps(neigh, 8.0f))
				{
					++z;
					neigh = neigh->next;
				}

				//lower overlap bound
				if (neigh && startSpan->Overlaps(neigh, 8.0f))
				{
					startSpan->neighbours[dir][0] = z;
					neigh = neigh->next;
				}
				else
				{
					return;
				}

				while (neigh && startSpan->Overlaps(neigh, 8.0f))
				{
					++z;
					neigh = neigh->next;
				}

				//upper overlap bound
				startSpan->neighbours[dir][1] = z;
			}
		}
	}
}

// ---------------------------------------------------
// This method expects the path to be in reverse order
// ---------------------------------------------------
void CFlightNavRegion2::NavData::BeautifyPath(const Vec3& _startPos, const Vec3& _endPos, const std::vector<Vec3i>& pathRaw, std::vector<Vec3>& pathOut) const
{
	FRAME_PROFILER("CFlightNavRegion2::NavData::BeautifyPath", gEnv->pSystem, PROFILE_AI);
	float oldTop = -FLT_MAX;
	float oldMin = FLT_MAX;
	Vec3 portalPos(_startPos);
	Vec3 lastDir(ZERO);

	Vec3 endTemp = _endPos;
	Vec3 diff = _startPos - _endPos;
	diff.Normalize();

	//pathOut.push_back(endTemp);

	if (!pathRaw.empty())
	{
		std::vector<Vec3i>::const_iterator it = pathRaw.begin();

		const Span* span = GetSpan(it->x, it->y, it->z);

		if (span)
		{
			Vec3 temp;
			GetHorCenter(it->x, it->y, temp);
			temp.z = _endPos.z;

			float tempMin = span->heightMin + basePos.z;
			float tempMax = span->heightMax + basePos.z;

			temp.z = static_cast<float>(__fsel(_endPos.z - tempMin, temp.z, tempMin));
			temp.z = static_cast<float>(__fsel(temp.z - tempMax, tempMax, temp.z));

			pathOut.push_back(temp);
		}

		for (; pathRaw.end() != it; ++it)
		{
			std::vector<Vec3i>::const_iterator itP1 = it;
			++itP1;

			if (itP1 != pathRaw.end())
			{
				const Vec3i& coordsStart = *it;
				const Vec3i& coordsEnd = *itP1;

				Vec3 centerStart;
				GetHorCenter(coordsStart.x, coordsStart.y, centerStart);

				Vec3 centerEnd;
				GetHorCenter(coordsEnd.x, coordsEnd.y, centerEnd);

				Vec3 dir = centerEnd - centerStart;
				dir.z = 0.0f;
				dir.Normalize();

				Vec3 newP = (centerStart + centerEnd) * 0.5f;
				newP.z += basePos.z;
				float dist = (newP - endTemp).GetLength2D();

				const Span* spanStart = GetSpan(coordsStart.x, coordsStart.y, coordsStart.z);
				const Span* spanEnd = GetSpan(coordsEnd.x, coordsEnd.y, coordsEnd.z);

				if (IsSpanValid(spanStart) && IsSpanValid(spanEnd))
				{
					float top, bottom;
					if (spanStart->GetPortal(spanEnd, top, bottom, 0.0f))
					{
						float dirChange = static_cast<float>(__fsel(lastDir.x * lastDir.x + lastDir.y * lastDir.y + lastDir.z * lastDir.z - 0.5f,
						                                            dir.x * lastDir.x + dir.y * lastDir.y + dir.z * lastDir.z, 1.0f));

						float tempZ = endTemp.z + dist * diff.z;
						float aboveBelow = static_cast<float>(__fsel(top + basePos.z - tempZ, 0.0f, 1.0f));
						aboveBelow += static_cast<float>(__fsel(tempZ - bottom - basePos.z, 0.0f, 1.0f));

						newP.z = static_cast<float>(__fsel(top + basePos.z - tempZ, tempZ, top + basePos.z));
						newP.z = static_cast<float>(__fsel(tempZ - bottom - basePos.z, tempZ, bottom + basePos.z + 1.0f));

						if (dirChange < 0.9f || aboveBelow > 0.5f)
						{
							if (dirChange < 0.9f)
							{
								centerStart.z = newP.z;
								AddPathPoint(pathOut, centerStart);
							}

							AddPathPoint(pathOut, newP);

							endTemp = newP;

							diff = _startPos - endTemp;
							diff.Normalize();
						}

						lastDir = dir;
					}
				}
			}
		}

		AddPathPoint(pathOut, _startPos);
	}
}

void CFlightNavRegion2::NavData::AddPathPoint(std::vector<Vec3>& pathOut, const Vec3& newPoint) const
{
	if (pathOut.size() > 1)
	{
		Vec3& last = pathOut.back();
		const Vec3& penUltimate = *(&last - 1);

		Vec3 newSeg = newPoint - last;
		Vec3 oldSeg = last - penUltimate;

		newSeg.z = 0.0f;
		oldSeg.z = 0.0f;

		float newSegLen = newSeg.len();
		float oldSegLen = oldSeg.len();

		newSeg /= newSegLen;
		oldSeg /= oldSegLen;

		if (oldSeg.dot(newSeg) < 0.5f)
		{
			Vec3 tempDiff = (newSeg - oldSeg);
			tempDiff.Normalize();

			Vec3 buffer = last;
			last -= oldSeg * (oldSegLen * 0.5f);

			//buffer += tempDiff * 15.0f;
			pathOut.push_back(buffer + tempDiff * 5.0f);

			pathOut.push_back(buffer + newSeg * (newSegLen * 0.5f));
		}
	}

	pathOut.push_back(newPoint);
}

void CFlightNavRegion2::NavData::GetRandomSpan(Vec3i& spanCoord, Vec3& center) const
{
	uint32 gridCell = 0;

	while (!gridCell)
	{
		uint32 testX = cry_random(0U, width - 1);
		uint32 testY = cry_random(0U, height - 1);

		gridCell = GetGridCell(testX, testY);

		if (gridCell > 0)
		{
			const Span* span = &spans[gridCell];

			spanCoord.z = 0;

			if (span->next && cry_random(0, 99) > 49)
			{
				span = span->next;
				spanCoord.z = 1;
			}

			spanCoord.x = testX;
			spanCoord.y = testY;

			GetHorCenter(testX, testY, center);

			float minH = span->heightMin;
			float maxH = span->heightMax;
			float diff = maxH - minH;

			float h = minH + diff * clamp_tpl(cry_random(0.0f, 1.0f), 0.2f, 0.8f);

			center.z = basePos.z + h;
		}
	}
}

void CFlightNavRegion2::NavData::GetClosestSpan(const Vec3& pos, Vec3i& spanCoord, Vec3& center) const
{
	GetGridOffset(pos, spanCoord);

	Limit(spanCoord.x, 0, (int)width);
	Limit(spanCoord.y, 0, (int)height);

	uint32 offsetX = spanCoord.x;
	uint32 offsetY = spanCoord.y;

	uint32 mindist = 10000000;

	uint32 gridCell = 0;

	for (int32 x = 0; x < (int32)width; ++x)
	{
		for (int32 y = 0; y < (int32)height; ++y)
		{
			if (GetGridCell(x, y))
			{
				uint32 dist = (x - spanCoord.x) * (x - spanCoord.x) + (y - spanCoord.y) * (y - spanCoord.y);

				if (dist < mindist)
				{
					mindist = dist;
					offsetX = x;
					offsetY = y;
				}
			}
		}
	}

	if (gridCell = GetGridCell(offsetX, offsetY))
	{
		const Span* span = GetSpan(offsetX, offsetY, 0);

		int32 offsetZ = 0;

		spanCoord.x = offsetX;
		spanCoord.y = offsetY;
		spanCoord.z = 0;

		while (span)
		{
			float z = pos.z - basePos.z;

			if (!(span->heightMin > z) && !(span->heightMax < z))
			{
				spanCoord.z = offsetZ;
			}

			span = span->next;
		}

		span = GetSpan(spanCoord.x, spanCoord.y, spanCoord.z);
		GetHorCenter(offsetX, offsetY, center);

		center.z = static_cast<float>(__fsel(span->heightMin + basePos.z - pos.z, span->heightMin + basePos.z, pos.z));
		center.z = static_cast<float>(__fsel(span->heightMax + basePos.z - center.z, center.z, span->heightMax + basePos.z));
	}
}

void CFlightNavRegion2::NavData::GetSpansAlongLine(const Vec3& startPos, const Vec3& endPos, std::vector<Vec3i>& spanCoords) const
{
	Vec3i spanCoordStart;
	GetGridOffset(startPos, spanCoordStart);

	Vec3i spanCoordEnd;
	GetGridOffset(endPos, spanCoordEnd);

	Limit(spanCoordStart.x, 0, (int)width);
	Limit(spanCoordStart.y, 0, (int)height);
	Limit(spanCoordEnd.x, 0, (int)width);
	Limit(spanCoordEnd.y, 0, (int)height);

	uint32 offsetX = spanCoordStart.x;
	uint32 offsetY = spanCoordStart.y;
	uint32 mindist = 10000000;
	uint32 gridCell = 0;

	// now breesenham along the direction
	int dx = spanCoordEnd.x - spanCoordStart.x;
	int dy = spanCoordEnd.y - spanCoordStart.y;

	int incx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
	int incy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

	if (0 > dx)
	{
		dx = -dx;
	}

	if (0 > dy)
	{
		dy = -dy;
	}

	int pdx, pdy, ddx, ddy, es, el;
	if (dx > dy)
	{
		pdx = incx;
		pdy = 0;
		ddx = incx;
		ddy = incy;
		es = dy;
		el = dx;
	}
	else
	{
		pdx = 0;
		pdy = incy;
		ddx = incx;
		ddy = incy;
		es = dx;
		el = dy;
	}

	int x = spanCoordStart.x;
	int y = spanCoordStart.y;
	int err = el / 2;

	if (GetGridCell(x, y))
	{
		int doit1 = 1;
		Vec3i temp(x, y, 0);
		spanCoords.push_back(temp);
	}

	for (int t = 0; t < el; ++t)
	{
		err -= es;
		if (0 > err)
		{
			err += el;

			x += ddx;
			y += ddy;
		}
		else
		{
			x += pdx;
			y += pdy;
		}

		if (GetGridCell(x, y))
		{
			int doit = 0;
			Vec3i temp(x, y, 0);
			spanCoords.push_back(temp);
		}
	}
}

int CFlightNavRegion2::m_DebugDraw;

void CFlightNavRegion2::InitCVars()
{
	REGISTER_CVAR2("ai_DebugDrawFlight2", &m_DebugDraw, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Set AI features to behave in earlier milestones - please use sparingly");
}

CFlightNavRegion2::CFlightNavRegion2(IPhysicalWorld* pPhysWorld, CGraph* pGraph)
	: m_pPhysWorld(pPhysWorld)
	, m_pGraph(pGraph)
{
}

CFlightNavRegion2::~CFlightNavRegion2()
{
	Clear();
}

void CFlightNavRegion2::Clear()
{
	for (std::vector<NavData*>::iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
	{
		if (*it)
		{
			delete *it;
		}
	}

	m_FlightNavData.clear();
}

// temp, will be removed soon
const float with = 2.0f;
const float factor = 5.0f;

//void CFlightNavRegion2::Process(I3DEngine* p3DEngine, const std::list<SpecialArea*>& flightAreas)
//{
//}

bool CFlightNavRegion2::ReadFromFile(const char* pName)
{
	Clear();

	CCryFile file;
	if (!file.Open(pName, "rb"))
	{
		AIWarning("could not load AI flight nav. [%s]", pName);
		return false;
	}

	int nFileVersion;
	file.ReadType(&nFileVersion, 1);

	if (BAI_FNAV2_FILE_VERSION_WRITE != nFileVersion)
	{
		AIWarning("unsupported file version for Flight V2.0. [%s]", pName);
		return false;
	}

	uint32 countModifier;
	file.ReadType(&countModifier, 1);

	uint32 width, height;

	for (uint32 loop = 0; loop < countModifier; ++loop)
	{
		file.ReadType(&width, 1);
		file.ReadType(&height, 1);

		Vec3 basePos;
		file.ReadType(&basePos, 1);
		float horVoxelSize, agentHeight;
		file.ReadType(&horVoxelSize, 1);
		file.ReadType(&agentHeight, 1);

		uint32 spanCount;
		file.ReadType(&spanCount, 1);

		if (0 < spanCount)
		{
			std::vector<SpanDesc> readBuffer;
			readBuffer.resize(spanCount);

			file.ReadType(&readBuffer[0], spanCount);

			m_FlightNavData.push_back(new NavData(width, height, spanCount));
			NavData& data = *m_FlightNavData.back();
			//data.spans = new LayeredNavMesh::SpanBuffer(0, 0);

			data.horVoxelSize = horVoxelSize;
			data.agentHeight = agentHeight;
			data.basePos = basePos;

			for (uint32 innerLoop = 0; innerLoop < spanCount; ++innerLoop)
			{
				SpanDesc& desc = readBuffer[innerLoop];
				//data.spans->AddSpan(desc.x, desc.y, desc/*.span*/.smin, desc/*.span*/.smax, 0);
				data.AddSpan(desc.x, desc.y, static_cast<float>(desc.smin), static_cast<float>(desc.smax));
			}

			data.CreateEightConnected();
		}
	}

	return true; //CreateEightConnected();
}

unsigned CFlightNavRegion2::GetEnclosing(int x, int y, const Vec3& pos, Vec3* closestValid, Vec3* spanPos)
{
	// that's easy :-) and not strictly needed here

	return 0;
}

//Todo: make it really handle multiple regions
const CFlightNavRegion2::NavData* CFlightNavRegion2::GetEnclosing(const Vec3& pos, Vec3i& spanPos, Vec3* outPos) const
{
	/*bool ret = false;*/

	int32 offX = 0;
	int32 offY = 0;
	int32 offZ = 0;

	for (std::vector<NavData*>::const_iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
	{
		Vec3 diff = pos - (*it)->basePos;
		float horVoxelSize = (*it)->horVoxelSize;

		offX = static_cast<int>(diff.x / horVoxelSize);
		offY = static_cast<int>(diff.y / horVoxelSize);

		if (offX < 0 || (uint32)offX > (*it)->width || offY < 0 || (uint32)offY > (*it)->height)
		{
			continue;
		}

		const NavData::Span* span = &(*it)->GetColumn(offX, offY);
		if ((*it)->IsSpanValid(span))
		{
			offZ = 0;
			while (span)
			{
				if (span->Overlaps(diff))
				{
					spanPos.x = offX;
					spanPos.y = offY;
					spanPos.z = offZ;

					//ret = true;

					return *(it);
					//break;
				}

				++offZ;
				span = span->next;
			}

			break;
		}
	}

	return 0;
}

void CFlightNavRegion2::DrawPath(const std::vector<Vec3>& path)
{
	if (path.size() > 1)
	{
		std::vector<Vec3>::const_iterator segmentStart = path.begin();
		std::vector<Vec3>::const_iterator segmentEnd = path.begin() + 1;
		ColorB c(255, 0, 0);

		SDrawTextInfo ti;
		ti.color[0] = ti.color[1] = ti.color[2] = ti.color[3] = 1.0f;
		ti.flags = eDrawText_Center;
		ti.scale = Vec2(8.0f);

		uint32 num = path.size();
		char buffer[16];
		cry_sprintf(buffer, "%d", --num);
		IRenderAuxText::DrawLabel(*segmentStart, 2.0f, buffer);

		for (; path.end() != segmentEnd; ++segmentEnd, ++segmentStart)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(*segmentStart, c, *segmentEnd, c, 20.0f);

			cry_sprintf(buffer, "%d", --num);
			IRenderAuxText::DrawLabel(*segmentEnd, 2.0f, buffer);
		}
	}
}

void CFlightNavRegion2::DebugDraw() const
{
	ColorB col(255, 255, 255, 128);
	ColorB colw(255, 255, 255, 255);
	ColorB colg(0, 255, 0, 255);
	ColorB colSpan(0, 0, 255, 255);
	ColorB colSpanStart(0, 255, 0, 255);
	ColorB colSpanEnd(255, 0, 0, 255);

	IEntity* debugEntStart = gEnv->pEntitySystem->FindEntityByName("DebugFlightStart");
	IEntity* debugEntEnd = gEnv->pEntitySystem->FindEntityByName("DebugFlightEnd");

	Vec3i spanStart, spanEnd;
	bool drawStart = false;
	bool drawEnd = false;

	if (debugEntStart)
	{
		drawStart = GetEnclosing(debugEntStart->GetPos(), spanStart) != 0;
	}

	if (debugEntEnd)
	{
		drawEnd = GetEnclosing(debugEntEnd->GetPos(), spanEnd) != 0;
	}

	static std::vector<Vec3i> path;
	static std::vector<Vec3> pathSmooth;
	if (drawStart && drawEnd && path.empty())
	{
		path.clear();
		pathSmooth.clear();

		astar.StartPathFind(*m_FlightNavData.begin(), *m_FlightNavData.begin(), *m_FlightNavData.begin(), debugEntStart->GetPos(), debugEntEnd->GetPos());
		astar.Update(2000);

		astar.GetPathReversed(path);

		Vec3i span;
		const NavData* graph = GetEnclosing(debugEntStart->GetPos(), span);

		if (graph)
		{
			graph->BeautifyPath(debugEntStart->GetPos(), debugEntEnd->GetPos(), path, pathSmooth);
		}
	}
	else if (!drawStart || !drawEnd)
	{
		path.clear();
	}

	switch (m_DebugDraw)
	{

	case DRAWCONNECTTIONS:
		{
			for (std::vector<NavData*>::const_iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
			{
				uint32 count = 0;
				Vec3 basePos = (*it)->basePos;

				uint32 sizeX = (*it)->width;
				uint32 sizeZ = (*it)->height;

				float horSize = (*it)->horVoxelSize;

				const NavData& data = *(*it);

				for (uint32 loopX = 0; loopX < sizeX; ++loopX)
				{
					for (uint32 loopZ = 0; loopZ < sizeZ; ++loopZ)
					{
						colSpan.a = 128;
						if (data.IsColumnSet(loopX, loopZ))
						{
							const NavData::Span* span = &data.GetColumn(loopX, loopZ);

							while (span)
							{
								Vec3 offset(loopX * horSize, loopZ * horSize, (span->heightMin + span->heightMax) * 0.5f);

								for (uint8 innerloop = 0; innerloop < 8; innerloop += 2)
								{
									if (span->neighbours[innerloop][0] != 0)
									{
										Vec3 start = basePos + offset;
										//Vec3 end = start + (offsets[innerloop][0] * Vec3(7.5f, 0.0f, 0.0f)) + (offsets[innerloop][1] * Vec3(0.0f, 7.5f, 0.0f));

										uint32 x = loopX + offsets[innerloop][0];
										uint32 y = loopZ + offsets[innerloop][1];

										Vec3 end, end2;
										data.GetHorCenter(x, y, end);
										end2 = end;

										const NavData::Span* spanFirst = data.GetSpan(x, y, span->neighbours[innerloop][0] - 1);
										const NavData::Span* spanLast = data.GetSpan(x, y, span->neighbours[innerloop][1] - 1);

										end.z = basePos.z + (spanFirst->heightMin + spanFirst->heightMax) * 0.5f;
										//										gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(start, colg, end, colw, 2.0f);

										end2.z = basePos.z + (spanLast->heightMin + spanLast->heightMax) * 0.5f;
										//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(start, colg, end, colw, 2.0f);

										gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(start, colg, end, colw, end2, colw);
									}
								}

								span = span->next;
							}
						}
					}
				}
			}
		}

	case DRAWPATH:
		{
			for (std::vector<NavData*>::const_iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
			{
				Vec3 basePos = (*it)->basePos;
				float horSize = (*it)->horVoxelSize;

				if (!path.empty())
					for (std::vector<Vec3i>::const_iterator coords = path.begin() + 1; path.end() != coords; ++coords)
					{
						const Vec3i& coord = *coords;
						const NavData::Span* span = (*it)->GetSpan(coord.x, coord.y, coord.z);

						if (span)
						{
							Vec3 offset(coord.x * horSize, coord.y * horSize, span->heightMin);
							Vec3 offset2((coord.x + 1) * horSize, (coord.y + 1) * horSize, span->heightMax);

							gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(basePos + offset, basePos + offset2), true, colw, eBBD_Faceted);

							//spanStart = span;
						}
						else
						{
							break;
						}
					}
			}

			DrawPath(pathSmooth);
		}
		break;

	case DRAWPORTALS:
		for (std::vector<NavData*>::const_iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
		{
			Vec3 basePos = (*it)->basePos;
			float horSize = (*it)->horVoxelSize;

			ColorB colwA(255, 255, 255, 128);
			ColorB colPortal(0, 0, 255, 128);

			if (!path.empty())
				for (std::vector<Vec3i>::const_iterator coords = path.begin() + 1; path.end() != coords; ++coords)
				{
					const Vec3i& coord = *coords;
					const NavData::Span* span = (*it)->GetSpan(coord.x, coord.y, coord.z);

					if (span)
					{
						Vec3 offset(coord.x * horSize, coord.y * horSize, span->heightMin);
						Vec3 offset2((coord.x + 1) * horSize, (coord.y + 1) * horSize, span->heightMax);

						gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(basePos + offset, basePos + offset2), true, colwA, eBBD_Faceted);

						Vec3 center = basePos + (offset + offset2) * 0.5f;

						SDrawTextInfo ti;
						ti.color[0] = ti.color[1] = 0.0f;
						ti.color[2] = ti.color[3] = 1.0f;
						ti.flags = eDrawText_Center;
						ti.scale = Vec2(8.0f);

						uint32 num = path.size();
						char buffer[16];
						cry_sprintf(buffer, "(%d, %d, %d)", coord.x, coord.y, coord.z);
						IRenderAuxText::DrawLabel(center, 2.0f, buffer);

						std::vector<Vec3i>::const_iterator coordsNext = coords + 1;

						if (coordsNext != path.end())
						{
							const Vec3i& coordNext = *coordsNext;
							const NavData::Span* otherSpan = (*it)->GetSpan(coordNext.x, coordNext.y, coordNext.z);

							float top, bottom;
							span->GetPortal(otherSpan, top, bottom, 0.0f);

							if (coordNext.x == coord.x || coordNext.y == coords->y)
							{
								Vec3i diff = coordNext - coord;

								diff.x = (diff.x < 0) ? 0 : diff.x;
								diff.y = (diff.y < 0) ? 0 : diff.y;

								if (coordNext.x == coord.x)
								{
									Vec3 offsetA(coord.x * horSize, (coord.y + diff.y) * horSize, bottom);
									Vec3 offsetB(coord.x * horSize, (coord.y + diff.y) * horSize, top);
									Vec3 offsetC((coord.x + 1) * horSize, (coord.y + diff.y) * horSize, bottom);
									Vec3 offsetD((coord.x + 1) * horSize, (coord.y + diff.y) * horSize, top);

									SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
									SAuxGeomRenderFlags newflags(flags);
									newflags.SetCullMode(e_CullModeNone);
									gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newflags);
									gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(offsetA + basePos, colPortal,
									                                                   offsetB + basePos, colPortal,
									                                                   offsetC + basePos, colPortal);
									gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(offsetB + basePos, colPortal,
									                                                   offsetD + basePos, colPortal,
									                                                   offsetC + basePos, colPortal);

									gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(flags);

								}
								else
								{
									SAuxGeomRenderFlags flags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
									SAuxGeomRenderFlags newflags(flags);
									newflags.SetCullMode(e_CullModeNone);
									gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newflags);

									Vec3 offsetA((coord.x + diff.x) * horSize, coord.y * horSize, bottom);
									Vec3 offsetB((coord.x + diff.x) * horSize, coord.y * horSize, top);
									Vec3 offsetC((coord.x + diff.x) * horSize, (coord.y + 1) * horSize, bottom);
									Vec3 offsetD((coord.x + diff.x) * horSize, (coord.y + 1) * horSize, top);

									gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(offsetA + basePos, colPortal,
									                                                   offsetB + basePos, colPortal,
									                                                   offsetC + basePos, colPortal);

									gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(offsetB + basePos, colPortal,
									                                                   offsetD + basePos, colPortal,
									                                                   offsetC + basePos, colPortal);
									gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(flags);
								}
							}
							else
							{
								Vec3i diff = coordNext - coord;
								Vec3 offset3((coord.x + 0.5f + 0.5f * diff.x) * horSize, (coord.y + 0.5f + 0.5f * diff.y) * horSize, span->heightMax);

								Vec3 start = offset3 + basePos;
								Vec3 end = offset3 + basePos;

								start.z = basePos.z + bottom;
								end.z = basePos.z + top;
								gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(start, colPortal, end, colPortal, 20.0f);
							}
						}
					}
					else
					{
						break;
					}
				}

			DrawPath(pathSmooth);
		}

		break;

	case DRAWNAVDATA:
		{
			for (std::vector<NavData*>::const_iterator it = m_FlightNavData.begin(); m_FlightNavData.end() != it; ++it)
			{
				uint32 count = 0;
				Vec3 basePos = (*it)->basePos;

				uint32 sizeX = (*it)->width;
				uint32 sizeZ = (*it)->height;

				float horSize = (*it)->horVoxelSize;

				const NavData& data = *(*it);

				for (uint32 loopX = 0; loopX < sizeX; ++loopX)
				{
					for (uint32 loopZ = 0; loopZ < sizeZ; ++loopZ)
					{
						if (data.IsColumnSet(loopX, loopZ))
						{
							const NavData::Span* span = &data.GetColumn(loopX, loopZ);

							uint32 zOffset = 0;
							while (span)
							{
								++count;
								Vec3 offset(loopX * horSize, loopZ * horSize, span->heightMin);
								Vec3 offset2((loopX + 1) * horSize, (loopZ + 1) * horSize, span->heightMax);

								bool startSpan = drawStart && (loopX == spanStart.x) && (loopZ == spanStart.y) && (zOffset == spanStart.z);
								bool endSpan = drawEnd && (loopX == spanEnd.x) && (loopZ == spanEnd.y) && (zOffset == spanEnd.z);

								ColorB colour = colSpan;

								if (startSpan)
								{
									colour = colSpanStart;
								}
								else if (endSpan)
								{
									colour = colSpanEnd;
								}
								else if (drawStart || drawEnd)
								{
									colour.a = 128;
								}

								gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(basePos + offset, basePos + offset2), true, colour, eBBD_Faceted);

								++zOffset;
								span = span->next;
							}
						}
					}
				}

				int m = 0;
			}

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB (boundingBox, false, colw, eBBD_Faceted);
		}
		break;

	}
}

//====================================================================
// Serialize
//====================================================================
void CFlightNavRegion2::Serialize(TSerialize ser)
{
	ser.BeginGroup("FlightNavRegion");

	ser.EndGroup();
}
