// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"
#include "Util/ArgumentModel.h"

namespace Designer
{
enum EExtrudePlaneAxis
{
	eExtrudePlaneAxis_Average,
	eExtrudePlaneAxis_Individual,
	eExtrudePlaneAxis_X,
	eExtrudePlaneAxis_Y,
	eExtrudePlaneAxis_Z
};

struct ExtrudeMultipleParams
{
	ExtrudeMultipleParams() :
		bLeaveEdgeLoop(false),
		axis(eExtrudePlaneAxis_Average)
	{
	}
	bool              bLeaveEdgeLoop;
	EExtrudePlaneAxis axis;
};

struct ExtrudeMultipleContext
{
	ExtrudeMultipleContext()
	{
		Invalidate();
	}

	void Invalidate()
	{
		pModel = NULL;
		pObject = NULL;
		pCompiler = NULL;
		height = 0;
		bIndividual = false;
		bLeaveLoopEdge = false;
		dir = BrushVec3(0, 0, 1);
		argumentModels.clear();
		affectedPolygons.clear();
	}

	Model*                        pModel;
	CBaseObject*                  pObject;
	ModelCompiler*                pCompiler;
	float                         height;
	bool                          bIndividual;
	bool                          bLeaveLoopEdge;
	BrushVec3                     dir;
	std::vector<ArgumentModelPtr> argumentModels;
	std::vector<PolygonPtr>       affectedPolygons;
};

class ExtrudeMultipleTool : public SelectTool
{
public:
	ExtrudeMultipleTool(EDesignerTool tool) : SelectTool(tool)
	{
		m_nPickFlag = ePF_Polygon;
		m_bAllowSelectionUndo = false;
	}
	void Enter() override;
	void Leave() override;

	void Serialize(Serialization::IArchive& ar);

	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	void Display(DisplayContext& dc) override;
	bool IsManipulatorVisible() { return false; }

private:
	void                     Initialize();
	void                     StartExtrusion();
	void                     UpdateSelectedElements();

	void                     UpdateModel(CViewport* view);
	static void              UpdateModel(ExtrudeMultipleContext& emc);

	std::vector<BrushEdge3D> FindSharedEdges(const std::vector<PolygonPtr>& polygons);
	bool                     HasEdge(const std::vector<BrushEdge3D>& edges, const BrushEdge3D& e) const;

	bool                   m_bStartedManipluation;
	ExtrudeMultipleParams  m_Params;
	ExtrudeMultipleContext m_Context;
};
}

