// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   SegmentedWorld.h
//  Version:     v1.00
//  Created:     4/11/2011 by Allen Chen
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Segmented_World_h__
#define __Segmented_World_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "SegmentNode.h"
#include "ILevelSystem.h"
#include <CryMemory/PoolAllocator.h>

// Helper struct for storing a position which will be automatically
// adjusted when the world is shifted.
// The main idea for this struct to serve as an abstraction to the game
// code from the shifts triggered by when the segmented world loads/unloads
// new segments, when it comes to storing positions in world coordinates.
struct SegmentedWorldLocation : public Vec3, public ISystemEventListener
{
public:
	SegmentedWorldLocation(const Vec3& vec3) : Vec3(vec3)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this,"SegmentedWorldLocation");
	}

	~SegmentedWorldLocation()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		if (event == ESYSTEM_EVENT_SW_SHIFT_WORLD)
		{
			Vec3* offset = (Vec3*)wparam;
			*this += *offset;
		}
	}

private:
	SegmentedWorldLocation() {}
};

class CSegmentedWorld : public ISegmentsManager, public ILevelSystemListener
{
public:
	CSegmentedWorld();
	~CSegmentedWorld();

	void Update();
	void PostUpdate();
	bool IsInsideSegmentedWorld(const Vec3& pt);
	bool MoveToSegment(int x, int y);

	// Convert position from world space to screen space
	inline Vec3 WorldToScreenPosition(const Vec3& InPos)
	{
		Vec3 OutPos;
		OutPos.x = InPos.x * m_invSegmentSizeMeters;
		OutPos.y = InPos.y * m_invSegmentSizeMeters;
		OutPos.z = 0;
		return OutPos;
	}

	// Convert position from world space to grid space
	inline Vec2i WorldToGridPosition(const Vec3& InPos)
	{
		Vec3 ScreenPos = WorldToScreenPosition(InPos);
		Vec2i OutPos((int)floorf(ScreenPos.x), (int)floorf(ScreenPos.y));
		return OutPos;
	}

	// Convert position from grid space to world space with zero height
	inline Vec3 GridToWorldPosition(int x, int y) const
	{
		Vec3 OutPos((float)(x * m_segmentSizeMeters), (float)(y * m_segmentSizeMeters), 0);
		return OutPos;
	}

	inline Vec3 GridToWorldPosition(const Vec2i& InPos) const
	{
		return GridToWorldPosition(InPos.x, InPos.y);
	}

	// ISegmentsManager
	virtual bool CreateSegments(ITerrain* pTerrain)
	{
		if (!gEnv->p3DEngine->GetITerrain())
			return false;

		return true;
	}
	virtual bool DeleteSegments(ITerrain* pTerrain) { return true; }
	virtual bool FindSegment(ITerrain* pTerrain, const Vec3& pt, int& nSID);
	virtual bool FindSegmentCoordByID(int nSID, int& x, int& y);
	virtual void GetTerrainSizeInMeters(int& x, int& y);
	virtual int  GetSegmentSizeInMeters() { return m_segmentSizeMeters; }
	virtual void ForceLoadSegments(unsigned int flags)
	{
		ForceLoadSegments(m_segmentsMin, m_segmentsMax + Vec2i(1, 1), flags);
	}
	virtual Vec3 LocalToAbsolutePosition(Vec3 const& vPos, f32 fDir = 1.f) const
	{
		Vec3 vOff = GridToWorldPosition(m_accSegmentOffset);
		vOff *= fDir;
		return vPos + vOff;
	}
	virtual void WorldVecToGlobalSegVec(const Vec3& inPos, Vec3& outPos, Vec2& outAbsCoords)
	{
		outAbsCoords = WorldToGridPosition(inPos);
		outPos = inPos - GridToWorldPosition(outAbsCoords);
	}
	virtual void GlobalSegVecToLocalSegVec(const Vec3& inPos, const Vec2& inAbsCoords, Vec3& outPos)
	{
#if USE_RELATIVE_COORD
		Vec2i offset = inAbsCoords - m_neededSegmentsMin;
#else
		Vec2i offset = inAbsCoords;
#endif
		outPos = inPos + GridToWorldPosition(offset);
	}
	virtual Vec3 WorldVecToLocalSegVec(const Vec3& inPos)
	{
		return inPos - GridToWorldPosition(m_neededSegmentsMin);
	}
	// ~ISegmentsManager

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName) {};
	virtual void OnLoadingStart(ILevelInfo* pLevel);
	virtual void OnLoadingComplete(ILevelInfo* pLevel);
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error)     {};
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {};
	virtual void OnUnloadComplete(ILevelInfo* pLevel);
	// ~ILevelSystemListener

	CSegmentNode* FindSegNodeByWorldPos(const Vec3& pos);

	virtual bool  PushEntityToSegment(unsigned int id, bool bLocal = true);

protected:
	// Reset the data structures and variables
	void Reset();

	// Load the segment data in the background and returns its segment id.
	CSegmentNode* PreloadSegment(int wx, int wy);

	// Remove a segment from the world and frees all associated memory.
	void DeleteSegment(int wx, int wy);

	// Update boundary info of active segments
	void UpdateActiveSegmentBound(const Vec2i& offset);

	// Load the initial segmented world based on the specified point in world space
	bool LoadInitialWorld(const Vec3& pos);

	// Load segments data within the given boundary
	void ForceLoadSegments(const Vec2i& segmentsMin, const Vec2i& segmentsMax, unsigned int flags = ISegmentsManager::slfAll);

	// Update all the activated segments according to the focal point
	void UpdateSegment(int wx, int wy);

	// Determine whether a segment needs to be updated with the given coordinate
	bool NeedToUpdateSegment(int wx, int wy);

	// Select a segment to update with the highest priority according to the focal point
	Vec2i SelectSegmentToUpdate(const Vec2i& focalPointWC);

	//adjust Entity position with GlobalInSW.
	void AdjustGlobalObjects();

	// Helper functions
	CSegmentNode* FindSegmentByID(int nSID);
	CSegmentNode* FindSegmentByWorldCoords(int wx, int wy);
	CSegmentNode* FindSegmentByLocalCoords(int lx, int ly);

	// Draw debug info
	void DrawDebugInfo();
	void DrawSegmentGrid(const Vec2& origin, float size);
	void PrintSegmentInfo(const Vec2& origin, float size);
	void PrintMemoryInfo(const Vec2& origin, float size);

	std::vector<CSegmentNode*>         m_arrSegments;

	Vec3                               m_worldFocalPoint;
	Vec2                               m_neededSegmentsHalfSizeMeters;
	Vec2i                              m_gridFocalPointWC;
	Vec2i                              m_gridFocalPointLC;
	Vec2i                              m_neededSegmentsMin;
	Vec2i                              m_neededSegmentsMax;
	Vec2i                              m_neededSegmentsCenter;
	Vec2i                              m_segmentsMin;
	Vec2i                              m_segmentsMax;
	Vec2i                              m_accSegmentOffset;
	int                                m_segmentSizeMeters;
	float                              m_invSegmentSizeMeters;

	stl::TPoolAllocator<CSegmentNode>* m_pPoolAllocator;

	bool                               m_bInitialWorldReady;

public:
	static string     s_levelName;
	static XmlNodeRef s_levelSurfaceTypes;
	static XmlNodeRef s_levelObjects;
};

#endif // __Segmented_World_h__
