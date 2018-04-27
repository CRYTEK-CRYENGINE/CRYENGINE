// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Util/ArgumentModel.h"
#include "Core/Model.h"

namespace Designer
{
enum EExtrudePhase
{
	eExtrudePhase_Select,
	eExtrudePhase_Resize,
	eExtrudePhase_Extrude
};

struct ExtrusionContext : public MainContext
{
	ExtrusionContext() :
		bFirstUpdate(false),
		bTouchedMirrorPlane(false),
		bUpdateBrush(true),
		bLeaveLoopEdges(true),
		bResetShelf(true),
		bUpdateSelectionMesh(true),
		pPolygon(NULL),
		pArgumentModel(NULL),
		pushPull(ePP_None),
		initPushPull(ePP_None),
		fScale(0)
	{
	}

	bool                    bUpdateBrush;
	bool                    bUpdateSelectionMesh;
	bool                    bFirstUpdate;
	bool                    bTouchedMirrorPlane;
	bool                    bIsLocatedAtOpposite;
	bool                    bLeaveLoopEdges;
	bool                    bResetShelf;
	PolygonPtr              pPolygon;
	EPushPull               pushPull;
	EPushPull               initPushPull;
	BrushFloat              fScale;
	ArgumentModelPtr        pArgumentModel;
	std::vector<PolygonPtr> backupPolygons;
	std::vector<PolygonPtr> mirroredBackupPolygons;
};

struct ExtrudeActionInfo
{
	ExtrudeActionInfo() : m_Type(ePP_None), m_Distance(0) {}
	EPushPull  m_Type;
	BrushFloat m_Distance;
	float      m_Scale;
};

struct ExtrudeParam
{
	ExtrudeParam() :
		bScalePolygonPhase(false),
		bLeaveLoopEdges(true),
		bAlignment(true)
	{
	}
	bool bScalePolygonPhase;
	bool bLeaveLoopEdges;
	bool bAlignment;
};

class ExtrudeTool : public BaseTool
{
public:
	ExtrudeTool(EDesignerTool tool) :
		BaseTool(tool),
		m_Phase(eExtrudePhase_Select)
	{
	}
	void        Enter() override;

	bool        OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool        OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;
	bool        OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;

	void        Display(DisplayContext& dc) override;
	void        OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void        Serialize(Serialization::IArchive& ar);

	bool        IsManipulatorVisible() { return false; }


	static void Extrude(MainContext& mc, PolygonPtr pPolygon, float fHeight, float fScale);
	static void UpdateModel(ExtrusionContext& ec);

private:
	static bool PrepareExtrusion(ExtrusionContext& ec);
	static void MakeArgumentBrush(ExtrusionContext& ec);
	static void FinishPushPull(ExtrusionContext& ec);
	static bool CheckBoundary(ExtrusionContext& ec);

private:
	void RaiseHeight(const CPoint& point, CViewport* view, int nFlags);
	bool AlignHeight(PolygonPtr pCapPolygon, CViewport* view, const CPoint& point);
	bool StartPushPull(CViewport* view, UINT nFlags, CPoint point);
	void RaiseLowerPolygon(CViewport* view, UINT nFlags, CPoint point);
	void ResizePolygon(CViewport* view, UINT nFlags, CPoint point);
	void SelectPolygon(CViewport* view, UINT nFlags, CPoint point);
	void RepeatPrevAction(CViewport* view, UINT nFlags, CPoint point);

private:
	ExtrusionContext  m_ec;
	ExtrudeActionInfo m_PrevAction;
	BrushVec2         m_ResizeStartScreenPos;
	EExtrudePhase     m_Phase;
	ExtrudeParam      m_Params;
	PolygonPtr        m_pSelectedPolygon;
};
}

