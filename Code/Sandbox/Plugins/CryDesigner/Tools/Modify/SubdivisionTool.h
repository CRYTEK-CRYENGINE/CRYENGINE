// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"

namespace Designer
{
class SubdivisionTool : public BaseTool
{
public:

	SubdivisionTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	static void Subdivide(int nLevel, bool bUpdateBrush);

	void Enter() override;
	void Leave() override;

	void HighlightEdgeGroup(const char* edgeGroupName);
	void AddNewEdgeTag();
	void DeleteEdgeTag(const char* name);
	void InvalideSelectedEdges() { m_SelectedEdgesAsEnter.clear(); }

	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override { return true; }
	void ApplySubdividedMesh();
	void DeleteAllUnused();

	void Display(DisplayContext& dc) override;

private:

	std::vector<BrushEdge3D> m_HighlightedSharpEdges;
	std::vector<BrushEdge3D> m_SelectedEdgesAsEnter;
};
}

