// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"

namespace Designer
{
class Model;

enum ESpotPositionState
{
	eSpotPosState_InPolygon,
	eSpotPosState_Edge,
	eSpotPosState_CenterOfEdge,
	eSpotPosState_CenterOfPolygon,
	eSpotPosState_EitherPointOfEdge,
	eSpotPosState_FirstPointOfPolygon,
	eSpotPosState_LastPointOfPolygon,
	eSpotPosState_AtFirstSpot,
	eSpotPosState_OnVirtualLine,
	eSpotPosState_OutsideDesigner,
	eSpotPosState_Invalid
};

struct Spot
{
	Spot() : m_Pos(BrushVec3(0, 0, 0))
	{
		Reset();
	}

	explicit Spot(const BrushVec3& pos) : m_Pos(pos)
	{
		Reset();
	}

	Spot(const BrushVec3& pos, ESpotPositionState posState, PolygonPtr pPolygon) : m_Pos(pos), m_PosState(posState), m_pPolygon(pPolygon)
	{
		if (m_pPolygon)
			m_Plane = m_pPolygon->GetPlane();
		else
			m_Plane = BrushPlane(BrushVec3(0, 0, 0), 0);
		m_bProcessed = false;
	}

	Spot(const BrushVec3& pos, ESpotPositionState posState, const BrushPlane& plane) : m_Pos(pos), m_PosState(posState), m_Plane(plane)
	{
		m_pPolygon = NULL;
		m_bProcessed = false;
	}

	bool IsAtEndPoint() const
	{
		return m_PosState == eSpotPosState_FirstPointOfPolygon || m_PosState == eSpotPosState_LastPointOfPolygon;
	}

	bool IsEquivalentPos(const Spot& spot) const
	{
		return Comparison::IsEquivalent(m_Pos, spot.m_Pos);
	}

	bool IsOnEdge() const
	{
		return IsAtEndPoint() || m_PosState == eSpotPosState_Edge || m_PosState == eSpotPosState_CenterOfEdge || m_PosState == eSpotPosState_EitherPointOfEdge;
	}

	bool IsCenterOfEdge() const
	{
		return m_PosState == eSpotPosState_CenterOfEdge;
	}

	bool IsAtEitherPointOnEdge() const
	{
		return IsAtEndPoint() || m_PosState == eSpotPosState_EitherPointOfEdge;
	}

	bool IsInPolygon() const
	{
		return m_PosState == eSpotPosState_InPolygon || m_PosState == eSpotPosState_CenterOfPolygon;
	}

	bool IsSamePos(const Spot& spot) const
	{
		return Comparison::IsEquivalent(m_Pos, spot.m_Pos);
	}

	void Reset()
	{
		m_PosState = eSpotPosState_InPolygon;
		m_pPolygon = NULL;
		m_bProcessed = false;
		m_Plane = BrushPlane(BrushVec3(0, 0, 0), 0);
	}

	ESpotPositionState m_PosState;
	bool               m_bProcessed;
	BrushVec3          m_Pos;
	BrushPlane         m_Plane;
	PolygonPtr         m_pPolygon;
};

struct SpotPair
{
	SpotPair(){}
	SpotPair(const Spot& spot0, const Spot& spot1)
	{
		m_Spot[0] = spot0;
		m_Spot[1] = spot1;
	}
	Spot m_Spot[2];
};

typedef std::vector<Spot>     SpotList;
typedef std::vector<SpotPair> SpotPairList;

struct CandidateInfo
{
	CandidateInfo(const BrushVec3& pos, ESpotPositionState spotPosState) : m_Pos(pos), m_SpotPosState(spotPosState){}
	BrushVec3          m_Pos;
	ESpotPositionState m_SpotPosState;
};

struct IntersectionInfo
{
	int       nPolygonIndex;
	BrushVec3 vIntersection;
};

class SpotManager
{
protected:
	SpotManager()
	{
		ResetAllSpots();
		m_bBuiltInSnap = false;
		m_BuiltInSnapSize = (BrushFloat)0.5;
		m_bEnableMagnetic = true;
	}
	~SpotManager(){}

	void DrawPolyline(DisplayContext& dc) const;
	void DrawCurrentSpot(DisplayContext& dc, const BrushMatrix34& worldTM) const;
	void ResetAllSpots()
	{
		m_SpotList.clear();
		m_CurrentSpot.Reset();
		m_StartSpot.Reset();
	}

	virtual void       CreatePolygonFromSpots(bool bClosedPolygon, const SpotList& spotList) { assert(0); };

	static void        GenerateVertexListFromSpotList(const SpotList& spotList, std::vector<BrushVec3>& outVList);

	void               RegisterSpotList(Model* pModel, const SpotList& spotList);

