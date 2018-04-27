// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{

class SliceTool : public BaseTool
{
public:
	SliceTool(EDesignerTool tool) :
		BaseTool(tool),
		m_PrevGizmoPos(BrushVec3(0, 0, 0)),
		m_GizmoPos(BrushVec3(0, 0, 0)),
		m_CursorPos(BrushVec3(0, 0, 0))
	{
	}

	virtual void Display(DisplayContext& dc) override;

	void         Enter() override;

	void         CenterPivot();

	void         SliceFrontPart();
	void         SliceBackPart();
	void         Clip();
	void         Divide();
	void         AlignSlicePlane(const BrushVec3& normalDir);
	void         InvertSlicePlane();

	void         Serialize(Serialization::IArchive& ar);

	void         OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int nFlags) override;
	void         OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags) override;
	bool         IsManipulatorVisible() override { return true; }

protected:
	struct ETraverseLineInfo
	{
		ETraverseLineInfo(PolygonPtr pPolygon, const BrushEdge3D& edge, const BrushPlane& slicePlane) :
			m_pPolygon(pPolygon),
			m_Edge(edge),
			m_SlicePlane(slicePlane)
		{
		}
		PolygonPtr  m_pPolygon;
		BrushEdge3D m_Edge;
		BrushPlane  m_SlicePlane;
	};

	typedef std::vector<ETraverseLineInfo> TraverseLineList;
	typedef std::vector<TraverseLineList>  TraverseLineLists;

	void         UpdateSlicePlane();
	virtual bool UpdateManipulatorInMirrorMode(const BrushMatrix34& offsetTM) { return false; }

	void         GenerateLoop(const BrushPlane& slicePlane, TraverseLineList& outLineList) const;
	BrushVec3    GetLoopPivotPoint() const;

	void         DrawOutlines(DisplayContext& dc);
	void         DrawOutline(DisplayContext& dc, TraverseLineList& lineList);
	virtual void UpdateGizmo();

	TraverseLineList  m_MainTraverseLines;
	TraverseLineLists m_RestTraverseLineSet;
	BrushVec3         m_PrevGizmoPos;
	BrushVec3         m_GizmoPos;
	BrushPlane        m_SlicePlane;
	BrushVec3         m_CursorPos;
};
}

