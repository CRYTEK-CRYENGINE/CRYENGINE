// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
enum EPivotSelectionType
{
	ePST_BoundBox,
	ePST_Mesh,
};

class PivotTool : public BaseTool
{
public:
	PivotTool(EDesignerTool tool) : BaseTool(tool), m_PivotSelectionType(ePST_BoundBox)
	{
	}

	void Enter() override;
	void Leave() override;

	void Display(DisplayContext& dc) override;

	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;

	void SetSelectionType(EPivotSelectionType selectionType, bool bForce = false);

	void OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int flags) override;
	void OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags) override;

	void Serialize(Serialization::IArchive& ar);

private:

	void Confirm();
	void InitializeManipulator();

	std::vector<BrushVec3> m_CandidateVertices;
	int                    m_nSelectedCandidate;
	int                    m_nPivotIndex;
	BrushVec3              m_PivotPos;
	BrushVec3              m_StartingDragManipulatorPos;
	EPivotSelectionType    m_PivotSelectionType;
};
}