	const BrushVec3&   GetCurrentSpotPos() const                        { return m_CurrentSpot.m_Pos; }
	const BrushVec3&   GetStartSpotPos() const                          { return m_StartSpot.m_Pos; }

	void               SetCurrentSpotPos(const BrushVec3& vPos)         { m_CurrentSpot.m_Pos = vPos; }
	void               SetStartSpotPos(const BrushVec3& vPos)           { m_StartSpot.m_Pos = vPos; }

	void               SetCurrentSpotPosState(ESpotPositionState state) { m_CurrentSpot.m_PosState = state; }
	ESpotPositionState GetCurrentSpotPosState() const                   { return m_CurrentSpot.m_PosState; }
	void               ResetCurrentSpot()
	{
		m_CurrentSpot.Reset();
	}
	void ResetCurrentSpotWeakly()
	{
		m_CurrentSpot.m_PosState = eSpotPosState_InPolygon;
	}

	const Spot&      GetCurrentSpot() const                          { return m_CurrentSpot; }
	const Spot&      GetStartSpot() const                            { return m_StartSpot; }

	void             SetCurrentSpot(const Spot& spot)                { m_CurrentSpot = spot; }
	void             SetStartSpot(const Spot& spot)                  { m_StartSpot = spot; }
	void             SetSpotProcessed(int nPos, bool bProcessed)     { m_SpotList[nPos].m_bProcessed = bProcessed; }
	void             SetPosState(int nPos, ESpotPositionState state) { m_SpotList[nPos].m_PosState = state; }

	void             SwapCurrentAndStartSpots()                      { std::swap(m_CurrentSpot, m_StartSpot); }

	int              GetSpotListCount() const                        { return m_SpotList.size(); }
	const BrushVec3& GetSpotPos(int nPos) const                      { return m_SpotList[nPos].m_Pos; }
	void             AddSpotToSpotList(const Spot& spot);
	void             ReplaceSpotList(const SpotList& spotList);

	void             ClearSpotList()           { m_SpotList.clear(); }

	const Spot&     GetSpot(int nIndex) const { return m_SpotList[nIndex]; }
	const SpotList& GetSpotList() const       { return m_SpotList; }

	void            SplitSpotList(Model* pModel, const SpotList& spotList, std::vector<SpotList>& outSpotLists);
	void            SplitSpot(Model* pModel, const SpotPair& spotPair, SpotPairList& outSpotPairs);

	bool            UpdateCurrentSpotPosition(Model* pModel, const BrushMatrix34& worldTM, const BrushPlane& plane, IDisplayViewport* view, CPoint point, bool bKeepInitialPlane, bool bSearchAllShelves = false);

	bool            GetPlaneBeginEndPoints(const BrushPlane& plane, BrushVec2& outProjectedStartPt, BrushVec2& outProjectedEndPt) const;
	bool            GetPosAndPlaneFromWorld(IDisplayViewport* view, const CPoint& point, const BrushMatrix34& worldTM, BrushVec3& outPos, BrushPlane& outPlane);

	bool            IsSnapEnabled() const;
	BrushVec3       Snap(const BrushVec3& vPos) const;

	void            EnableBuiltInSnap(bool bEnabled)         { m_bBuiltInSnap = bEnabled; }
	void            SetBuiltInSnapSize(BrushFloat fSnapSize) { m_BuiltInSnapSize = fSnapSize; }

	void            EnableMagnetic(bool bEnabled)            { m_bEnableMagnetic = bEnabled; }

private:

	bool FindBestPlane(Model* pModel, const Spot& s0, const Spot& s1, BrushPlane& outPlane);
	bool AddPolygonToDesignerFromSpotList(Model* pModel, const SpotList& spotList);
	bool FindSnappedSpot(const BrushMatrix34& worldTM, PolygonPtr pPickedPolygon, const BrushVec3& pickedPos, Spot& outSpot) const;
	bool FindNicestSpot(IDisplayViewport* pViewport, const std::vector<CandidateInfo>& candidates, const Model* pModel, const BrushMatrix34& worldTM, const BrushVec3& pickedPos, PolygonPtr pPickedPolygon, const BrushPlane& plane, Spot& outSpot) const;
	bool FindSpotNearAxisAlignedLine(IDisplayViewport* pViewport, PolygonPtr pPolygon, const BrushMatrix34& worldTM, Spot& outSpot);
	bool IsVertexOnEdgeInModel(Model* pModel, const BrushPlane& plane, const BrushVec3& vertex, PolygonPtr pExcludedPolygon = NULL) const;

	Spot       m_CurrentSpot;
	Spot       m_StartSpot;
	SpotList   m_SpotList;

	bool       m_bEnableMagnetic;
	bool       m_bBuiltInSnap;
	BrushFloat m_BuiltInSnapSize;
};
}

