// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class LoopCutTool : public BaseTool
{
public:

	LoopCutTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void Enter() override;

	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseWheel(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	void Display(DisplayContext& dc) override;
	void Serialize(Serialization::IArchive& ar);

private:

	void UpdateLoops(PolygonPtr pPolygon, const BrushVec3& posOnSurface);
	void UpdateLoops();
	void FreezeLoops();

	enum ELoopCutMode
	{
		eLoopCutMode_Divide,
		eLoopCutMode_Slide,
	};

	struct LoopSection
	{
		LoopSection(PolygonPtr _pPolygon = NULL) : pPolygon(_pPolygon) {}
		std::vector<BrushEdge3D> edges;
		PolygonPtr               pPolygon;
	};

	PolygonPtr               m_pSelectedPolygon;
	std::vector<LoopSection> m_LoopPolygons;
	BrushVec3                m_vPosOnSurface;
	int                      m_SegmentNum;
	BrushFloat               m_SlideOffset;
	BrushFloat               m_NormalizedSlideOffset;
	BrushVec3                m_LoopCutDir;
	ELoopCutMode             m_LoopCutMode;

};
}

