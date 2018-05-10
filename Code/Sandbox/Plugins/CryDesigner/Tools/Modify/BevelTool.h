// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"

namespace Designer
{
class BevelTool : public SelectTool
{
public:
	BevelTool(EDesignerTool tool) :
		SelectTool(tool),
		m_nMousePrevY(0),
		m_fDelta(0)
	{
		m_bAllowSelectionUndo = false;
	}

	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnKeyDown(CViewport* view, uint32 nKeycode, uint32 nRepCnt, uint32 nFlags) override;
	void Display(DisplayContext& dc) override;

	void Enter() override;
	void Leave() override;

private:
	typedef std::vector<std::pair<BrushVec3, BrushVec3>> MapBewteenSpreadedVertexAndApex;
	typedef std::map<int, std::vector<BrushEdge3D>>      MapBetweenElementIndexAndEdges;
	typedef std::map<int, PolygonPtr>                    MapBetweenElementIndexAndOrignialPolygon;
	struct MappingInfo
	{
		void Reset()
		{
			mapSpreadedVertex2Apex.clear();
			mapElementIdx2Edges.clear();
			mapElementIdx2OriginalPolygon.clear();
			vertexSetToMakePolygon.clear();
		}
		MapBewteenSpreadedVertexAndApex                 mapSpreadedVertex2Apex;
		MapBetweenElementIndexAndEdges                  mapElementIdx2Edges;
		MapBetweenElementIndexAndOrignialPolygon        mapElementIdx2OriginalPolygon;
		std::set<BrushVec3, Comparison::less_BrushVec3> vertexSetToMakePolygon;
	};

	// First - Polygon, Second - EdgeIndex
	typedef std::pair<PolygonPtr, int> EdgeIdentifier;
	struct ResultForNextPhase
	{
		void Reset()
		{
			mapBetweenEdgeIdToApex.clear();
			mapBetweenEdgeIdToVertex.clear();
			middlePhaseEdgePolygons.clear();
			middlePhaseSidePolygons.clear();
			middlePhaseBottomPolygons.clear();
			middlePhaseApexPolygons.clear();
		}
		std::map<EdgeIdentifier, BrushVec3> mapBetweenEdgeIdToApex;
		std::map<EdgeIdentifier, BrushVec3> mapBetweenEdgeIdToVertex;
		std::vector<PolygonPtr>             middlePhaseEdgePolygons;
		std::vector<PolygonPtr>             middlePhaseSidePolygons;
		std::vector<PolygonPtr>             middlePhaseBottomPolygons;
		std::vector<PolygonPtr>             middlePhaseApexPolygons;
	};
	ResultForNextPhase m_ResultForSecondPhase;

	bool PP0_Initialize(bool bSpreadEdge = false);

	void PP0_SpreadEdges(int offset, bool bSpreadEdge = true);
	bool PP1_PushEdgesAndVerticesOut(ResultForNextPhase& outResultForNextPhase, MappingInfo& outMappingInfo);
	void PP1_MakeEdgePolygons(const MappingInfo& mappingInfo, ResultForNextPhase& outResultForNextPhase);
	void PP2_MapBetweenEdgeIdToApexPos(
	  const MappingInfo& mappingInfo,
	  PolygonPtr pEdgePolygon,
	  const BrushEdge3D& sideEdge0,
	  const BrushEdge3D& sideEdge1,
	  ResultForNextPhase& outResultForNextPhase);
	void PP1_MakeApexPolygons(const MappingInfo& mappingInfo, ResultForNextPhase& outResultForNextPhase);
	void PP0_SubdivideSpreadedEdge(int nSubdivideNum);

	struct InfoForSubdivingApexPolygon
	{
		BrushEdge3D edge;
		std::vector<std::pair<BrushVec3, BrushVec3>> vIntermediate;
	};
	void PP1_SubdivideApexPolygon(int nSubdivideNum, const std::vector<InfoForSubdivingApexPolygon>& infoForSubdividingApexPolygonList);

private:
	int                     GetEdgeCountHavingVertexInElementList(const BrushVec3& vertex, const ElementSet& elementList) const;
	int                     FindCorrespondingEdge(const BrushEdge3D& e, const std::vector<InfoForSubdivingApexPolygon>& infoForSubdividingApexPolygonList) const;

	std::vector<PolygonPtr> CreateFirstOddSubdividedApexPolygons(const std::vector<const InfoForSubdivingApexPolygon*>& subdividedEdges);
	std::vector<PolygonPtr> CreateFirstEvenSubdividedApexPolygons(const std::vector<const InfoForSubdivingApexPolygon*>& subdividedEdges);

	enum EBevelMode
	{
		eBevelMode_Nothing,
		eBevelMode_Spread,
		eBevelMode_Divide,
		eBevelMode_Done,
	};
	EBevelMode              m_BevelMode;

	_smart_ptr<Model>       m_pOriginalModel;
	ElementSet              m_OriginalSelectedElements;

	std::vector<PolygonPtr> m_OriginalPolygons;

	int                     m_nMousePrevY;
	BrushFloat              m_fDelta;
	int                     m_nDividedNumber;

};
}

