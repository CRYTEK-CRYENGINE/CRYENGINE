// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"
#include "Tools/Select/SelectTool.h"
#include "Core/ModelDB.h"

namespace Designer
{
class MovePipeline
{
public:

	MovePipeline(){}
	~MovePipeline(){}

	void           TransformSelections(MainContext& mc, const BrushMatrix34& offsetTM);
	void           SetQueryResultsFromSelectedElements(const ElementSet& selectedElements);
	bool           ExcutedAdditionPass() const           { return m_bExecutedAdditionPass; }
	void           SetExcutedAdditionPass(bool bExcuted) { m_bExecutedAdditionPass = bExcuted; }
	void           CreateOrganizedResultsAroundPolygonFromQueryResults();
	void           ComputeIntermediatePositionsBasedOnInitQueryResults(const BrushMatrix34& offsetTM);
	ModelDB::Mark& GetMark(const SelectTool::QueryInput& qInput) { return m_QueryResult[qInput.first].m_MarkList[qInput.second]; }
	bool           VertexAdditionFirstPass();
	bool           VertexAdditionSecondPass();
	bool           SubdivisionPass();
	void           TransformationPass();
	bool           MergeCoplanarPass();
	PolygonPtr     FindAdjacentPolygon(PolygonPtr pPolygon, const BrushVec3& vPos, int& outAdjacentPolygonIndex);
	void           Initialize(const ElementSet& elements);
	void           InitializeIsolated(ElementSet& elements);
	void           End();
	bool           GetAveragePos(BrushVec3& outAveragePos) const;
	void           SetModel(Model* pModel) { m_pModel = pModel; }

private:

	void   SnappedToMirrorPlane();
	Model* GetModel() { return m_pModel; }
	void   InitPolygonsInUVIslands();

	Model*                            m_pModel;
	std::set<PolygonPtr>              m_UnsubdividedPolygons;
	std::map<PolygonPtr, PolygonList> m_SubdividedPolygons;
	ModelDB::QueryResult              m_QueryResult;
	ModelDB::QueryResult              m_InitQueryResult;
	ElementSet                        m_InitSelection;
	std::vector<BrushVec3>            m_IntermediateTransQueryPos;
	SelectTool::OrganizedQueryResults m_OrganizedQueryResult;
	bool                              m_bExecutedAdditionPass;
	std::set<PolygonPtr>              m_PolygonsInUVIslands;
};
}

